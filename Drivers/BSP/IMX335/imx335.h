/**
 ****************************************************************************************************
 * @file        imx335.h
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2025-01-13
 * @brief       IMX335驱动代码
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

#ifndef __IMX335_H
#define __IMX335_H

#include "main.h"

/* 引脚定义 */
#define IMX335_PWDN_GPIO_PORT   GPIOG
#define IMX335_PWDN_GPIO_PIN    GPIO_PIN_6
#define IMX335_RST_GPIO_PORT    GPIOG
#define IMX335_RST_GPIO_PIN     GPIO_PIN_4

/* IO操作 */
#define IMX335_PWDN(x)          do { (x) ?                                                                          \
                                    HAL_GPIO_WritePin(IMX335_PWDN_GPIO_PORT, IMX335_PWDN_GPIO_PIN, GPIO_PIN_SET):   \
                                    HAL_GPIO_WritePin(IMX335_PWDN_GPIO_PORT, IMX335_PWDN_GPIO_PIN, GPIO_PIN_RESET); \
                                } while (0)
#define IMX335_RST(x)           do { (x) ?                                                                          \
                                    HAL_GPIO_WritePin(IMX335_RST_GPIO_PORT, IMX335_RST_GPIO_PIN, GPIO_PIN_SET):     \
                                    HAL_GPIO_WritePin(IMX335_RST_GPIO_PORT, IMX335_RST_GPIO_PIN, GPIO_PIN_RESET);   \
                                } while (0)

/* IMX335 ID和I2C访问地址定义 */
#define IMX335_ID               0x00
#define IMX335_ADDR             0x34

/* 函数声明 */
uint8_t imx335_init(void);                                                      /* 初始化IMX335 */
uint8_t imx335_get_capture(uint32_t address);                                   /* IMX335采集图像 */
uint8_t imx335_start_capture(uint32_t address);                                 /* 启动IMX335采集图像 */
void imx335_stop_capture(void);                                                 /* 停止IMX335采集图像 */
uint32_t imx335_get_capture_frame_count(void);                                  /* 获取IMX335采集帧数 */
uint8_t imx335_isp_background_process(void);                                    /* IMX335 ISP后台处理函数 */
void imx335_dcmipp_pipe_frame_cb(DCMIPP_HandleTypeDef *hdcmipp, uint32_t pipe); /* IMX335 DCMIPP pipe 帧事件回调函数 */
void imx335_dcmipp_pipe_vsync_cb(DCMIPP_HandleTypeDef *hdcmipp, uint32_t pipe); /* IMX335 DCMIPP pipe Vsync事件回调函数 */

#endif
