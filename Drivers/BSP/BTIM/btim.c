/**
 ****************************************************************************************************
 * @file        btim.c
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2025-01-13
 * @brief       基本定时器驱动代码
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

#include "btim.h"

extern TIM_HandleTypeDef htim6; /* TIM句柄 */

/**
 * @brief   初始化基本定时器中断
 * @param   arr: 自动重装载值
 * @param   psc: 预分频系数
 * @retval  无
 */
void btim_timx_int_init(uint16_t arr, uint16_t psc)
{
    UNUSED(arr);
    UNUSED(psc);

    /* 开启TIM中断模式计数 */
    HAL_TIM_Base_Start_IT(&htim6);
}
