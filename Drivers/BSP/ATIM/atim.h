/**
 ****************************************************************************************************
 * @file        atim.h
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

#ifndef __ATIM_H
#define __ATIM_H

#include "main.h"

/* 高级定时器定义 */
#define ATIM_TIMX       TIM1
#define ATIM_TIMX_CH1   TIM_CHANNEL_1
#define ATIM_TIMX_CH2   TIM_CHANNEL_2

/* 变量导出 */
extern uint32_t g_timx_chy_npwm_remain;
extern uint8_t g_timx_chy_pwmin_sta;
extern uint32_t g_timx_chy_pwmin_hval;
extern uint32_t g_timx_chy_pwmin_cval;

/* 函数声明 */
void atim_timx_chy_npwm_init(uint16_t arr, uint16_t psc);   /* 初始化高级定时器输出指定个数PWM */
void atim_timx_chy_npwm_set(uint32_t npwm);                 /* 设置高级定时器输出指定个数PWM */
void atim_timx_comp_init(uint16_t arr, uint16_t psc);       /* 初始化高级定时器输出比较 */
void atim_timx_cplm_init(uint16_t arr, uint16_t psc);       /* 初始化高级定时器互补输出带死区控制 */
void atim_timx_cplm_set(uint16_t ccr, uint8_t dtg);         /* 设置高级定时器互补输出带死区控制 */
void atim_timx_pwmin_chy_init(uint16_t psc);                /* 初始化高级定时器PWM输入 */

#endif
