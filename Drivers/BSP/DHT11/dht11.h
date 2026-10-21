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
typedef enum
{
    DHT11_STATUS_NOT_READ = 0,
    DHT11_STATUS_OK,
    DHT11_STATUS_BAD_ARGUMENT,
    DHT11_STATUS_BAD_CONTEXT,
    DHT11_STATUS_NO_RESPONSE,
    DHT11_STATUS_RESPONSE_TIMEOUT,
    DHT11_STATUS_BIT_HIGH_TIMEOUT,
    DHT11_STATUS_BIT_LOW_TIMEOUT,
    DHT11_STATUS_CHECKSUM
} dht11_status_t;

typedef struct
{
    uint32_t attempts;
    uint32_t successes;
    uint32_t failures;
    uint32_t consecutive_failures;
    uint32_t last_attempt_ms;
    uint32_t last_success_ms;
    dht11_status_t last_status;
    uint8_t has_sample;
} dht11_diag_t;

uint8_t dht11_init(void);                                       /* 初始化DHT11 */
/* 0: complete checked frame; 1: bad arguments, timing or checksum failure.
 * Caller outputs remain unchanged on failure. */
uint8_t dht11_get_data(float *temperature, float *humidity);    /* 获取DHT11的温湿度值 */
/* Read a coherent diagnostic snapshot; initialization is not a sample. */
void dht11_get_diag(dht11_diag_t *diag);

#endif
