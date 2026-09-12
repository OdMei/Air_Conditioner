/**
 ****************************************************************************************************
 * @file        remote.h
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2025-01-13
 * @brief       红外遥控驱动代码
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

#ifndef __REMOTE_H
#define __REMOTE_H

#include "main.h"

/* 红外遥控定义 */
#define REMOTE_DATA_GPIO_PORT   GPIOE
#define REMOTE_DATA_GPIO_PIN    GPIO_PIN_9

#define REMOTE_TIMX             TIM1
#define REMOTE_TIMX_CH1         TIM_CHANNEL_1

#define REMOTE_ID               0

/* IO操作 */
#define REMOTE_DATA()           ((HAL_GPIO_ReadPin(REMOTE_DATA_GPIO_PORT, REMOTE_DATA_GPIO_PIN) == GPIO_PIN_RESET) ? 0 : 1)

/* 变量导出 */
extern uint8_t g_remote_sta;
extern uint32_t g_remote_data;
extern uint8_t g_remote_cnt;

/* 函数声明 */
void remote_init(void);     /* 初始化红外遥控 */
uint8_t remote_scan(void);  /* 扫描红外遥控键值 */

#endif
