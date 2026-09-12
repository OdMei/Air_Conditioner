/**
 ****************************************************************************************************
 * @file        gtim.c
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2025-01-13
 * @brief       通用定时器驱动代码
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

#include "gtim.h"

extern TIM_HandleTypeDef htim3; /* TIM句柄 */

/**
 * @brief   初始化通用定时器中断
 * @param   arr: 自动重装载值
 * @param   psc: 预分频系数
 * @retval  无
 */
void gtim_timx_int_init(uint16_t arr, uint16_t psc)
{
    UNUSED(arr);
    UNUSED(psc);

    /* 开启TIM中断模式计数 */
    HAL_TIM_Base_Start_IT(&htim3);
}

/**
 * @brief   初始化通用定时器PWM输出
 * @param   arr: 自动重装载值
 * @param   psc: 预分频系数
 * @retval  无
 */
void gtim_timx_pwm_chy_init(uint16_t arr, uint16_t psc)
{
    UNUSED(arr);
    UNUSED(psc);

    /* 开启TIM PWM输出 */
    HAL_TIM_PWM_Start(&htim3, GTIM_TIMX_CH1);
}

/**
 * @brief   初始化通用定时器输入捕获
 * @param   arr: 自动重装载值
 * @param   psc: 预分频系数
 * @retval  无
 */
void gtim_timx_cap_chy_init(uint16_t arr, uint16_t psc)
{
    UNUSED(arr);
    UNUSED(psc);

    /* 使能TIM更新中断 */
    __HAL_TIM_ENABLE_IT(&htim3, TIM_IT_UPDATE);

    /* 开启TIM中断模式输入捕获 */
    HAL_TIM_IC_Start_IT(&htim3, GTIM_TIMX_CH1);
}

/**
 * @brief   初始化通用定时器脉冲计数
 * @param   psc: 预分频系数
 * @retval  无
 */
void gtim_timx_cnt_chy_init(uint16_t psc)
{
    UNUSED(psc);

    /* 使能TIM更新中断 */
    __HAL_TIM_ENABLE_IT(&htim3, TIM_IT_UPDATE);

    /* 开启TIM输入捕获 */
    HAL_TIM_IC_Start(&htim3, GTIM_TIMX_CH1);
}

/* TIM溢出计数器 */
uint32_t g_timx_chy_cnt_ofcnt = 0;

/**
 * @brief   重启通用定时器脉冲计数
 * @param   无
 * @retval  无
 */
void gtim_timx_cnt_chy_restart(void)
{
    __HAL_TIM_DISABLE(&htim3);
    g_timx_chy_cnt_ofcnt = 0;
    __HAL_TIM_SET_COUNTER(&htim3, 0);
    __HAL_TIM_ENABLE(&htim3);
}

/**
 * @brief   获取通用定时器脉冲计数值
 * @param   无
 * @retval  脉冲计数值
 */
uint32_t gtim_timx_cnt_chy_get_count(void)
{
    uint32_t total;

    /* 计算总脉冲计数值 */
    total = g_timx_chy_cnt_ofcnt * 0xFFFF;
    total += __HAL_TIM_GET_COUNTER(&htim3);

    return total;
}
