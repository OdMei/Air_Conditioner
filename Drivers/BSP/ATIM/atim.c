/**
 ****************************************************************************************************
 * @file        atim.c
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2025-01-13
 * @brief       高级定时器驱动代码
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

#include "atim.h"

extern TIM_HandleTypeDef htim1; /* TIM句柄 */

/**
 * @brief   初始化高级定时器输出指定个数PWM
 * @param   arr: 自动重装载值
 * @param   psc: 预分频系数
 * @retval  无
 */
void atim_timx_chy_npwm_init(uint16_t arr, uint16_t psc)
{
    UNUSED(arr);
    UNUSED(psc);

    /* 使能TIM更新中断 */
    __HAL_TIM_ENABLE_IT(&htim1, TIM_IT_UPDATE);

    /* 开启TIM PWM输出 */
    HAL_TIM_PWM_Start(&htim1, ATIM_TIMX_CH1);
}

/* PWM剩余个数计数器 */
uint32_t g_timx_chy_npwm_remain = 0;

/**
 * @brief   设置高级定时器输出指定个数PWM
 * @param   npwm: 指定PWM个数
 * @retval  无
 */
void atim_timx_chy_npwm_set(uint32_t npwm)
{
    if (npwm == 0)
    {
        return;
    }

    g_timx_chy_npwm_remain = npwm;
    HAL_TIM_GenerateEvent(&htim1, TIM_EVENTSOURCE_UPDATE);
}

/**
 * @brief   初始化高级定时器输出比较
 * @param   arr: 自动重装载值
 * @param   psc: 预分频系数
 * @retval  无
 */
void atim_timx_comp_init(uint16_t arr, uint16_t psc)
{
    UNUSED(arr);
    UNUSED(psc);

    /* 开启TIM输出比较输出 */
    HAL_TIM_OC_Start(&htim1, ATIM_TIMX_CH1);
    HAL_TIM_OC_Start(&htim1, ATIM_TIMX_CH2);
}

/**
 * @brief   初始化高级定时器互补输出带死区控制
 * @param   arr: 自动重装载值
 * @param   psc: 预分频系数
 * @retval  无
 */
void atim_timx_cplm_init(uint16_t arr, uint16_t psc)
{
    UNUSED(arr);
    UNUSED(psc);

    /* 开启TIM PWM输出 */
    HAL_TIM_PWM_Start(&htim1, ATIM_TIMX_CH1);
    HAL_TIMEx_PWMN_Start(&htim1, ATIM_TIMX_CH1);
}

/**
 * @brief   设置高级定时器互补输出带死区控制
 * @param   ccr: 输出比较值
 * @param   dtg: 死区时间
 * @retval  无
 */
void atim_timx_cplm_set(uint16_t ccr, uint8_t dtg)
{
    TIM_BreakDeadTimeConfigTypeDef break_dead_time_config_struct = {
        .OffStateRunMode = TIM_OSSR_DISABLE,
        .OffStateIDLEMode = TIM_OSSI_DISABLE,
        .LockLevel = TIM_LOCKLEVEL_OFF,
        .DeadTime = 0,
        .BreakState = TIM_BREAK_ENABLE,
        .BreakPolarity = TIM_BREAKPOLARITY_HIGH,
        .BreakFilter = 0,
        .BreakAFMode = TIM_BREAK_AFMODE_INPUT,
        .Break2State = TIM_BREAK2_DISABLE,
        .Break2Polarity = TIM_BREAK2POLARITY_HIGH,
        .Break2Filter = 0,
        .Break2AFMode = TIM_BREAK_AFMODE_INPUT,
        .AutomaticOutput = TIM_AUTOMATICOUTPUT_ENABLE,
    };

    break_dead_time_config_struct.DeadTime = dtg;                           /* 死区时间 */
    HAL_TIMEx_ConfigBreakDeadTime(&htim1, &break_dead_time_config_struct);  /* 配置死区时间参数 */
    __HAL_TIM_MOE_ENABLE(&htim1);                                           /* 使能主输出 */
    __HAL_TIM_SET_COMPARE(&htim1, ATIM_TIMX_CH1, ccr);                      /* 设置比较寄存器 */
}

/**
 * @brief   初始化高级定时器PWM输入
 * @param   psc: 预分频系数
 * @retval  无
 */
void atim_timx_pwmin_chy_init(uint16_t psc)
{
    UNUSED(psc);

    /* 开启TIM中断模式输入捕获 */
    HAL_TIM_IC_Start_IT(&htim1, ATIM_TIMX_CH1);
    HAL_TIM_IC_Start_IT(&htim1, ATIM_TIMX_CH2);
}

/* PWM捕获状态 */
uint8_t g_timx_chy_pwmin_sta = 0;

/* PWM高电平脉宽 */
uint32_t g_timx_chy_pwmin_hval;

/* PWM周期 */
uint32_t g_timx_chy_pwmin_cval;
