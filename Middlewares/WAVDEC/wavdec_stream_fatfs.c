/**
 ****************************************************************************************************
 * @file        wavdec_stream_fatfs.c
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
#include "wavdec_ll.h"
#include "ff.h"

#define WAVDEC_STREAM_GET_BUFFER_NUM(stream) (sizeof(stream->buffer_table) / sizeof(stream->buffer_table[0]))

static wavdec_status_t wavdec_stream_fill_buffer(wavdec_stream_t *stream);

wavdec_status_t wavdec_stream_init(wavdec_stream_t *stream, uint32_t buffer_size, void *data)
{
    uint32_t index;
    FRESULT res;
    FIL *file;

    if (stream == NULL)
    {
        return WAVDEC_ERROR;
    }

    if ((buffer_size == 0) || (data == NULL))
    {
        return WAVDEC_ERROR;
    }

    for (index = 0; index < WAVDEC_STREAM_GET_BUFFER_NUM(stream); index++)
    {
        stream->buffer_table[index] = (uint8_t *)wavdec_ll_malloc(buffer_size);
        if (stream->buffer_table[index] == NULL)
        {
            break;
        }
    }

    if (index != WAVDEC_STREAM_GET_BUFFER_NUM(stream))
    {
        goto ERROR_BUFFER_MALLOC;
    }

    stream->buffer = stream->buffer_table[0];
    stream->buffer_size = buffer_size;
    stream->buffer_end = stream->buffer + stream->buffer_size;
    stream->buffer_reader = stream->buffer_end;

    file = (FIL *)wavdec_ll_malloc(sizeof(FIL));
    if (file == NULL)
    {
        goto ERROR_FILE_MALLOC;
    }

    res = f_open(file, (char *)data, FA_READ);
    if (res != FR_OK)
    {
        goto ERROR_FILE_OPEN;
    }

    stream->data = (void *)file;

    return WAVDEC_OK;

ERROR_FILE_OPEN:
    wavdec_ll_free(file);
ERROR_FILE_MALLOC:
    stream->buffer = NULL;
    stream->buffer_size = 0;
    stream->buffer_reader = NULL;
    stream->buffer_end = NULL;
ERROR_BUFFER_MALLOC:
    for (; index > 0; index--)
    {
        wavdec_ll_free(stream->buffer_table[index - 1]);
        stream->buffer_table[index - 1] = NULL;
    }

    return WAVDEC_ERROR;
}

wavdec_status_t wavdec_stream_deinit(wavdec_stream_t *stream)
{
    uint32_t index;

    if (stream == NULL)
    {
        return WAVDEC_ERROR;
    }

    if (stream->data != NULL)
    {
        f_close((FIL *)stream->data);
        wavdec_ll_free(stream->data);
    }

    for (index = 0; index < WAVDEC_STREAM_GET_BUFFER_NUM(stream); index++)
    {
        if (stream->buffer_table[index] != NULL)
        {
            wavdec_ll_free(stream->buffer_table[index]);
            stream->buffer_table[index] = NULL;
        }
    }

    stream->buffer = NULL;
    stream->buffer_size = 0;
    stream->buffer_reader = NULL;
    stream->buffer_end = NULL;
    stream->data = NULL;

    return WAVDEC_OK;
}

wavdec_status_t wavdec_stream_read_byte(wavdec_stream_t *stream, uint8_t *data)
{
    if (stream == NULL)
    {
        return WAVDEC_ERROR;
    }

    if (data == NULL)
    {
        return WAVDEC_ERROR;
    }

    if (stream->buffer_reader >= stream->buffer_end)
    {
        if (wavdec_stream_fill_buffer(stream) != WAVDEC_OK)
        {
            return WAVDEC_ERROR;
        }
    }

    if (stream->buffer_reader < stream->buffer_end)
    {
        *data = *stream->buffer_reader;
        stream->buffer_reader++;
    }

    return WAVDEC_OK;
}

wavdec_status_t wavdec_stream_output(wavdec_stream_t *stream, uint8_t **buffer, uint32_t *size)
{
    if (stream == NULL)
    {
        return WAVDEC_ERROR;
    }

    if (buffer == NULL)
    {
        return WAVDEC_ERROR;
    }

    if (size == NULL)
    {
        return WAVDEC_ERROR;
    }

    if (stream->buffer_reader >= stream->buffer_end)
    {
        if (wavdec_stream_fill_buffer(stream) != WAVDEC_OK)
        {
            return WAVDEC_ERROR;
        }
    }

    *buffer = stream->buffer_reader;
    *size = stream->buffer_end - stream->buffer_reader;

    return WAVDEC_OK;
}

wavdec_status_t wavdec_stream_switch_buffer(wavdec_stream_t *stream)
{
    uint32_t index;

    if (stream == NULL)
    {
        return WAVDEC_ERROR;
    }

    for (index = 0; index < WAVDEC_STREAM_GET_BUFFER_NUM(stream); index++)
    {
        if (stream->buffer == stream->buffer_table[index])
        {
            break;
        }
    }

    if (index == WAVDEC_STREAM_GET_BUFFER_NUM(stream))
    {
        return WAVDEC_ERROR;
    }

    if (++index == WAVDEC_STREAM_GET_BUFFER_NUM(stream))
    {
        index = 0;
    }

    stream->buffer = stream->buffer_table[index];
    stream->buffer_end = stream->buffer + stream->buffer_size;
    stream->buffer_reader = stream->buffer_end;

    return WAVDEC_OK;
}

wavdec_status_t wavdec_stream_flash_buffer(wavdec_stream_t *stream)
{
    uint32_t remain;
    FRESULT res;

    if (stream == NULL)
    {
        return WAVDEC_ERROR;
    }

    if ((stream->buffer == NULL) || (stream->data == NULL))
    {
        return WAVDEC_ERROR;
    }

    if (stream->buffer_reader == stream->buffer)
    {
        return WAVDEC_OK;
    }

    remain = stream->buffer_end - stream->buffer_reader;
    if (remain == 0)
    {
        return WAVDEC_OK;
    }

    res = f_lseek((FIL *)stream->data, f_tell((FIL *)stream->data) - remain);
    if (res != FR_OK)
    {
        return WAVDEC_ERROR;
    }

    stream->buffer_end = stream->buffer + stream->buffer_size;
    stream->buffer_reader = stream->buffer_end;

    return WAVDEC_OK;
}

static wavdec_status_t wavdec_stream_fill_buffer(wavdec_stream_t *stream)
{
    FRESULT res;
    UINT byte_read;

    if (stream == NULL)
    {
        return WAVDEC_ERROR;
    }

    if ((stream->buffer == NULL) || (stream->data == NULL))
    {
        return WAVDEC_ERROR;
    }

    res = f_read((FIL *)stream->data, stream->buffer, (UINT)stream->buffer_size, &byte_read);
    if (res != FR_OK)
    {
        return WAVDEC_ERROR;
    }

    stream->buffer_reader = stream->buffer;
    stream->buffer_end = stream->buffer + byte_read;

    if (stream->buffer_end == stream->buffer)
    {
        return WAVDEC_ERROR;
    }

    return WAVDEC_OK;
}
