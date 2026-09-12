/**
 ****************************************************************************************************
 * @file        wavdec.h
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2025-01-13
 * @brief       WAV解码库
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

#ifndef __WAVDEC_H
#define __WAVDEC_H

#include "main.h"

typedef enum
{
    WAVDEC_OK = 0,
    WAVDEC_ERROR = 1,
} wavdec_status_t;

typedef struct wavdec_stream_configure
{
    struct
    {
        uint32_t buffer_size;
        void *data;
    } stream;
} wavdec_configure_t;

typedef struct wavdec_stream
{
    uint8_t *buffer_table[2];

    uint8_t *buffer;
    uint32_t buffer_size;

    uint8_t *buffer_reader;
    uint8_t *buffer_end;

    void *data;
} wavdec_stream_t;

typedef struct wavdec
{
    wavdec_stream_t stream;

    uint16_t channels;
    uint32_t sample_rate;
    uint32_t bit_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;

    uint32_t data_size;
} wavdec_t;

wavdec_status_t wavdec_init(wavdec_t *wavdec, wavdec_configure_t *configure);
wavdec_status_t wavdec_deinit(wavdec_t *wavdec);
wavdec_status_t wavdec_parse_header(wavdec_t *wavdec);
wavdec_status_t wavdec_get_data_buffer(wavdec_t *wavdec, uint8_t **buffer, uint32_t *size);

wavdec_status_t wavdec_stream_init(wavdec_stream_t *stream, uint32_t buffer_size, void *data);
wavdec_status_t wavdec_stream_deinit(wavdec_stream_t *stream);
wavdec_status_t wavdec_stream_read_byte(wavdec_stream_t *stream, uint8_t *data);
wavdec_status_t wavdec_stream_output(wavdec_stream_t *stream, uint8_t **buffer, uint32_t *size);
wavdec_status_t wavdec_stream_switch_buffer(wavdec_stream_t *stream);
wavdec_status_t wavdec_stream_flash_buffer(wavdec_stream_t *stream);

#endif
