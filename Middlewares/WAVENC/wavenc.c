/**
 ****************************************************************************************************
 * @file        wavenc.c
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2025-01-13
 * @brief       WAV编码库
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

#include "wavenc.h"

#define WAVENC_MKTAG(a, b, c, d) ((a) | ((b) << 8) | ((c) << 16) | ((uint32_t)(d) << 24))

static void wavenc_write_8bits(wavenc_t *wavenc, uint8_t data);
static void wavenc_write_16bits(wavenc_t *wavenc, uint16_t data);
static void wavenc_write_32bits(wavenc_t *wavenc, uint32_t data);
static void wavenc_put_fmt_tag(wavenc_t *wavenc);
static void wavenc_put_header(wavenc_t *wavenc);

wavenc_status_t wavenc_init(wavenc_t *wavenc, wavenc_configure_t *configure)
{
    if (wavenc == NULL)
    {
        return WAVENC_ERROR;
    }

    if (configure == NULL)
    {
        return WAVENC_ERROR;
    }

    if ((configure->channels != 1) && configure->channels != 2)
    {
        return WAVENC_ERROR;
    }

    if ((configure->sample_rate != 8000) && (configure->sample_rate != 11025) && (configure->sample_rate != 16000) && (configure->sample_rate != 22050)  && (configure->sample_rate != 32000) && (configure->sample_rate != 44100) && (configure->sample_rate != 48000) && (configure->sample_rate != 96000) && (configure->sample_rate != 192000))
    {
        return WAVENC_ERROR;
    }

    if (configure->bits_per_sample != 16)
    {
        return WAVENC_ERROR;
    }

    if (wavenc_stream_init(&wavenc->stream, configure->stream.buffer_size, configure->stream.data) != WAVENC_OK)
    {
        return WAVENC_ERROR;
    }


    wavenc->channels = configure->channels;
    wavenc->sample_rate = configure->sample_rate;
    wavenc->bit_rate = configure->channels * configure->sample_rate * configure->bits_per_sample;
    wavenc->block_align = configure->channels * (configure->bits_per_sample / 8);
    wavenc->bits_per_sample = configure->bits_per_sample;
    wavenc->data_size = 0;

    wavenc_put_header(wavenc);
    wavenc_stream_flush_current_buffer(&wavenc->stream);

    return WAVENC_OK;
}

wavenc_status_t wavenc_deinit(wavenc_t *wavenc)
{
    if (wavenc == NULL)
    {
        return WAVENC_ERROR;
    }

    if (wavenc_stream_deinit(&wavenc->stream) != WAVENC_OK)
    {
        return WAVENC_ERROR;
    }

    wavenc->channels = 0;
    wavenc->sample_rate = 0;
    wavenc->bits_per_sample = 0;

    return WAVENC_OK;
}

static void wavenc_write_8bits(wavenc_t *wavenc, uint8_t data)
{
    wavenc_stream_write_byte(&wavenc->stream, &data);
}

static void wavenc_write_16bits(wavenc_t *wavenc, uint16_t data)
{
    wavenc_write_8bits(wavenc, (uint8_t)data);
    wavenc_write_8bits(wavenc, data >> 8);
}

static void wavenc_write_32bits(wavenc_t *wavenc, uint32_t data)
{
    wavenc_write_8bits(wavenc, (uint8_t)data);
    wavenc_write_8bits(wavenc, (uint8_t)(data >> 8));
    wavenc_write_8bits(wavenc, (uint8_t)(data >> 16));
    wavenc_write_8bits(wavenc, data >> 24);
}

static void wavenc_put_fmt_tag(wavenc_t *wavenc)
{
    wavenc_write_32bits(wavenc, 0x00000010);
    wavenc_write_16bits(wavenc, 0x0001);
    wavenc_write_16bits(wavenc, wavenc->channels);
    wavenc_write_32bits(wavenc, wavenc->sample_rate);
    wavenc_write_32bits(wavenc, wavenc->bit_rate >> 3);
    wavenc_write_16bits(wavenc, wavenc->block_align);
    wavenc_write_16bits(wavenc, wavenc->bits_per_sample);
}

static void wavenc_put_header(wavenc_t *wavenc)
{
    wavenc_write_32bits(wavenc, WAVENC_MKTAG('R', 'I', 'F', 'F'));
    wavenc_write_32bits(wavenc, -1);

    wavenc_write_32bits(wavenc, WAVENC_MKTAG('W', 'A', 'V', 'E'));

    wavenc_write_32bits(wavenc, WAVENC_MKTAG('f', 'm', 't', ' '));
    wavenc_put_fmt_tag(wavenc);

    wavenc_write_32bits(wavenc, WAVENC_MKTAG('d', 'a', 't', 'a'));
    wavenc_write_32bits(wavenc, -1);
}
