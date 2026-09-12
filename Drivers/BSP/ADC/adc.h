/**
 ****************************************************************************************************
 * @file        adc.h
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2025-01-13
 * @brief       ADC驱动代码
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

#ifndef __ADC_H
#define __ADC_H

#include "main.h"

/* 函数声明 */
void adc_init(void);                                                /* 初始化ADC */
uint32_t adc_get_result(uint32_t channel);                          /* 获取ADC结果 */
uint32_t adc_get_result_average(uint32_t channel, uint8_t times);   /* 均值滤波获取ADC结果 */

#endif
