/**
 ****************************************************************************************************
 * @file        mjpeg.h
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2025-03-18
 * @brief       MJPEG视频处理 代码
 * @license     Copyright (c) 2023-2035, 广州市星翼电子科技有限公司
 ****************************************************************************************************
 * @attention
 *
 * 实验平台:正点原子 N647开发板
 * 在线视频:www.yuanzige.com
 * 技术论坛:www.openedv.com
 * 公司网址:www.alientek.com
 * 购买地址:zhengdianyuanzi.tmall.com
 *
 ****************************************************************************************************
 */

#ifndef __MJPEG_H
#define __MJPEG_H

#include "main.h"
#include "./JPEGCODEC/jpegcodec.h"


extern jpeg_codec_typedef mjpeg;


uint8_t mjpeg_jpeg_core_init(jpeg_codec_typedef *tjpeg);
void mjpeg_jpeg_core_destroy(jpeg_codec_typedef *tjpeg); 
void mjpeg_dma_in_callback(void);
void mjpeg_dma_out_callback(void);
void mjpeg_endofcovert_callback(void);
void mjpeg_hdrover_callback(void);
uint8_t mjpeg_init(uint16_t offx, uint16_t offy, uint32_t width, uint32_t height);
void mjpeg_free(void);
void mjpeg_ltdc_dma2d_yuv2rgb_fill(uint16_t sx, uint16_t sy, jpeg_codec_typedef *tjpeg);
void mjpeg_fill_color(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t *color);
uint8_t mjpeg_decode(uint8_t *buf, uint32_t bsize);

#endif
