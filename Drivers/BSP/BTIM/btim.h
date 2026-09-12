/**
 ****************************************************************************************************
 * @file        btim.h
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

#ifndef __BTIM_H
#define __BTIM_H

#include "main.h"

/* 基本定时器定义 */
#define BTIM_TIMX   TIM6

/* 函数声明 */
void btim_timx_int_init(uint16_t arr, uint16_t psc);    /* 初始化基本定时器中断 */

#endif
