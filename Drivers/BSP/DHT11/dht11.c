/**
 ****************************************************************************************************
 * @file        dht11.c
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2025-01-13
 * @brief       DHT11驱动代码
 * @license     Copyright (c) 2020-2032, 广州市星翼电子科技有限公司
 ****************************************************************************************************
 * @attention
 * 
 * 实验平台:正点原子 N647开发板
 * 在线视频:www.yuanzige.com
 * 技术论坛:www.openedv.com
 * 公司网址:www.alientek.com
 * 购买地址:openedv.taobao.com
 * 
 ****************************************************************************************************
 */

#include "dht11.h"
#include "../SYS/sys.h"

static dht11_status_t dht11_capture(uint8_t *data);
static dht11_status_t dht11_wait_response(void);
static dht11_status_t dht11_read_byte(uint8_t *value);
static float to_decimal(uint8_t pos_integer);
static dht11_diag_t s_diag;

/* N6 uses four implemented priority bits. Preserve camera/CSI (7), TIM1
   IR (0), and ThreadX SysTick (4); defer PendSV (15) during bit capture. */
#define DHT11_CAPTURE_BASEPRI (8U << (8U - __NVIC_PRIO_BITS))

static uint8_t dht11_bad_context(void)
{
    return (__get_IPSR() != 0U) || (__get_PRIMASK() != 0U) ||
           (__get_BASEPRI() != 0U);
}

static uint8_t dht11_record_result(dht11_status_t status, uint32_t attempt_ms)
{
    uint32_t completed_ms = HAL_GetTick();
    uint32_t irq_state = __get_PRIMASK();
    __disable_irq();
    s_diag.attempts++;
    s_diag.last_attempt_ms = attempt_ms;
    s_diag.last_status = status;
    if (status == DHT11_STATUS_OK)
    {
        s_diag.successes++;
        s_diag.consecutive_failures = 0U;
        s_diag.last_success_ms = completed_ms;
        s_diag.has_sample = 1U;
    }
    else
    {
        s_diag.failures++;
        if (s_diag.consecutive_failures != UINT32_MAX)
        {
            s_diag.consecutive_failures++;
        }
    }
    __set_PRIMASK(irq_state);
    return status == DHT11_STATUS_OK ? 0U : 1U;
}

void dht11_get_diag(dht11_diag_t *diag)
{
    if (diag != NULL)
    {
        uint32_t irq_state = __get_PRIMASK();
        __disable_irq();
        *diag = s_diag;
        __set_PRIMASK(irq_state);
    }
}

/**
 * @brief   初始化DHT11
 * @param   无
 * @retval  初始化结果
 * @arg     0: 初始化成功
 * @arg     1: 初始化失败
 */
uint8_t dht11_init(void)
{
    /* HAL_Delay needs the tick interrupt. Never start from an ISR or a
       caller that has already disabled interrupts. A probe is not a sample. */
    if (dht11_bad_context())
    {
        return 1U;
    }
    return dht11_capture(NULL) == DHT11_STATUS_OK ? 0U : 1U;
}

/**
 * @brief   获取DHT11的温湿度值
 * @param   temperature: 温度值
 * @param   humidity: 湿度值
 * @retval  获取结果
 * @arg     0: 获取成功
 * @arg     1: 获取失败
 */
uint8_t dht11_get_data(float *temperature, float *humidity)
{
    uint8_t data[5];
    uint8_t sun = 0;
    uint8_t i;
    float temperature_sign = 1.0f;
    uint32_t attempt_ms = HAL_GetTick();
    dht11_status_t status;

    if ((temperature == NULL) || (humidity == NULL))
    {
        return dht11_record_result(DHT11_STATUS_BAD_ARGUMENT, attempt_ms);
    }
    if (dht11_bad_context())
    {
        return dht11_record_result(DHT11_STATUS_BAD_CONTEXT, attempt_ms);
    }
    status = dht11_capture(data);
    if (status != DHT11_STATUS_OK)
    {
        return dht11_record_result(status, attempt_ms);
    }

    for (i = 0; i < 4; i++)
    {
        sun += data[i];
    }

    if (sun != data[4])
    {
        return dht11_record_result(DHT11_STATUS_CHECKSUM, attempt_ms);
    }

    *humidity = (float)data[0] + to_decimal(data[1]);
    if ((data[3] & 0x80) == 0x80)
    {
        data[3] &= ~0x80;
        temperature_sign = -1.0f;
    }
    *temperature = temperature_sign * ((float)data[2] + to_decimal(data[3]));

    return dht11_record_result(DHT11_STATUS_OK, attempt_ms);
}

/* The 18 ms start pulse is interruptible. Only the timing-sensitive reply
   and 40 data bits defer thread switching (about 3-5 ms for a healthy
   sensor). Higher-priority camera/CSI and IR IRQs continue to run. A long
   high-priority IRQ may still spoil a frame; checksum failure keeps the
   caller's last values. Every capture exit restores the original BASEPRI. */
static dht11_status_t dht11_capture(uint8_t *data)
{
    uint32_t basepri_state;
    dht11_status_t status;

    DHT11_DQ_OUT(0);
    HAL_Delay(18U);
    basepri_state = __get_BASEPRI();
    __set_BASEPRI(DHT11_CAPTURE_BASEPRI);
    __DSB();
    __ISB();
    DHT11_DQ_OUT(1);
    sys_delay_us(35U);
    status = dht11_wait_response();
    if ((status == DHT11_STATUS_OK) && (data != NULL))
    {
        for (uint8_t i = 0U; i < 5U; i++)
        {
            status = dht11_read_byte(&data[i]);
            if (status != DHT11_STATUS_OK)
            {
                break;
            }
        }
    }
    __set_BASEPRI(basepri_state);
    __DSB();
    __ISB();
    return status;
}

/**
 * @brief   复位DHT11
 * @param   无
 * @retval  DHT11状态
 * @arg     0: DHT11存在
 * @arg     1: DHT11不存在
 */
static dht11_status_t dht11_wait_response(void)
{
    uint8_t count;

    count = 0;
    while ((DHT11_DQ_IN() != 0) && (count++ < 88))
    {
        sys_delay_us(1);
    }

    if (count >= 88)
    {
        return DHT11_STATUS_NO_RESPONSE;
    }

    count = 0;
    while ((DHT11_DQ_IN() == 0) && (count++ < 92))
    {
        sys_delay_us(1);
    }

    if (count >= 92)
    {
        return DHT11_STATUS_RESPONSE_TIMEOUT;
    }

    return DHT11_STATUS_OK;
}

/**
 * @brief   从DHT11读一字节数据
 * @param   无
 * @retval  一字节数据
 */
static dht11_status_t dht11_read_byte(uint8_t *value)
{
    uint8_t i;
    uint8_t data = 0;
    uint8_t count;

    for (i = 0; i < 8; i++)
    {
        count = 0;
        while ((DHT11_DQ_IN() != 0) && (count++ < 87))
        {
            sys_delay_us(1);
        }

        if (count >= 87)
        {
            return DHT11_STATUS_BIT_HIGH_TIMEOUT;
        }

        count = 0;
        while ((DHT11_DQ_IN() == 0) && (count++ < 58))
        {
            sys_delay_us(1);
        }

        if (count >= 58)
        {
            return DHT11_STATUS_BIT_LOW_TIMEOUT;
        }

        /* A zero pulse is about 26-28 us, a one about 70 us. Sample in
           between; 68 us sits at the falling edge of a valid one pulse. */
        sys_delay_us(40U);

        data |= DHT11_DQ_IN() << (7 - i);
    }

    *value = data;
    return DHT11_STATUS_OK;
}

/**
 * @brief   将正整数转为小数
 * @param   pos_integer: 整数
 * @retval  小数
 */
static float to_decimal(uint8_t pos_integer)
{
    uint8_t digits = 0;
    uint8_t temp_integer = pos_integer;
    uint8_t i;
    float divisor = 1.0f;

    if (pos_integer == 0)
    {
        return 0.0f;
    }

    while (temp_integer != 0)
    {
        temp_integer /= 10;
        digits++;
    }

    for (i = 0; i < digits; i++)
    {
        divisor *= 10.0f;
    }

    return (float)pos_integer / divisor;
}
