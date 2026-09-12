/**
 ****************************************************************************************************
 * @file        dts.h
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2025-01-13
 * @brief       DTS驱动代码
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

#ifndef __DTS_H
#define __DTS_H

#include "main.h"

/* 函数声明 */
void dts_init(void);                            /* 初始化DTS */
float_t dts_get_result(HAL_DTS_Sensor sensor);  /* 获取DTS测量结果 */

#endif
