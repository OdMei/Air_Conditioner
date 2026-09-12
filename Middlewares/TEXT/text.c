/**
 ****************************************************************************************************
 * @file        text.c
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2025-02-19
 * @brief       汉字显示 代码
 *              提供text_show_font和text_show_string两个函数,用于显示汉字
 * @license     Copyright (c) 2020-2032, 广州市星翼电子科技有限公司
 ****************************************************************************************************
 * @attention
 *
 * 实验平台:正点原子 STM32开发板
 * 在线视频:www.yuanzige.com
 * 技术论坛:www.openedv.com
 * 公司网址:www.alientek.com
 * 购买地址:openedv.taobao.com
 *
 * 修改说明
 * V1.0 20250219
 * 添加字库存放于SD_NAND
 *
 ****************************************************************************************************
 */

#include "string.h"
#include "text.h"
#include "./RGBLCD/rgblcd.h"
#include "malloc.h"
#include "./UART/uart.h"
#include "./SD_NAND/sd_nand.h"


/**
 * @brief       UTF-8转UNICODE编码
 * @param       utf8  : 字符串地址
 * @retval      字符的UNICODE编码
 */
uint32_t utf8_to_unicode(char *utf8)
{
    uint32_t unicode = 0;
    /* 判断UTF-8编码的字节数 */
    if ((utf8[0] & 0x80) == 0x00) 
    {
        /* 1字节编码 */
        unicode = utf8[0];
    } 
    else if ((utf8[0] & 0xE0) == 0xC0) 
    {
        /* 2字节编码 */
        unicode = ((utf8[0] & 0x1F) << 6) | (utf8[1] & 0x3F);
    } 
    else if ((utf8[0] & 0xF0) == 0xE0) 
    {
        /* 3字节编码 */
        unicode = ((utf8[0] & 0x0F) << 12) | ((utf8[1] & 0x3F) << 6) | (utf8[2] & 0x3F);
    }
     else if ((utf8[0] & 0xF8) == 0xF0) 
     {
        /* 4字节编码 */
        unicode = ((utf8[0] & 0x07) << 18) | ((utf8[1] & 0x3F) << 12) | ((utf8[2] & 0x3F) << 6) | (utf8[3] & 0x3F);
    } else {
        /* 无效的UTF-8编码 */ 
        unicode = 0xFFFD; /* 替换字符（REPLACEMENT CHARACTER）*/
    }

    return unicode;
}

/**
 * @brief       UNICODE转GBK编码
 * @param       char_unicode  : 当前字符编码(UNICODE编码)
 *   @note      传入的参数为两个字节的UNICODE编码
 * @retval      字符的GBK编码
 */
uint16_t unicode_to_gbk(uint16_t char_unicode)
{
    uint16_t char_gbk = 0,offset;
    uint8_t *buf;
    uint32_t hi,li,i;
    uint8_t n;
    unsigned long foffset;

    buf = mymalloc(SRAMIN, 512); /* 分配512个字节空间 */
    hi = 87172 / 4 - 1;
    li = 0;
    /* Unicode转GBK */
    for(n = 16; n > 0; n--)  /* 二分法查表 */
    {
        i = (int)(li + (hi - li) / 2);
        foffset = (i*4) >> 9;  /* 到达偏移后对应的块 */
        sd_nand_read_disk(buf, ftinfo.ugbkaddr + foffset, 1);
        offset = (i*4) % 512;
        if (char_unicode == (buf[offset + 1] << 8 | buf[offset])) 
        {
            break;
        }
        if (char_unicode > (buf[offset + 1] << 8 | buf[offset]))
        {
            li = i;
        }
        else hi = i;
    }
    foffset = (i*4 +2) >> 9;
    sd_nand_read_disk(buf, ftinfo.ugbkaddr + foffset, 1);
    offset = (i*4+2) % 512;
    char_gbk = buf[offset + 1] << 8 | buf[offset];
    myfree(SRAMIN, buf); /* 释放内存 */
    return char_gbk;
}

/**
 * @brief       获取汉字点阵数据
 * @param       code  : 当前汉字编码(GBK码)
 * @param       mat   : 当前汉字点阵数据存放地址
 * @param       size  : 字体大小
 *   @note      size大小的字体,其点阵数据大小为: (size / 8 + ((size % 8) ? 1 : 0)) * (size)  字节
 * @retval      无
 */
static void text_get_hz_mat(unsigned char *code, unsigned char *mat, uint8_t size)
{
    uint16_t fdataend,blkoffset,rdata;
    uint8_t *tempbuf;
    uint8_t *ptempbuf;    /* 用于指向tempbuf的首地址 */
    unsigned char qh, ql;
    unsigned char i;
    unsigned long foffset,offset;  /* 用于存放SDNAND内字库的块偏移量和块内偏移 */
    uint8_t csize = (size / 8 + ((size % 8) ? 1 : 0)) * (size); /* 得到字体一个字符对应点阵集所占的字节数 */
    tempbuf = mymalloc(SRAMIN, 512); /* 分配512个字节空间 */
    ptempbuf = tempbuf;   /* 指向tempbuf的首地址 */
    qh = *code;
    ql = *(++code);
    /* GBK字库偏移计算 */
    if (qh < 0x81 || ql < 0x40 || ql == 0xff || qh == 0xff)   /* 非常用汉字 */
    {
        for (i = 0; i < csize; i++)
        {
            *mat++ = 0x00;  /* 填充满格 */
        }

        return;     /* 结束访问 */
    }

    if (ql < 0x7f)
    {
        ql -= 0x40; /* 注意! */
    }
    else
    {
        ql -= 0x41;
    }

    qh -= 0x81;
    offset = ((unsigned long)190 * qh + ql) * csize; /* 得到字库中的字节偏移量 */

    foffset = offset >> 9;          /* 得到汉字在SDNAND起始块 */
    blkoffset = offset % 512;       /* 字符数组块内偏移值 */
    fdataend = blkoffset + csize;   /* 用于计算字符数组是否跨扇区存储 */

    switch (size)
    {
        case 12:
            if(fdataend <= 512)
            {
                sd_nand_read_disk(tempbuf,foffset + ftinfo.f12addr, 1);
                tempbuf += blkoffset;
                for (i = 0; i < csize; i++)
                {
                    *mat++ = *tempbuf++;  /* 填充满格 */
                }
            }
            else
            {
                rdata = fdataend - 512;
                sd_nand_read_disk(tempbuf,foffset + ftinfo.f12addr, 1);
                tempbuf += blkoffset;
                for (i = 0; i < csize; i++)
                {
                    if (i == (csize - rdata)) /* 当前块数据读完 */
                    {
                        tempbuf = ptempbuf;  /* 地址偏移回首地址 */
                        sd_nand_read_disk(tempbuf,foffset + ftinfo.f12addr + 1, 1); /* 读下个块的数据 */
                    }  
                    *mat++ = *tempbuf++;  /* 填充满格 */                
                }
            }
            break;

        case 16:
            if(fdataend <= 512)
            {
                sd_nand_read_disk(tempbuf,foffset + ftinfo.f16addr, 1);
                tempbuf += blkoffset;
                for (i = 0; i < csize; i++)
                {
                    *mat++ = *tempbuf++;  /* 填充满格 */
                }
            }
            else
            {
                rdata = fdataend - 512;
                sd_nand_read_disk(tempbuf,foffset + ftinfo.f16addr, 1);
                tempbuf += blkoffset;
                for (i = 0; i < csize; i++)
                {
                    if (i == (csize - rdata)) /* 当前块数据读完 */
                    {
                        tempbuf = ptempbuf;  /* 地址偏移回首地址 */
                        sd_nand_read_disk(tempbuf,foffset + ftinfo.f16addr + 1, 1); /* 读下个块的数据 */
                    }  
                    *mat++ = *tempbuf++;  /* 填充满格 */                
                }
            }
            break;

        case 24:
            if(fdataend <= 512)
            {
                sd_nand_read_disk(tempbuf,foffset + ftinfo.f24addr, 1);
                tempbuf += blkoffset;
                for (i = 0; i < csize; i++)
                {
                    *mat++ = *tempbuf++;  /* 填充满格 */
                }
            }
            else
            {
                rdata = fdataend - 512;
                sd_nand_read_disk(tempbuf,foffset + ftinfo.f24addr, 1);
                tempbuf += blkoffset;
                for (i = 0; i < csize; i++)
                {
                    if (i == (csize - rdata)) /* 当前块数据读完 */
                    {
                        tempbuf = ptempbuf;  /* 地址偏移回首地址 */
                        sd_nand_read_disk(tempbuf,foffset + ftinfo.f24addr + 1, 1); /* 读下个块的数据 */
                    }  
                    *mat++ = *tempbuf++;  /* 填充满格 */                
                }
            }
            break;

        case 32:
            if(fdataend <= 512)
            {
                sd_nand_read_disk(tempbuf,foffset + ftinfo.f32addr, 1);
                tempbuf += blkoffset;
                for (i = 0; i < csize; i++)
                {
                    *mat++ = *tempbuf++;  /* 填充满格 */
                }
            }
            else
            {
                rdata = fdataend - 512;
                sd_nand_read_disk(tempbuf,foffset + ftinfo.f32addr, 1);
                tempbuf += blkoffset;
                for (i = 0; i < csize; i++)
                {
                    if (i == (csize - rdata)) /* 当前块数据读完 */
                    {
                        tempbuf = ptempbuf;  /* 地址偏移回首地址 */
                        sd_nand_read_disk(tempbuf,foffset + ftinfo.f32addr + 1, 1); /* 读下个块的数据 */
                    }  
                    *mat++ = *tempbuf++;  /* 填充满格 */                
                }
            }
            break;

    }
    myfree(SRAMIN, ptempbuf); /* 释放内存 */
}

/**
 * @brief       显示一个指定大小的汉字
 * @param       x,y   : 汉字的坐标
 * @param       font  : 汉字GBK码
 * @param       size  : 字体大小
 * @param       mode  : 显示模式
 *   @note              0, 正常显示(不需要显示的点,用LCD背景色填充,即g_back_color)
 *   @note              1, 叠加显示(仅显示需要显示的点, 不需要显示的点, 不做处理)
 * @param       color : 字体颜色
 * @retval      无
 */
void text_show_font(uint16_t x, uint16_t y, uint8_t *font, uint8_t size, uint8_t mode, uint16_t color)
{
    uint8_t temp, t, t1;
    uint16_t y0 = y;
    uint8_t *dzk;
    uint8_t csize = (size / 8 + ((size % 8) ? 1 : 0)) * (size);     /* 得到字体一个字符对应点阵集所占的字节数 */

    if (size != 12 && size != 16 && size != 24 && size != 32)
    {
        return;     /* 不支持的size */
    }

    dzk = mymalloc(SRAMIN, size);       /* 申请内存 */

    if (dzk == 0) return;               /* 内存不够了 */

    text_get_hz_mat(font, dzk, size);   /* 得到相应大小的点阵数据 */

    for (t = 0; t < csize; t++)
    {
        temp = dzk[t];                  /* 得到点阵数据 */

        for (t1 = 0; t1 < 8; t1++)
        {
            if (temp & 0x80)
            {
                rgblcd_draw_point(x, y, color);        /* 画需要显示的点 */
            }
            else if (mode == 0)     /* 如果非叠加模式, 不需要显示的点,用背景色填充 */
            {
                rgblcd_draw_point(x, y, g_back_color);  /* 填充背景色 */
            }

            temp <<= 1;
            y++;

            if ((y - y0) == size)
            {
                y = y0;
                x++;
                break;
            }
        }
    }

    myfree(SRAMIN, dzk);    /* 释放内存 */
}

/**
 * @brief       在指定位置开始显示一个字符串
 *   @note      该函数支持自动换行
 * @param       x,y   : 起始坐标
 * @param       width : 显示区域宽度
 * @param       height: 显示区域高度
 * @param       str   : 字符串
 * @param       size  : 字体大小
 * @param       mode  : 显示模式
 *   @note              0, 正常显示(不需要显示的点,用LCD背景色填充,即g_back_color)
 *   @note              1, 叠加显示(仅显示需要显示的点, 不需要显示的点, 不做处理)
 * @param       encode : 字符串编码方式：UTF-8:0 GBK:1
 * @param       color : 字体颜色
 * @retval      无
 */
void text_show_string(uint16_t x, uint16_t y, uint16_t width, uint16_t height, char *str, uint8_t size, uint8_t mode, uint8_t encode, uint16_t color)
{
    uint16_t x0 = x;
    uint16_t y0 = y;
    uint8_t bHz = 0;                /* 字符或者中文 */
    uint16_t char_gbk = 0;
    uint32_t unicode = 0;
    uint8_t hz_gbk[2];
    uint8_t *pstr = (uint8_t *)str; /* 指向char*型字符串首地址 */

    while (*pstr != 0)   /* 数据未结束 */
    {
        if (!bHz)
        {
            if (*pstr > 0x80)   /* 中文 */
            {
                bHz = 1;    /* 标记是中文 */
            }
            else            /* 字符 */
            {
                if (x > (x0 + width - size / 2))    /* 换行 */
                {
                    y += size;
                    x = x0;
                }

                if (y > (y0 + height - size))break; /* 越界返回 */

                if (*pstr == 13)   /* 换行符号 */
                {
                    y += size;
                    x = x0;
                    pstr++;
                }
                else
                {
                    rgblcd_show_char(x, y, *pstr, size, mode, color);   /* 有效部分写入 */
                }

                pstr++;

                x += size / 2;  /* 英文字符宽度, 为中文汉字宽度的一半 */
            }
        }
        else     /* 中文 */
        {
            bHz = 0; /* 有汉字库 */

            if (x > (x0 + width - size))   /* 换行 */
            {
                y += size;
                x = x0;
            }

            if (y > (y0 + height - size))break; /* 越界返回 */

            if (encode)
            {
                /* GBK编码，不需要转码 */
                text_show_font(x, y, pstr, size, mode, color); /* 显示这个汉字,空心显示 */
                pstr += 2;
            }
            else
            {
                /* UTF-8编码，需要先转码 */
                unicode = utf8_to_unicode((char *)pstr); 
                char_gbk = unicode_to_gbk((uint16_t)unicode);
                /* 得到汉字的GBK编码 */
                hz_gbk[1] = char_gbk & 0xff;
                hz_gbk[0] = char_gbk >> 8 & 0xff;
                text_show_font(x, y, hz_gbk, size, mode, color); /* 显示这个汉字,空心显示 */
                pstr += 3;
            }
            x += size; /* 下一个汉字偏移 */
        }
    }
}


/**
 * @brief       在指定宽度的中间显示字符串
 *   @note      如果字符长度超过了len,则用text_show_string_middle显示
 * @param       x,y   : 起始坐标
 * @param       str   : 字符串
 * @param       size  : 字体大小
 * @param       width : 显示区域宽度
 * @param       color : 字体颜色
 * @retval      无
 */
void text_show_string_middle(uint16_t x, uint16_t y, char *str, uint8_t size, uint16_t width, uint16_t color)
{
    uint16_t strlenth = 0;
    strlenth = strlen((const char *)str);
    strlenth *= size / 2;

    if (strlenth > width) /* 超过了, 不能居中显示 */
    {
        text_show_string(x, y, rgblcddev.width, rgblcddev.height, str, size, 1, 0, color);
    }
    else
    {
        strlenth = (width - strlenth) / 2;
        text_show_string(strlenth + x, y, rgblcddev.width, rgblcddev.height, str, size, 1, 0, color);
    }
}













