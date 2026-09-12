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

static uint8_t dht11_reset(void);
static uint8_t dht11_read_byte(void);
static float to_decimal(uint8_t pos_integer);

/**
 * @brief   初始化DHT11
 * @param   无
 * @retval  初始化结果
 * @arg     0: 初始化成功
 * @arg     1: 初始化失败
 */
uint8_t dht11_init(void)
{
    return dht11_reset();
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

    if (dht11_reset() != 0)
    {
        return 1;
    }

    for (i = 0; i < 5; i++)
    {
        data[i] = dht11_read_byte();
    }

    for (i = 0; i < 4; i++)
    {
        sun += data[i];
    }

    if (sun != data[4])
    {
        return 1;
    }

    *humidity = (float)data[0] + to_decimal(data[1]);
    if ((data[3] & 0x80) == 0x80)
    {
        data[3] &= ~0x80;
        temperature_sign = -1.0f;
    }
    *temperature = temperature_sign * (float)data[2] + to_decimal(data[3]);

    return 0;
}

/**
 * @brief   复位DHT11
 * @param   无
 * @retval  DHT11状态
 * @arg     0: DHT11存在
 * @arg     1: DHT11不存在
 */
static uint8_t dht11_reset(void)
{
    uint8_t count;

    DHT11_DQ_OUT(0);
    HAL_Delay(18);
    DHT11_DQ_OUT(1);
    sys_delay_us(35);

    count = 0;
    while ((DHT11_DQ_IN() != 0) && (count++ < 88))
    {
        sys_delay_us(1);
    }

    if (count >= 88)
    {
        return 1;
    }

    count = 0;
    while ((DHT11_DQ_IN() == 0) && (count++ < 92))
    {
        sys_delay_us(1);
    }

    if (count >= 92)
    {
        return 1;
    }

    return 0;
}

/**
 * @brief   从DHT11读一字节数据
 * @param   无
 * @retval  一字节数据
 */
static uint8_t dht11_read_byte(void)
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
            return 0x00;
        }

        count = 0;
        while ((DHT11_DQ_IN() == 0) && (count++ < 58))
        {
            sys_delay_us(1);
        }

        if (count >= 58)
        {
            return 0x00;
        }

        sys_delay_us(68);

        data |= DHT11_DQ_IN() << (7 - i);
    }

    return data;
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
