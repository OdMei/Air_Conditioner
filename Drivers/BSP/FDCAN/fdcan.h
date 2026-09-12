/**
 ****************************************************************************************************
 * @file        fdcan.h
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2025-01-13
 * @brief       FDCAN驱动代码
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

#ifndef __FDCAN_H
#define __FDCAN_H

#include "main.h"

/* FDCAN定义 */
#define FDCAN_DATA_LENGTH 8

/* 函数声明 */
uint8_t fdcan_init(void);                                   /* 初始化FDCAN */
uint8_t fdcan_send_data(uint8_t *buffer, uint32_t timeout); /* FDCAN发送数据 */
uint8_t fdcan_recv_data(uint8_t *buffer, uint32_t timeout); /* FDCAN接收数据 */

#endif
