/**
 ****************************************************************************************************
 * @file        dht11.h
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2025-01-13
 * @brief       DHT11驱动代码
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

#ifndef __DHT11_H
#define __DHT11_H

#include "main.h"

/* 引脚定义 */
#define DHT11_DQ_GPIO_PORT  GPIOG
#define DHT11_DQ_GPIO_PIN   GPIO_PIN_3

/* IO操作 */
#define DHT11_DQ_OUT(x)     do { (x) ?                                                                      \
                                HAL_GPIO_WritePin(DHT11_DQ_GPIO_PORT, DHT11_DQ_GPIO_PIN, GPIO_PIN_SET):     \
                                HAL_GPIO_WritePin(DHT11_DQ_GPIO_PORT, DHT11_DQ_GPIO_PIN, GPIO_PIN_RESET);   \
                            } while (0)
#define DHT11_DQ_IN()       ((HAL_GPIO_ReadPin(DHT11_DQ_GPIO_PORT, DHT11_DQ_GPIO_PIN) == GPIO_PIN_RESET) ? 0 : 1)

/* 函数声明 */
uint8_t dht11_init(void);                                       /* 初始化DHT11 */
uint8_t dht11_get_data(float *temperature, float *humidity);    /* 获取DHT11的温湿度值 */

#endif
