/**
 ****************************************************************************************************
 * @file        iwdg.c
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2025-01-13
 * @brief       IWDG驱动代码
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

#include "iwdg.h"

extern IWDG_HandleTypeDef hiwdg;    /* IWDG句柄 */

/**
 * @brief   初始化IWDG
 * @note    IWDG时钟源为lsi_ck
 * @param   prescaler: 预分频器系数
 * @arg     IWDG_PRESCALER_4   : 4分频
 * @arg     IWDG_PRESCALER_8   : 8分频
 * @arg     IWDG_PRESCALER_16  : 16分频
 * @arg     IWDG_PRESCALER_32  : 32分频
 * @arg     IWDG_PRESCALER_64  : 64分频
 * @arg     IWDG_PRESCALER_128 : 128分频
 * @arg     IWDG_PRESCALER_256 : 256分频
 * @arg     IWDG_PRESCALER_512 : 512分频
 * @arg     IWDG_PRESCALER_1024: 1024分频
 * @param   reload: 自动重装载值（0~0xFFF）
 * @retval  无
 */
void iwdg_init(uint32_t prescaler, uint32_t reload)
{
    UNUSED(prescaler);
    UNUSED(reload);
}

/**
 * @brief   喂狗IWDG
 * @param   无
 * @retval  无
 */
void iwdg_feed(void)
{
    HAL_IWDG_Refresh(&hiwdg);
}
