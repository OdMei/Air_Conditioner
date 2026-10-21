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

#define AP3216C_I2C_TIMEOUT_MS (20U)

/**
 * @brief   初始化AP3216C
 * @param   无
 * @retval  初始化结果
 * @arg     0: 初始化成功
 * @arg     1: 初始化失败
 */
uint8_t ap3216c_init(void)
{
    uint8_t data[1] = {0x04U};

    if (HAL_I2C_Mem_Write(&hi2c4, AP3216C_ADDR, 0x00U, I2C_MEMADD_SIZE_8BIT,
                           data, 1U, AP3216C_I2C_TIMEOUT_MS) != HAL_OK)
    {
        return 1U;
    }
    HAL_Delay(10U);
    data[0] = 0x03U;
    if (HAL_I2C_Mem_Write(&hi2c4, AP3216C_ADDR, 0x00U, I2C_MEMADD_SIZE_8BIT,
                           data, 1U, AP3216C_I2C_TIMEOUT_MS) != HAL_OK)
    {
        return 1U;
    }
    data[0] = 0U;
    if (HAL_I2C_Mem_Read(&hi2c4, AP3216C_ADDR, 0x00U, I2C_MEMADD_SIZE_8BIT,
                          data, 1U, AP3216C_I2C_TIMEOUT_MS) != HAL_OK)
    {
        return 1U;
    }
    return (data[0] == 0x03U) ? 0U : 1U;
}

/**
 * @brief   读AP3216C数据
 * @param   ir: IR传感器数据
 * @param   ps: PS传感器数据
 * @param   als: ALS传感器数据
 * @retval  无
 */
uint8_t ap3216c_read_checked(uint16_t *ir, uint16_t *ps, uint16_t *als)
{
    uint8_t data[6] = {0};
    uint8_t index;
    uint32_t started;
    uint32_t elapsed;

    if ((ir == NULL) || (ps == NULL) || (als == NULL))
    {
        return 1U;
    }
    /* Keep the documented single-register protocol. All six reads share one
     * software deadline, rather than restarting a long timeout per byte.
     * The vendor HAL has an additional fixed 25 ms BUSY wait on a stuck bus. */
    started = HAL_GetTick();
    for (index = 0U; index < sizeof(data); index++)
    {
        elapsed = HAL_GetTick() - started;
        if (elapsed >= AP3216C_I2C_TIMEOUT_MS)
        {
            return 1U;
        }
        if (HAL_I2C_Mem_Read(&hi2c4, AP3216C_ADDR, 0x0AU + index,
                              I2C_MEMADD_SIZE_8BIT, &data[index], 1U,
                              AP3216C_I2C_TIMEOUT_MS - elapsed) != HAL_OK)
        {
            return 1U;
        }
    }
    if ((HAL_GetTick() - started) > AP3216C_I2C_TIMEOUT_MS)
    {
        return 1U;
    }
    *ir = (data[0] & 0x80U) ? 0U : (((uint16_t)data[1] << 2U) | (data[0] & 0x03U));
    *als = ((uint16_t)data[3] << 8U) | data[2];
    *ps = (data[4] & 0x40U) ? 0U :
          (((uint16_t)(data[5] & 0x3FU) << 4U) | (data[4] & 0x0FU));
    return 0U;
}

void ap3216c_read_data(uint16_t *ir, uint16_t *ps, uint16_t *als)
{
    (void)ap3216c_read_checked(ir, ps, als);
}
