/**
 ****************************************************************************************************
 * @file        wavenc_stream_fatfs.c
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
#include "wavenc_ll.h"
#include "ff.h"

#define WAVENC_STREAM_GET_BUFFER_NUM(stream) (sizeof(stream->buffer) / sizeof(stream->buffer[0]))

static wavenc_status_t wavenc_stream_flush_buffer(wavenc_stream_t *stream, uint32_t index);

wavenc_status_t wavenc_stream_init(wavenc_stream_t *stream, uint32_t buffer_size, void *data)
{
    uint32_t index;
    FRESULT res;
    FIL *file;

    if (stream == NULL)
    {
        return WAVENC_ERROR;
    }

    if ((buffer_size == 0) || (data == NULL))
    {
        return WAVENC_ERROR;
    }

    for (index = 0; index < WAVENC_STREAM_GET_BUFFER_NUM(stream); index++)
    {
        stream->buffer[index].buffer_base = (uint8_t *)wavenc_ll_malloc(buffer_size);
        if (stream->buffer[index].buffer_base == NULL)
        {
            break;
        }
        stream->buffer[index].buffer_writer = stream->buffer[index].buffer_base;
        stream->buffer[index].buffer_end = stream->buffer[index].buffer_base + buffer_size;
    }

    if (index != WAVENC_STREAM_GET_BUFFER_NUM(stream))
    {
        goto ERROR_BUFFER_MALLOC;
    }

    stream->buffer_current_index = 0;

    file = (FIL *)wavenc_ll_malloc(sizeof(FIL));
    if (file == NULL)
    {
        goto ERROR_FILE_MALLOC;
    }

    res = f_open(file, (char *)data, FA_WRITE | FA_CREATE_ALWAYS);
    if (res != FR_OK)
    {
        goto ERROR_FILE_OPEN;
    }

    stream->data = (void *)file;

    return WAVENC_OK;

ERROR_FILE_OPEN:
    wavenc_ll_free(file);
ERROR_FILE_MALLOC:
    stream->data = NULL;
ERROR_BUFFER_MALLOC:
    for (; index > 0; index--)
    {
        wavenc_ll_free(stream->buffer[index - 1].buffer_base);
        stream->buffer[index - 1].buffer_base = NULL;
        stream->buffer[index - 1].buffer_writer = NULL;
        stream->buffer[index - 1].buffer_end = NULL;
    }

    return WAVENC_ERROR;
}

wavenc_status_t wavenc_stream_deinit(wavenc_stream_t *stream)
{
    uint32_t index;

    if (stream == NULL)
    {
        return WAVENC_ERROR;
    }

    if (stream->data != NULL)
    {
        f_close((FIL *)stream->data);
        wavenc_ll_free(stream->data);
        stream->data = NULL;
    }

    for (index = 0; index < WAVENC_STREAM_GET_BUFFER_NUM(stream); index++)
    {
        if (stream->buffer[index].buffer_base != NULL)
        {
            wavenc_ll_free(stream->buffer[index].buffer_base);
            stream->buffer[index].buffer_base = NULL;
            stream->buffer[index].buffer_writer = NULL;
            stream->buffer[index].buffer_end = NULL;
        }
    }

    stream->buffer_current_index = 0;

    return WAVENC_OK;
}

wavenc_status_t wavenc_stream_flush_current_buffer(wavenc_stream_t *stream)
{
    return wavenc_stream_flush_buffer(stream, stream->buffer_current_index);
}

wavenc_status_t wavenc_stream_write_byte(wavenc_stream_t *stream, uint8_t *data)
{
    if (stream == NULL)
    {
        return WAVENC_ERROR;
    }

    if (data == NULL)
    {
        return WAVENC_ERROR;
    }

    if (stream->buffer[stream->buffer_current_index].buffer_writer >= stream->buffer[stream->buffer_current_index].buffer_end)
    {
        if (wavenc_stream_flush_current_buffer(stream) != WAVENC_OK)
        {
            return WAVENC_ERROR;
        }
    }

    if (stream->buffer[stream->buffer_current_index].buffer_writer < stream->buffer[stream->buffer_current_index].buffer_end)
    {
        *stream->buffer[stream->buffer_current_index].buffer_writer = *data;
        stream->buffer[stream->buffer_current_index].buffer_writer++;
    }

    return WAVENC_OK;
}

wavenc_status_t wavenc_stream_input(wavenc_stream_t *stream)
{
    uint32_t index;

    if (stream == NULL)
    {
        return WAVENC_ERROR;
    }

    if (stream->buffer_current_index == 0)
    {
        index = WAVENC_STREAM_GET_BUFFER_NUM(stream) - 1;
    }
    else
    {
        index = stream->buffer_current_index - 1;
    }

    stream->buffer[index].buffer_writer = stream->buffer[index].buffer_end;

    if (wavenc_stream_flush_buffer(stream, index) != WAVENC_OK)
    {
        return WAVENC_ERROR;
    }

    return WAVENC_OK;
}

uint8_t *wavenc_stream_get_buffer(wavenc_stream_t *stream)
{
    if (stream == NULL)
    {
        return NULL;
    }

    if (++stream->buffer_current_index == WAVENC_STREAM_GET_BUFFER_NUM(stream))
    {
        stream->buffer_current_index = 0;
    }

    return stream->buffer[stream->buffer_current_index].buffer_base;
}

static wavenc_status_t wavenc_stream_flush_buffer(wavenc_stream_t *stream, uint32_t index)
{
    FRESULT res;
    UINT byte_write;

    if (stream == NULL)
    {
        return WAVENC_ERROR;
    }

    if (index >= WAVENC_STREAM_GET_BUFFER_NUM(stream))
    {
        return WAVENC_ERROR;
    }

    if ((stream->buffer[index].buffer_base == NULL) || (stream->data == NULL))
    {
        return WAVENC_ERROR;
    }

    if (stream->buffer[index].buffer_writer == stream->buffer[index].buffer_base)
    {
        return WAVENC_OK;
    }

    res = f_write((FIL *)stream->data, stream->buffer[index].buffer_base, (UINT)(stream->buffer[index].buffer_writer - stream->buffer[index].buffer_base), &byte_write);
    if (res != FR_OK)
    {
        return WAVENC_ERROR;
    }

    if (byte_write != (stream->buffer[index].buffer_writer - stream->buffer[index].buffer_base))
    {
        return WAVENC_ERROR;
    }

    stream->buffer[index].buffer_writer = stream->buffer[index].buffer_base;

    return WAVENC_OK;
}
