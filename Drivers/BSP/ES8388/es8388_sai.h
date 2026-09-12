/**
 ****************************************************************************************************
 * @file        es8388_sai.h
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2025-01-13
 * @brief       ES8388 SAI驱动代码
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

#ifndef __ES8388_SAI_H
#define __ES8388_SAI_H

#include "main.h"

/* 函数声明 */
uint8_t es8388_sai_clock_config(uint32_t sample_rate);                                          /* 配置ES8388 SAI时钟 */
uint8_t es8388_sai_init_tx(uint16_t channels, uint32_t sample_rate, uint16_t bits_per_sample);  /* 初始化ES8388 SAI发送 */
uint8_t es8388_sai_init_rx(uint16_t channels, uint32_t sample_rate, uint16_t bits_per_sample);  /* 初始化ES8388 SAI接收 */
void es8388_sai_tx_dma_cplt_user_cb_register(void (*cb)(void));                                 /* ES8388 SAI 注册DMA发送完成回调 */
void es8388_sai_rx_dma_cplt_user_cb_register(void (*cb)(void));                                 /* ES8388 SAI 注册DMA接收完成回调 */
uint8_t es8388_sai_tx_dma(uint8_t *buffer, uint32_t data_amount);                               /* ES8388 SAI DMA发送 */
uint8_t es8388_sai_rx_dma(uint8_t *buffer, uint32_t data_amount);                               /* ES8388 SAI DMA接收 */
void es8388_sai_tx_dma_cplt_handler(void);                                                      /* ES8388 SAI DMA发送完成处理函数 */
void es8388_sai_rx_dma_cplt_handler(void);                                                      /* ES8388 SAI DMA接收完成处理函数 */
uint8_t es8388_sai_tx_dma_is_busy(void);                                                        /* ES8388 SAI DMA发送是否忙 */
uint8_t es8388_sai_rx_dma_is_busy(void);                                                        /* ES8388 SAI DMA接收是否忙 */

#endif
