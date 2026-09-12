/**
 ****************************************************************************************************
 * @file        ap3216c.c
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2025-01-13
 * @brief       AP3216C驱动代码
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

#include "ap3216c.h"

extern I2C_HandleTypeDef hi2c4; /* I2C句柄 */

/**
 * @brief   初始化AP3216C
 * @param   无
 * @retval  初始化结果
 * @arg     0: 初始化成功
 * @arg     1: 初始化失败
 */
uint8_t ap3216c_init(void)
{
    uint8_t data[1];

    data[0] = 0x04;
    HAL_I2C_Mem_Write(&hi2c4, AP3216C_ADDR, 0x00, I2C_MEMADD_SIZE_8BIT, data, 1, 1000);
    HAL_Delay(10);

    data[0] = 0x03;
    HAL_I2C_Mem_Write(&hi2c4, AP3216C_ADDR, 0x00, I2C_MEMADD_SIZE_8BIT, data, 1, 1000);

    data[0] = 0x00;
    HAL_I2C_Mem_Read(&hi2c4, AP3216C_ADDR, 0x00, I2C_MEMADD_SIZE_8BIT, data, 1, 1000);

    return ((data[0] == 0x03) ? 0 : 1);
}

/**
 * @brief   读AP3216C数据
 * @param   ir: IR传感器数据
 * @param   ps: PS传感器数据
 * @param   als: ALS传感器数据
 * @retval  无
 */
void ap3216c_read_data(uint16_t *ir, uint16_t *ps, uint16_t *als)
{
    uint8_t data[6];
    uint8_t i;

    for (i = 0; i < sizeof(data) / sizeof(data[0]); i++)
    {
        HAL_I2C_Mem_Read(&hi2c4, AP3216C_ADDR, 0x0A + i, I2C_MEMADD_SIZE_8BIT, &data[i], 1, 1000);
    }

    *ir = (data[0] & 0x80) ? 0 : (((uint16_t)data[1] << 2) | (data[0] & 0x03));
    *als = ((uint16_t)data[3] << 8) | data[2];
    *ps = (data[4] & 0x40) ? 0 : (((uint16_t)(data[5] & 0x3F) << 4) | (data[4] & 0x0F));
}
