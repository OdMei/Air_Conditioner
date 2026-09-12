/**
 ****************************************************************************************************
 * @file        exti.h
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2025-01-13
 * @brief       外部中断驱动代码
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

#ifndef __EXTI_H
#define __EXTI_H

#include "main.h"

/* 引脚定义 */
#define WKUP_INT_GPIO_PIN   GPIO_PIN_13
#define KEY0_INT_GPIO_PIN   GPIO_PIN_6
#define KEY1_INT_GPIO_PIN   GPIO_PIN_1
#define KEY2_INT_GPIO_PIN   GPIO_PIN_11

/* 函数声明 */
void exti_init(void);   /* 初始化外部中断 */

#endif
