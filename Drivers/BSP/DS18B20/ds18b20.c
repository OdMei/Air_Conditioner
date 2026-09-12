/**
 ****************************************************************************************************
 * @file        ds18b20.c
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2025-01-13
 * @brief       DS18B20驱动代码
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

#include "ds18b20.h"
#include "../SYS/sys.h"

static uint8_t ds18b20_reset(void);
static uint8_t ds18b20_read_byte(void);
static void ds18b20_write_byte(uint8_t data);

/**
 * @brief   初始化DS18B20
 * @param   无
 * @retval  初始化结果
 * @arg     0: 初始化成功
 * @arg     1: 初始化失败
 */
uint8_t ds18b20_init(void)
{
    return ds18b20_reset();
}

/**
 * @brief   获取DS18B20的温度值
 * @param   temperature: 温度值
 * @retval  获取结果
 * @arg     0: 获取成功
 * @arg     1: 获取失败
 */
uint8_t ds18b20_get_temperature(float *temperature)
{
    union {
        struct {
            uint8_t lsb;
            uint8_t msb;
        } byte;
        uint16_t halfword;
    } digital;
    float sign = 1.0f;
    
    if (ds18b20_reset() != 0)
    {
        return 1;
    }

    ds18b20_write_byte(0xCC);
    ds18b20_write_byte(0x44);

    if (ds18b20_reset() != 0)
    {
        return 1;
    }

    ds18b20_write_byte(0xCC);
    ds18b20_write_byte(0xBE);
    digital.byte.lsb = ds18b20_read_byte();
    digital.byte.msb = ds18b20_read_byte();

    if ((digital.byte.msb & 0xF8) == 0xF8)
    {
        digital.halfword = ~digital.halfword;
        digital.halfword += 1;
        sign = -1.0f;
    }

    *temperature = sign * digital.halfword * 0.0625f;

    return 0;
}

/**
 * @brief   复位DS18B20
 * @param   无
 * @retval  DS18B20状态
 * @arg     0: DS18B20存在
 * @arg     1: DS18B20不存在
 */
static uint8_t ds18b20_reset(void)
{
    uint8_t count;

    DS18B20_DQ_OUT(0);
    sys_delay_us(480);
    DS18B20_DQ_OUT(1);
    sys_delay_us(60);

    count = 0;
    while ((DS18B20_DQ_IN() != 0) && (count++ < 240))
    {
        sys_delay_us(1);
    }

    if (count >= 240)
    {
        return 1;
    }

    count = 0;
    while ((DS18B20_DQ_IN() == 0) && (count++ < 180))
    {
        sys_delay_us(1);
    }

    if (count >= 180)
    {
        return 1;
    }

    return 0;
}

/**
 * @brief   从DS18B20读一字节数据
 * @param   无
 * @retval  一字节数据
 */
static uint8_t ds18b20_read_byte(void)
{
    uint8_t i;
    uint8_t data = 0;

    for (i = 0; i < 8; i++)
    {
        DS18B20_DQ_OUT(0);
        sys_delay_us(1);
        DS18B20_DQ_OUT(1);
        sys_delay_us(10);
        data |= DS18B20_DQ_IN() << i;
        sys_delay_us(50);
    }

    return data;
}

/**
 * @brief   写一字节数据到DS18B20
 * @param   data: 一字节数据
 * @retval  无
 */
static void ds18b20_write_byte(uint8_t data)
{
    uint8_t i;

    for (i = 0; i < 8; i++)
    {
        if ((data & 0x01) == 0x01)
        {
            DS18B20_DQ_OUT(0);
            sys_delay_us(1);
            DS18B20_DQ_OUT(1);
            sys_delay_us(60);
        }
        else
        {
            DS18B20_DQ_OUT(0);
            sys_delay_us(60);
            DS18B20_DQ_OUT(1);
            sys_delay_us(1);
        }

        data >>= 1;
    }
}
