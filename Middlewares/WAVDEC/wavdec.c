/**
 ****************************************************************************************************
 * @file        wavdec.c
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

#include "wavdec.h"

#define WAVDEC_MKTAG(a, b, c, d) ((a) | ((b) << 8) | ((c) << 16) | ((uint32_t)(d) << 24))

static uint8_t wavdec_read_8bits(wavdec_t *wavdec);
static uint16_t wavdec_read_16bits(wavdec_t *wavdec);
static uint32_t wavdec_read_32bits(wavdec_t *wavdec);
static uint32_t wavdec_get_next_tag(wavdec_t *wavdec, uint32_t *tag);
static wavdec_status_t wavdec_parse_fmt_tag(wavdec_t *wavdec, uint32_t size);
static wavdec_status_t wavdec_parse_data_tag(wavdec_t *wavdec, uint32_t size);

wavdec_status_t wavdec_init(wavdec_t *wavdec, wavdec_configure_t *configure)
{
    if (wavdec == NULL)
    {
        return WAVDEC_ERROR;
    }

    if (wavdec_stream_init(&wavdec->stream, configure->stream.buffer_size, configure->stream.data) != WAVDEC_OK)
    {
        return WAVDEC_ERROR;
    }

    wavdec->channels = 0;
    wavdec->sample_rate = 0;
    wavdec->bit_rate = 0;
    wavdec->block_align = 0;
    wavdec->bits_per_sample = 0;
    wavdec->data_size = 0;

    return WAVDEC_OK;
}

wavdec_status_t wavdec_deinit(wavdec_t *wavdec)
{
    if (wavdec == NULL)
    {
        return WAVDEC_ERROR;
    }

    if (wavdec_stream_deinit(&wavdec->stream) != WAVDEC_OK)
    {
        return WAVDEC_ERROR;
    }

    return WAVDEC_OK;
}

wavdec_status_t wavdec_parse_header(wavdec_t *wavdec)
{
    uint32_t tag;
    uint32_t size;
    wavdec_status_t ret = WAVDEC_OK;

    if (wavdec_read_32bits(wavdec) != WAVDEC_MKTAG('R', 'I', 'F', 'F'))
    {
        return WAVDEC_ERROR;
    }

    wavdec_read_32bits(wavdec);

    if (wavdec_read_32bits(wavdec) != WAVDEC_MKTAG('W', 'A', 'V', 'E'))
    {
        return WAVDEC_ERROR;
    }

    while (1)
    {
        size = wavdec_get_next_tag(wavdec, &tag);

        if (tag == WAVDEC_MKTAG('f', 'm', 't', ' '))
        {
            ret = wavdec_parse_fmt_tag(wavdec, size);
            if (ret != WAVDEC_OK)
            {
                break;
            }
        }
        else if (tag == WAVDEC_MKTAG('d', 'a', 't', 'a'))
        {
            ret = wavdec_parse_data_tag(wavdec, size);
            break;
        }
        else
        {
            while (size--)
            {
                wavdec_read_8bits(wavdec);
            }
        }
    }

    if (ret == WAVDEC_OK)
    {
        wavdec_stream_flash_buffer(&wavdec->stream);
    }

    return ret;
}

static uint8_t wavdec_read_8bits(wavdec_t *wavdec)
{
    uint8_t data;

    wavdec_stream_read_byte(&wavdec->stream, &data);

    return data;
}

static uint16_t wavdec_read_16bits(wavdec_t *wavdec)
{
    uint16_t value;

    value = wavdec_read_8bits(wavdec);
    value |= wavdec_read_8bits(wavdec) << 8;

    return value;
}

static uint32_t wavdec_read_32bits(wavdec_t *wavdec)
{
    uint32_t value;

    value = wavdec_read_16bits(wavdec);
    value |= wavdec_read_16bits(wavdec) << 16;

    return value;
}

static uint32_t wavdec_get_next_tag(wavdec_t *wavdec, uint32_t *tag)
{
    *tag = wavdec_read_32bits(wavdec);
    return wavdec_read_32bits(wavdec);
}

static wavdec_status_t wavdec_parse_fmt_tag(wavdec_t *wavdec, uint32_t size)
{
    uint16_t id;

    if (size < 14)
    {
        return WAVDEC_ERROR;
    }

    id = wavdec_read_16bits(wavdec);
    if ((id != 0x0001) && (id != 0xFFFE))
    {
        return WAVDEC_ERROR;
    }

    wavdec->channels = wavdec_read_16bits(wavdec);
    wavdec->sample_rate = wavdec_read_32bits(wavdec);
    wavdec->bit_rate = wavdec_read_32bits(wavdec) << 3;
    wavdec->block_align = wavdec_read_16bits(wavdec);

    if (size == 14)
    {
        wavdec->bits_per_sample = 8;
    }
    else
    {
        wavdec->bits_per_sample = wavdec_read_16bits(wavdec);
        size -= 16;
        while (size--)
        {
            wavdec_read_8bits(wavdec);
        }
    }

    return WAVDEC_OK;
}

static wavdec_status_t wavdec_parse_data_tag(wavdec_t *wavdec, uint32_t size)
{
    if (size == 0)
    {
        return WAVDEC_ERROR;
    }

    wavdec->data_size = size;

    return WAVDEC_OK;
}
