/**
 ****************************************************************************************************
 * @file        gtim.h
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

#ifndef __GTIM_H
#define __GTIM_H

#include "main.h"

/* 通用定时器定义 */
#define GTIM_TIMX       TIM3
#define GTIM_TIMX_CH1   TIM_CHANNEL_1

/* 变量导出 */
extern uint32_t g_timx_chy_cnt_ofcnt;                       /* TIM溢出计数器 */

/* 函数声明 */
void gtim_timx_int_init(uint16_t arr, uint16_t psc);        /* 初始化通用定时器中断 */
void gtim_timx_pwm_chy_init(uint16_t arr, uint16_t psc);    /* 初始化通用定时器PWM输出 */
void gtim_timx_cap_chy_init(uint16_t arr, uint16_t psc);    /* 初始化通用定时器输入捕获 */
void gtim_timx_cnt_chy_init(uint16_t psc);                  /* 初始化通用定时器脉冲计数 */
void gtim_timx_cnt_chy_restart(void);                       /* 重启通用定时器脉冲计数 */
uint32_t gtim_timx_cnt_chy_get_count(void);                 /* 获取通用定时器脉冲计数值 */

#endif
