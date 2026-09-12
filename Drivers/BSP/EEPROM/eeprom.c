/**
 ****************************************************************************************************
 * @file        eeprom.c
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2025-01-13
 * @brief       EEPROM驱动代码
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

#include "eeprom.h"

extern I2C_HandleTypeDef hi2c4; /* I2C句柄 */

/**
 * @brief   初始化EEPROM
 * @param   无
 * @retval  无
 */
void eeprom_init(void)
{
    return;
}

/**
 * @brief   检查EEPROM
 * @param   无
 * @retval  检查结果
 * @arg     0: 检查成功
 * @arg     1: 检查失败
 */
uint8_t eeprom_check(void)
{
    return (HAL_I2C_IsDeviceReady(&hi2c4, EEPROM_ADDR, 1, 1000) != HAL_OK) ? 1 : 0;
}

/**
 * @brief   读EEPROM
 * @param   addr: 数据地址
 * @param   buf: 读缓冲区
 * @param   size: 数据大小
 * @retval  无
 */
void eeprom_read(uint8_t addr, uint8_t *buf, uint16_t size)
{
    HAL_I2C_Mem_Read(&hi2c4, EEPROM_ADDR, addr, I2C_MEMADD_SIZE_8BIT, buf, size, 1000);
}

/**
 * @brief   写EEPROM
 * @param   addr: 数据地址
 * @param   buf: 写缓冲区
 * @param   size: 数据大小
 * @retval  无
 */
void eeprom_write(uint8_t addr, uint8_t *buf, uint16_t size)
{
    uint16_t i;

    for (i = 0; i < size; i++)
    {
        while (HAL_I2C_IsDeviceReady(&hi2c4, EEPROM_ADDR, 1, 1000) != HAL_OK);

        if (HAL_I2C_Mem_Write(&hi2c4, EEPROM_ADDR, addr + i, I2C_MEMADD_SIZE_8BIT, &buf[i], 1, 1000) != HAL_OK)
        {
            break;
        }
    }
}
