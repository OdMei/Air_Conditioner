/**
 ****************************************************************************************************
 * @file        jpegcodec.c
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2025-03-18
 * @brief       硬件JPEG编解码器驱动代码
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

#include "malloc.h"
#include "./UART/uart.h"
#include "./JPEGCODEC/jpegcodec.h"


/* JPEG规范(ISO/IEC 10918-1标准)的样本量化表
 * 获取JPEG图片质量时需要用到
 */
const uint8_t JPEG_LUM_QuantTable[JPEG_QUANT_TABLE_SIZE] =
{
    16, 11, 10, 16, 24, 40, 51, 61, 12, 12, 14, 19, 26, 58, 60, 55,
    14, 13, 16, 24, 40, 57, 69, 56, 14, 17, 22, 29, 51, 87, 80, 62,
    18, 22, 37, 56, 68, 109, 103, 77, 24, 35, 55, 64, 81, 104, 113, 92,
    49, 64, 78, 87, 103, 121, 120, 101, 72, 92, 95, 98, 112, 100, 103, 99
};
const uint8_t JPEG_ZIGZAG_ORDER[JPEG_QUANT_TABLE_SIZE] =
{
    0, 1, 8, 16, 9, 2, 3, 10, 17, 24, 32, 25, 18, 11, 4, 5,
    12, 19, 26, 33, 40, 48, 41, 34, 27, 20, 13, 6, 7, 14, 21, 28,
    35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51,
    58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63
};

/**
 * @brief       JPEG硬件解码输入HPDMA配置
 * @param       meminaddr   : JPEG输入HPDMA存储器地址
 * @param       meminsize   : 输入DMA数据长度,0~262143,以字节为单位
 * @retval      无
 */
void jpeg_in_dma_init(uint32_t meminaddr, uint32_t meminsize)
{
    if (meminsize % 4)
    {
        meminsize += 4 - meminsize % 4;     /* 扩展到4的倍数 */
    }
    __HAL_RCC_HPDMA1_CLK_ENABLE();          /* 使能HPDMA时钟 */

    HAL_DMA_Init(&handle_HPDMA1_Channel0);  /* 重新初始化 */

    HAL_DMA_Start(&handle_HPDMA1_Channel0, meminaddr, (uint32_t)&JPEG->DIR, meminsize);  /* 开启DMA */
    __HAL_DMA_ENABLE_IT(&handle_HPDMA1_Channel0, DMA_IT_TC);                             /* 开启传输完成中断 */
}

/**
 * @brief       JPEG硬件解码输出HPDMA配置
 * @param       memoutaddr  : JPEG输出HPDMA存储器地址
 * @param       memoutsize  : 输出DMA数据长度,0~262143,以字节为单位
 * @retval      无
 */
void jpeg_out_dma_init(uint32_t memoutaddr, uint32_t memoutsize)
{
    if (memoutsize % 4)
    {
        memoutsize += 4 - memoutsize % 4;   /* 扩展到4的倍数 */
    }

    __HAL_RCC_HPDMA1_CLK_ENABLE();          /* 使能HPDMA时钟 */

    /* JPEG数据输出HPDMA通道配置 */
    HPDMA1_Channel1->CCR = 0;               /* 清零CCR */
    while (HPDMA1_Channel1->CCR & 0X01);    /* 等待HPDMA1_Channel1关闭完成 */

    HAL_DMA_Init(&handle_HPDMA1_Channel1);  /* 重新初始化 */
    HAL_DMA_Start(&handle_HPDMA1_Channel1, (uint32_t)&JPEG->DOR, memoutaddr, memoutsize); /* 开启DMA */
    __HAL_DMA_ENABLE_IT(&handle_HPDMA1_Channel1, DMA_IT_TC);                              /* 开启传输完成中断 */

}

void (*jpeg_in_callback)(void);     /* JPEG HPDMA输入回调函数 */
void (*jpeg_out_callback)(void);    /* JPEG HPDMA输出 回调函数 */
void (*jpeg_eoc_callback)(void);    /* JPEG 解码完成 回调函数 */
void (*jpeg_hdp_callback)(void);    /* JPEG Header解码完成 回调函数 */

void dma_Channel0_callback(void);    /* DMA传输完成回调函数 */
void dma_Channel1_callback(void);    /* DMA传输完成回调函数 */
void jpeg_decode_callback(void);     /* JPEG信息头解码函数 */


/**
 * @brief       HPDMA中断服务函数
 * @note        处理硬件JPEG解码时输入/输出数据流
 * @param       无
 * @retval      无
 */
void dma_Channel0_callback(void)
{
    if (__HAL_DMA_GET_FLAG(&handle_HPDMA1_Channel0, DMA_FLAG_TC) != RESET)   /* HPDMA传输完成 */
    {
        __HAL_DMA_CLEAR_FLAG(&handle_HPDMA1_Channel0, DMA_FLAG_TC);          /* 清除HPDMA传输完成中断标志位 */
        JPEG->CR &= ~(1 << 11);                                         /* 关闭JPEG的HPDMA IN */
        JPEG->CR &= ~(0X7E);                                            /* 关闭JPEG中断,防止被打断. */
        jpeg_in_callback();                                             /* 执行回调函数 */
        JPEG->CR |= 3 << 5;                                             /* 使能EOC和HPD中断. */
    }
}
void dma_Channel1_callback(void)
{
    if (__HAL_DMA_GET_FLAG(&handle_HPDMA1_Channel1, DMA_FLAG_TC) != RESET)  /* HPDMA传输完成 */
    {
        __HAL_DMA_CLEAR_FLAG(&handle_HPDMA1_Channel1, DMA_FLAG_TC);         /* 清除HPDMA传输完成中断标志位 */
        JPEG->CR &= ~(1 << 12);
        JPEG->CR &= ~(0X7E);                                            /* 关闭JPEG中断,防止被打断. */
        jpeg_out_callback();                                            /* 执行回调函数 */
        JPEG->CR |= 3 << 5;                                             /* 使能EOC和HPD中断. */
    }
}

/**
 * @brief       JPEG解码中断服务函数
 * @param       无
 * @retval      无
 */
void jpeg_decode_callback(void)
{
    if (__HAL_JPEG_GET_FLAG(&hjpeg, JPEG_FLAG_HPDF) != RESET)/* JPEG Header解码完成 */
    {
        jpeg_hdp_callback();
        JPEG->CR &= ~(1 << 6);                                      /* 禁止Jpeg Header解码完成中断 */
        JPEG->CFR |= 1 << 6;                                        /* 清除HPDF位(header解码完成位) */
    }

    if (__HAL_JPEG_GET_FLAG(&hjpeg, JPEG_FLAG_EOCF) != RESET)/* JPEG解码完成 */
    {
        jpeg_dma_stop();
        jpeg_eoc_callback();
        JPEG->CFR |= 1 << 5;                                        /* 清除EOC位(解码完成位) */
        __HAL_DMA_DISABLE(&handle_HPDMA1_Channel0);                      /* 关闭JPEG数据输入DMA */
        __HAL_DMA_DISABLE(&handle_HPDMA1_Channel1);                     /* 关闭JPEG数据输出DMA */
    }
}

/**
 * @brief       初始化硬件JPEG内核
 * @param       tjpeg       : JPEG编解码控制结构体
 * @retval      0, 成功; 1, 失败;
 */
uint8_t jpeg_core_init(jpeg_codec_typedef *tjpeg)
{
    uint8_t i;
    for (i = 0; i < JPEG_DMA_INBUF_NB; i++)
    {
        tjpeg->inbuf[i].buf = mymalloc(SRAMEX, JPEG_DMA_INBUF_LEN);

        if (tjpeg->inbuf[i].buf == NULL)
        {
            jpeg_core_destroy(tjpeg);
            return 1;
        }
    }

    JPEG->CR = 0;                       /* 先清零 */
    JPEG->CR |= 1 << 0;                 /* 使能硬件JPEG */
    JPEG->CONFR0 &= ~(1 << 0);          /* 停止JPEG编解码进程 */
    JPEG->CR |= 1 << 11;                /* 输入DMA使能 */
    JPEG->CR |= 1 << 12;                /* 输出DMA使能 */
    JPEG->CR |= 1 << 13;                /* 清空输入fifo */
    JPEG->CR |= 1 << 14;                /* 清空输出fifo */
    JPEG->CFR = 3 << 5;                 /* 清空标志 */
    HAL_NVIC_SetPriority(JPEG_IRQn, 1, 0);  /* JPEG中断服务函数 1,3 */
    HAL_NVIC_EnableIRQ(JPEG_IRQn);
    JPEG->CONFR1 |= 1 << 8;             /* 使能header处理 */
    return 0;
}


/**
 * @brief       关闭硬件JPEG内核,并释放内存
 * @param       tjpeg       : JPEG编解码控制结构体
 * @retval      无
 */
void jpeg_core_destroy(jpeg_codec_typedef *tjpeg)
{
    uint8_t i;
    jpeg_dma_stop();                            /* 停止HPDMA传输 */

    for (i = 0; i < JPEG_DMA_INBUF_NB; i++)
    {
        myfree(SRAMEX, tjpeg->inbuf[i].buf);    /* 释放内存 */
    }

    for (i = 0; i < JPEG_DMA_OUTBUF_NB; i++)
    {
        myfree(SRAMIN, tjpeg->outbuf[i].buf);   /* 释放内存 */
    }
}

/**
 * @brief       初始化硬件JPEG解码器
 * @param       tjpeg       : JPEG编解码控制结构体
 * @retval      无
 */
void jpeg_decode_init(jpeg_codec_typedef *tjpeg)
{
    uint8_t i;
    tjpeg->inbuf_read_ptr = 0;
    tjpeg->inbuf_write_ptr = 0;
    tjpeg->indma_pause = 0;
    tjpeg->outbuf_read_ptr = 0;
    tjpeg->outbuf_write_ptr = 0;
    tjpeg->outdma_pause = 0;
    tjpeg->state = JPEG_STATE_NOHEADER;         /* 图片解码结束标志 */
    
    for (i = 0; i < JPEG_DMA_INBUF_NB; i++)
    {
        tjpeg->inbuf[i].sta = 0;
        tjpeg->inbuf[i].size = 0;
    }

    for (i = 0; i < JPEG_DMA_OUTBUF_NB; i++)
    {
        tjpeg->outbuf[i].sta = 0;
        tjpeg->outbuf[i].size = 0;
    }
    __HAL_DMA_DISABLE(&handle_HPDMA1_Channel0);
    __HAL_DMA_DISABLE(&handle_HPDMA1_Channel1);
    handle_HPDMA1_Channel0.Instance->CFCR |= (0X7F << 8);    /* 中断标志清零 */
    handle_HPDMA1_Channel1.Instance->CFCR |= (0X7F << 8);   /* 中断标志清零 */
    
    JPEG->CONFR1 |= 1 << 3;                             /* 硬件JPEG解码模式 */
    JPEG->CONFR0 &= ~(1 << 0);                          /* 停止JPEG编解码进程 */
    JPEG->CR &= ~(0X3F << 1);                           /* 关闭所有中断 */
    JPEG->CR |= 1 << 13;                                /* 清空输入fifo */
    JPEG->CR |= 1 << 14;                                /* 清空输出fifo */
    JPEG->CR |= 1 << 6;                                 /* 使能Jpeg Header解码完成中断 */
    JPEG->CR |= 1 << 5;                                 /* 使能解码完成中断 */
    JPEG->CFR = 3 << 5;                                 /* 清空标志 */
    JPEG->CONFR0 |= 1 << 0;                             /* 使能JPEG编解码进程 */
}



/**
 * @brief       启动 jpeg in HPDMA, 开始解码JPEG
 * @param       无
 * @retval      无
 */
void jpeg_in_dma_start(void)
{
    __HAL_DMA_ENABLE(&handle_HPDMA1_Channel0);   /* 打开JPEG数据输入HPDMA */
    JPEG->CR |= 1 << 11;                         /* JPEG IN HPDMA使能 */
}

/**
 * @brief       启动 jpeg out HPDMA, 开始输出YUV数据
 * @param       无
 * @retval      无
 */
void jpeg_out_dma_start(void)
{
    __HAL_DMA_ENABLE(&handle_HPDMA1_Channel1);  /* 打开JPEG数据输出HPDMA */
    JPEG->CR |= 1 << 12;                    /* JPEG OUT HPDMA使能 */
}

/**
 * @brief       停止JPEG HPDMA解码过程
 * @param       无
 * @retval      无
 */
void jpeg_dma_stop(void)
{
    JPEG->CR &= ~(3 << 11);             /* JPEG IN&OUT HPDMA禁止 */
    JPEG->CONFR0 &= ~(1 << 0);          /* 停止JPEG编解码进程 */
    JPEG->CR &= ~(0X3F << 1);           /* 关闭所有中断 */
    JPEG->CFR = 3 << 5;                 /* 清空标志 */
}

/**
 * @brief       暂停HPDMA IN过程
 * @param       无
 * @retval      无
 */
void jpeg_in_dma_pause(void)
{
    JPEG->CR &= ~(1 << 11);             /* 暂停JPEG的HPDMA IN */
}

/**
 * @brief       暂停HPDMA OUT过程
 * @param       无
 * @retval      无
 */
void jpeg_out_dma_pause(void)
{
    JPEG->CR &= ~(1 << 12);             /* 暂停JPEG的HPDMA OUT */
}

/**
 * @brief       恢复HPDMA IN过程
 * @param       memaddr     : 存储区首地址
 * @param       memlen      : 要传输数据长度(以字节为单位)
 * @retval      无
 */
void jpeg_in_dma_resume(uint32_t memaddr, uint32_t memlen)
{
    if (memlen % 4)
    {
        memlen += 4 - memlen % 4;                       /* 扩展到4的倍数 */
    }
    handle_HPDMA1_Channel0.Instance->CFCR = (0X7F << 8);     /* 中断标志清零 */
    handle_HPDMA1_Channel0.Instance->CBR1 = memlen;          /* 传输长度为memlen */
    handle_HPDMA1_Channel0.Instance->CSAR = memaddr;         /* memaddr作为源地址 */
    handle_HPDMA1_Channel0.Instance->CCR |= 1 << 0;          /* 使能HPDMA通道7的传输 */
    JPEG->CR |= 1 << 11;                                /* 恢复JPEG HPDMA IN */
}

/**
 * @brief       恢复HPDMA OUT过程
 * @param       memaddr     : 存储区首地址
 * @param       memlen      : 要传输数据长度(以字节为单位)
 * @retval      无
 */
void jpeg_out_dma_resume(uint32_t memaddr, uint32_t memlen)
{
    if (memlen % 4)
    {
        memlen += 4 - memlen % 4;    /* 扩展到4的倍数 */
    }
    handle_HPDMA1_Channel1.Instance->CFCR = (0X7F << 8);        /* 中断标志清零 */
    handle_HPDMA1_Channel1.Instance->CBR1 = memlen;             /* 传输长度为memlen */
    handle_HPDMA1_Channel1.Instance->CDAR = memaddr;            /* memaddr作为源地址 */
    handle_HPDMA1_Channel1.Instance->CCR |= 1 << 0;             /* 使能HPDMA通道6的传输 */
    JPEG->CR |= 1 << 12;                                    /* 恢复JPEG HPDMA OUT */
}

/**
 * @brief       获取图像信息
 * @param       tjpeg       : JPEG编解码控制结构体
 * @retval      无
 */
void jpeg_get_info(jpeg_codec_typedef *tjpeg)
{
    uint32_t yblockNb, cBblockNb, cRblockNb;

    switch (JPEG->CONFR1 & 0X03)
    {
        case 0:          /* grayscale,1 color component */
            tjpeg->Conf.ColorSpace = JPEG_GRAYSCALE_COLORSPACE;
            break;

        case 2:          /* YUV/RGB,3 color component */
            tjpeg->Conf.ColorSpace = JPEG_YCBCR_COLORSPACE;
            break;

        case 3:          /* CMYK,4 color component */
            tjpeg->Conf.ColorSpace = JPEG_CMYK_COLORSPACE;
            break;
    }

    tjpeg->Conf.ImageHeight = (JPEG->CONFR1 & 0XFFFF0000) >> 16;    /* 获得图像高度 */
    tjpeg->Conf.ImageWidth = (JPEG->CONFR3 & 0XFFFF0000) >> 16;     /* 获得图像宽度 */

    if ((tjpeg->Conf.ColorSpace == JPEG_YCBCR_COLORSPACE) || (tjpeg->Conf.ColorSpace == JPEG_CMYK_COLORSPACE))
    {
        yblockNb  = (JPEG->CONFR4 & (0XF << 4)) >> 4;
        cBblockNb = (JPEG->CONFR5 & (0XF << 4)) >> 4;
        cRblockNb = (JPEG->CONFR6 & (0XF << 4)) >> 4;

        if ((yblockNb == 1) && (cBblockNb == 0) && (cRblockNb == 0))
        {
            tjpeg->Conf.ChromaSubsampling = JPEG_422_SUBSAMPLING; /* 16x8 block */
        }
        else if ((yblockNb == 0) && (cBblockNb == 0) && (cRblockNb == 0))
        {
            tjpeg->Conf.ChromaSubsampling = JPEG_444_SUBSAMPLING;
        }
        else if ((yblockNb == 3) && (cBblockNb == 0) && (cRblockNb == 0))
        {
            tjpeg->Conf.ChromaSubsampling = JPEG_420_SUBSAMPLING;
        }
        else 
        {
            tjpeg->Conf.ChromaSubsampling = JPEG_444_SUBSAMPLING;
        }
    }
    else 
    {
        tjpeg->Conf.ChromaSubsampling = JPEG_444_SUBSAMPLING;       /* 默认用4:4:4 */
    }

    tjpeg->Conf.ImageQuality = 0;   /* 图像质量参数在整个图片的最末尾,刚开始的时候,是无法获取的,所以直接设置为0 */
}

/**
 * @brief       得到JPEG图像质量
 * @note        在解码完成后,可以调用并获得正确的结果.
 * @param       无
 * @retval      图像质量, 0~100
 */
uint8_t jpeg_get_quality(void)
{
    uint32_t quality = 0;
    uint32_t quantRow, quantVal, scale, i, j;
    uint32_t *tableAddress = (uint32_t *)JPEG->QMEM0;
    i = 0;

    while (i < JPEG_QUANT_TABLE_SIZE)
    {
        quantRow = *tableAddress;

        for (j = 0; j < 4; j++)
        {
            quantVal = (quantRow >> (8 * j)) & 0xFF;

            if (quantVal == 1)
            {
                quality += 100;       /* 100% */
            }
            else
            {
                scale = (quantVal * 100) / ((uint32_t)JPEG_LUM_QuantTable[JPEG_ZIGZAG_ORDER[i + j]]);

                if (scale <= 100)
                {
                    quality += (200 - scale) / 2;
                }
                else
                {
                    quality += 5000 / scale;
                }
            }
        }

        i += 4;
        tableAddress++;
    }

    return (quality / ((uint32_t)64));
}

/**
 * @brief       将YUV数据转换成RGB数据
 * @note        利用DMA2D, 将JPEG解码的YUV数据转换成RGB数据, 全硬件完成, 速度非常快
 * @param       tjpeg       : JPEG编解码控制结构体
 * @param       pdst        : 输出数组首地址
 * @retval      0, 成功; 1, 超时,失败;
 */
uint8_t jpeg_dma2d_yuv2rgb_conversion(jpeg_codec_typedef *tjpeg, uint32_t *pdst)
{
    uint32_t regval = 0;
    uint32_t cm = 0;                    /* 采样方式 */
    uint32_t timeout = 0;
    uint32_t destination = 0;

    if (tjpeg->Conf.ChromaSubsampling == JPEG_420_SUBSAMPLING)
    {
        cm = DMA2D_CSS_420;             /* YUV420转RGB */
    }

    else if (tjpeg->Conf.ChromaSubsampling == JPEG_422_SUBSAMPLING)
    {
        cm = DMA2D_CSS_422;             /* YUV422转RGB */
    }
    else if (tjpeg->Conf.ChromaSubsampling == JPEG_444_SUBSAMPLING)
    {
        cm = DMA2D_NO_CSS;              /* YUV444转RGB */
    }

    destination = (uint32_t)pdst + (tjpeg->yuvblk_curheight * tjpeg->Conf.ImageWidth) * 2;  /* 计算目标地址的首地址 */

    __HAL_RCC_DMA2D_CLK_ENABLE();       /* 使能DMA2D时钟 */
    __HAL_RCC_DMA2D_FORCE_RESET();      /* 复位DMA2D */
    __HAL_RCC_DMA2D_RELEASE_RESET();    /* 结束复位 */
    DMA2D->CR &= ~(1 << 0);             /* 先停止DMA2D */
    DMA2D->CR = 1 << 16;                /* MODE[1:0]=01,存储器到存储器,带PFC模式 */
    DMA2D->OPFCCR = 2 << 0;             /* CM[2:0]=010,输出为RGB565格式 */
    DMA2D->OOR = 0;                     /* 设置行偏移为0 */
    DMA2D->IFCR |= 1 << 1;              /* 清除传输完成标志 */
    regval = 11 << 0;                   /* CM[3:0]=1011,输入数据为YCbCr格式 */
    regval |= cm << 18;                 /* CSS[1:0]=cm,Chroma Sub-Sampling:0,4:4:4;1,4:2:2;2,4:2:0 */
    DMA2D->FGPFCCR = regval;            /* 设置FGPCCR寄存器 */
    DMA2D->FGOR = 0;                    /* 前景层行偏移为0 */
    DMA2D->NLR = tjpeg->yuvblk_height | (tjpeg->Conf.ImageWidth << 16); /* 设定行数寄存器 */

    DMA2D->OMAR = destination;          /* 输出存储器地址 */

    DMA2D->FGMAR = (uint32_t)tjpeg->outbuf[tjpeg->outbuf_read_ptr].buf; /* 源地址 */
    DMA2D->CR |= 1 << 0;                /* 启动DMA2D */

    while ((DMA2D->ISR & (1 << 1)) == 0)/* 等待传输完成 */
    {
        timeout++;

        if (timeout > 0X1FFFFFF)
        {
            break;      /* 超时退出 */
        }
    }

    tjpeg->yuvblk_curheight += tjpeg->yuvblk_height;    /* 偏移到下一个内存地址 */
    /* YUV2RGB转码结束后,再复位一次DMA2D */
    __HAL_RCC_DMA2D_FORCE_RESET();      /* 复位DMA2D */
    __HAL_RCC_DMA2D_RELEASE_RESET();    /* 结束复位 */

    if (timeout > 0X1FFFFFF)
    {
        return 1;
    }

    return 0;
}
