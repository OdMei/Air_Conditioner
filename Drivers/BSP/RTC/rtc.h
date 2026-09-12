/**
 ****************************************************************************************************
 * @file        rtc.h
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2025-01-13
 * @brief       RTC驱动代码
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

#ifndef __RTC_H
#define __RTC_H

#include "main.h"

/* 函数声明 */
uint32_t rtc_read_bkr(uint32_t bkrx);                                               /* 读取后备寄存器 */
void rtc_write_bkr(uint32_t bkrx, uint32_t data);                                   /* 写后背寄存器 */
uint8_t rtc_init(void);                                                             /* 初始化RTC */
uint8_t rtc_set_time(uint8_t hour, uint8_t minute, uint8_t second, uint8_t ampm);   /* 设置RTC时间信息 */
uint8_t rtc_set_date(uint8_t year, uint8_t month, uint8_t date, uint8_t week);      /* 设置RTC日期信息 */
void rtc_get_time(uint8_t *hour, uint8_t *minute, uint8_t *second, uint8_t *ampm);  /* 获取RTC时间信息 */
void rtc_get_date(uint8_t *year, uint8_t *month, uint8_t *date, uint8_t *week);     /* 获取RTC日期信息 */
void rtc_set_alarm(uint8_t week, uint8_t hour, uint8_t minute, uint8_t second);     /* 设置RTC闹钟时间信息 */
void rtc_set_wakeup(uint8_t clock, uint8_t count);                                  /* 设置RTC周期性唤醒中断 */

#endif
