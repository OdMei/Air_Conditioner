/**
 ****************************************************************************************************
 * @file        rng.h
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2025-01-13
 * @brief       RNG驱动代码
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

#ifndef __RNG_H
#define __RNG_H

#include "main.h"

/* 函数声明 */
uint8_t rng_init(void);                                 /* 初始化随机数发生器 */
uint32_t rng_get_random(void);                          /* 获取随机数 */
int32_t rng_get_random_range(int32_t min, int32_t max); /* 获取指定范围的随机数 */

#endif
