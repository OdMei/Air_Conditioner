/**
 ****************************************************************************************************
 * @file        tpad.h
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2025-01-13
 * @brief       电容触摸按键驱动代码
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

#ifndef __TPAD_H
#define __TPAD_H

#include "main.h"

/* 电容触摸按键定义 */
#define TPAD_GPIO_PORT          GPIOF
#define TPAD_GPIO_PIN           GPIO_PIN_6
#define TPAD_GPIO_AF            GPIO_AF1_TIM2
#define TPAD_TIMX_CAP           TIM2
#define TPAD_TIMX_CAP_CHY       TIM_CHANNEL_4
#define TPAD_TIMX_ARR_MAX_VAL   0xFFFFFFFF
#define TPAD_GATE_VAL           100

/* 函数声明 */
uint8_t tpad_init(uint16_t psc);    /* 初始化TPAD */
uint8_t tpad_scan(uint8_t mode);    /* 扫描TPAD */

#endif
