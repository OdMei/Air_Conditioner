/**
 ****************************************************************************************************
 * @file        iwdg.h
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

#ifndef __IWDG_H
#define __IWDG_H

#include "main.h"

/* 函数声明 */
void iwdg_init(uint32_t prescaler, uint32_t reload);    /* 初始化IWDG */
void iwdg_feed(void);                                   /* 喂狗IWDG */

#endif
