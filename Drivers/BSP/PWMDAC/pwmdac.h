/**
 ****************************************************************************************************
 * @file        pwmdac.h
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2025-01-13
 * @brief       PWMDAC驱动代码
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

#ifndef __PWMDAC_H
#define __PWMDAC_H

#include "main.h"

/* PWMDAC定义 */
#define PWMDAC_TIMX_CH1 TIM_CHANNEL_4

/* 函数声明 */
void pwmdac_init(uint16_t arr, uint16_t psc);   /* 初始化PWMDAC */
void pwmdac_set_voltage(uint16_t voltage);      /* 设置PWMDAC输出电压 */

#endif
