/**
 ****************************************************************************************************
 * @file        font.c
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2025-02-20
 * @brief       字库 代码
 *              提供fonts_update_font和fonts_init用于字库更新和初始化
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
 * V1.0 20250220
 * 添加字库存放于SD_NAND
 *
 ****************************************************************************************************
 */

#include "string.h"
#include "./RGBLCD/rgblcd.h"
#include "fonts.h"
#include "malloc.h"
#include "./FATFS/source/ff.h"
#include "./UART/uart.h"
#include "./SD_NAND/sd_nand.h"

extern SD_HandleTypeDef hsd2;   /* SD句柄 */

/**
 * SD_NAND区域划分（最大扇区大小扇区对齐）
 * 字库区域占用的总扇区数大小(4个字库+unigbk表+字库信息=6302984字节,约占12313个SD_NAND的块,一个块512字节) */
#define FONTSECSIZE         12313


/* 字库存放起始地址
 * SD NAND区域划分（TotalBlkNum为SD NAND的总逻辑块数量）
 * |  Name |     Start Block     |   Number of Blocks  |
 * | FatFs |          0          | TotalBlkNum - 12313 |
 * |  Font | TotalBlkNum - 12313 |        12313        |
 * 文件系统后紧跟4个字库+UNIGBK.BIN,总大小6.01M, 12313个块,被字库占用了,不能动!
 */
#define FONTINFOADDR        g_sd_nand_info_struct.LogBlockNbr -  12313 /* TotalBlkNum - 12313 */

 
/* 用来保存字库基本信息，地址，大小等 */
_font_info ftinfo;

/* 字库存放在磁盘中的路径 */
char *const FONT_GBK_PATH[5] =
{
    "/SYSTEM/FONT/UNIGBK.BIN",      /* UNIGBK.BIN的存放位置 */
    "/SYSTEM/FONT/GBK12.FON",       /* GBK12的存放位置 */
    "/SYSTEM/FONT/GBK16.FON",       /* GBK16的存放位置 */
    "/SYSTEM/FONT/GBK24.FON",       /* GBK24的存放位置 */
    "/SYSTEM/FONT/GBK32.FON",       /* GBK32的存放位置 */
};

/* 更新时的提示信息 */
char *const FONT_UPDATE_REMIND_TBL[5] =
{
    "Updating UNIGBK.BIN",          /* 提示正在更新UNIGBK.bin */
    "Updating GBK12.FON ",          /* 提示正在更新GBK12 */
    "Updating GBK16.FON ",          /* 提示正在更新GBK16 */
    "Updating GBK24.FON ",          /* 提示正在更新GBK24 */
    "Updating GBK32.FON ",          /* 提示正在更新GBK32 */
};

/**
 * @brief       显示当前字体更新进度
 * @param       x, y    : 坐标
 * @param       size    : 字体大小
 * @param       totsize : 整个文件大小
 * @param       pos     : 当前文件指针位置
 * @param       color   : 字体颜色
 * @retval      无
 */
static void fonts_progress_show(uint16_t x, uint16_t y, uint8_t size, uint32_t totsize, uint32_t pos, uint16_t color)
{
    float prog;
    uint8_t t = 0XFF;
    prog = (float)pos / totsize;
    prog *= 100;

    if (t != prog)
    {
        rgblcd_show_string(x + 3 * size / 2, y, 240, 320, size, "%", color);
        t = prog;

        if (t > 100)t = 100;

        rgblcd_show_num(x, y, t, 3, size, color);  /* 显示数值 */
    }
}

/**
 * @brief       更新某一个字库
 * @param       x, y    : 提示信息的显示地址
 * @param       size    : 提示信息字体大小
 * @param       fpath   : 字体路径
 * @param       fx      : 更新的内容
 *   @arg                 0, ungbk;
 *   @Arg                 1, gbk12;
 *   @arg                 2, gbk16;
 *   @arg                 3, gbk24;
 *   @arg                 4, gbk32;
 * @param       color   : 字体颜色
 * @retval      0, 成功; 其他, 错误代码;
 */
static uint8_t fonts_update_fontx(uint16_t x, uint16_t y, uint8_t size, uint8_t *fpath, uint8_t fx, uint16_t color)
{
    uint32_t flashaddr = 0;
    FIL *fftemp;
    uint8_t *tempbuf;
    FRESULT res;
    uint16_t bread;
    uint32_t offx = 0;
    uint8_t rval = 0;
    fftemp = (FIL *)mymalloc(SRAMIN, sizeof(FIL));  /* 分配内存 */

    if (fftemp == NULL)rval = 1;

    tempbuf = mymalloc(SRAMIN, 4096);               /* 分配4096个字节空间 */

    if (tempbuf == NULL)rval = 1;

    res = f_open(fftemp, (const TCHAR *)fpath, FA_READ);

    if (res)rval = 2;   /* 打开文件失败 */

    if (rval == 0)
    {
        switch (fx)
        {
            case 0: /* 更新 UNIGBK.BIN */
                ftinfo.ugbkaddr = FONTINFOADDR + 1;    /* FONTINFOADDR地址由信息头独占，FONTINFOADDR+1紧跟UNIGBK转换码表 */ 
                ftinfo.ugbksize = fftemp->obj.objsize;              /* UNIGBK大小 */
                flashaddr = ftinfo.ugbkaddr;
                break;

            case 1: /* 更新 GBK12.BIN */
                if (ftinfo.ugbksize % 512)            /* 计算UNIGBK.BIN需要几个块（512字节）存放，如果取余不等于0，需要多一个块存放后续数据 */
                {
                    ftinfo.f12addr = ftinfo.ugbkaddr + (ftinfo.ugbksize >> 9) + 1; /* UNIGBK之后，紧跟GBK12字库,ftinfo.ugbksize是字节大小，sdnand是按扇区读写 */
                }
                else
                {
                    ftinfo.f12addr = ftinfo.ugbkaddr + (ftinfo.ugbksize >> 9); 
                }
                ftinfo.gbk12size = fftemp->obj.objsize;             /* GBK12字库大小 */
                flashaddr = ftinfo.f12addr;                         /* GBK12的起始地址 */
                break;

            case 2: /* 更新 GBK16.BIN */
                if (ftinfo.gbk12size % 512)
                {
                    ftinfo.f16addr = ftinfo.f12addr + (ftinfo.gbk12size>> 9) +1; /* GBK12之后，紧跟GBK16字库 */
                }
                else
                {
                    ftinfo.f16addr = ftinfo.f12addr + (ftinfo.gbk12size>> 9); /* GBK12之后，紧跟GBK16字库 */
                }
                ftinfo.gbk16size = fftemp->obj.objsize;             /* GBK16字库大小 */
                flashaddr = ftinfo.f16addr;                         /* GBK16的起始地址 */
                break;

            case 3: /* 更新 GBK24.BIN */
                if (ftinfo.gbk16size % 512)
                {
                    ftinfo.f24addr = ftinfo.f16addr + (ftinfo.gbk16size >> 9) + 1; /* GBK16之后，紧跟GBK24字库 */
                }
                else
                {
                    ftinfo.f24addr = ftinfo.f16addr + (ftinfo.gbk16size >> 9); /* GBK16之后，紧跟GBK24字库 */
                }
                ftinfo.gbk24size = fftemp->obj.objsize;             /* GBK24字库大小 */
                flashaddr = ftinfo.f24addr;                         /* GBK24的起始地址 */
                break;

            case 4: /* 更新 GBK32.BIN */
                if (ftinfo.gbk24size % 512)
                {
                    ftinfo.f32addr = ftinfo.f24addr + (ftinfo.gbk24size >> 9) + 1; /* GBK24之后，紧跟GBK32字库 */
                }
                else
                {
                    ftinfo.f32addr = ftinfo.f24addr + (ftinfo.gbk24size >> 9); /* GBK24之后，紧跟GBK32字库 */
                }
                ftinfo.gbk32size = fftemp->obj.objsize;             /* GBK32字库大小 */
                flashaddr = ftinfo.f32addr;                         /* GBK32的起始地址 */
                break;
        }

        while (res == FR_OK)   /* 死循环执行 */
        {
            res = f_read(fftemp, tempbuf, 4096, (UINT *)&bread);    /* 读取数据 */

            if (res != FR_OK)
			{
                break;    /* 执行错误 */
			}   
            if (bread == 4096)
            {
                sd_nand_write_disk(tempbuf, offx + flashaddr, 8);   /* 从0开始写入bread个数据 */
                offx += 8;  /* 4096即偏移8个块 */
                fonts_progress_show(x, y, size, fftemp->obj.objsize >> 9, offx, color);    /* 进度显示 */
            }
            else  /* 最后一组数据 */
            {
            	if(bread % 512) sd_nand_write_disk(tempbuf, offx + flashaddr, (bread >> 9) + 1 );   /* 需要多写个扇区*/
            	else sd_nand_write_disk(tempbuf, offx + flashaddr, bread >> 9);   /* 从0开始写入bread个数据 */
            	fonts_progress_show(x, y, size, fftemp->obj.objsize >> 9, offx, color);    /* 进度显示 */
            	break;    /* 读完了. */
            }
        }

        f_close(fftemp);
    }

    myfree(SRAMIN, fftemp);     /* 释放内存 */
    myfree(SRAMIN, tempbuf);    /* 释放内存 */
    return res;
}

/**
 * @brief       更新字体文件
 *   @note      所有字库一起更新(UNIGBK,GBK12,GBK16,GBK24,GBK32)
 * @param       x, y    : 提示信息的显示地址
 * @param       size    : 提示信息字体大小
 * @param       src     : 字库来源磁盘
 *   @arg                 "0:", SD卡;
 *   @Arg                 "1:", FLASH盘
 *   @arg                 "2:", U盘
 * @param       color   : 字体颜色
 * @retval      0, 成功; 其他, 错误代码;
 */
uint8_t fonts_update_font(uint16_t x, uint16_t y, uint8_t size, uint8_t *src, uint16_t color)
{
    uint8_t *pname;
    uint8_t res = 0;
    uint16_t i;
    FIL *fftemp;
    uint8_t rval = 0;
    uint8_t *tempbuf;
    uint16_t index;
    res = 0XFF;
    ftinfo.fontok = 0XFF;
    pname = mymalloc(SRAMIN, 100);  /* 申请100字节内存 */
    fftemp = (FIL *)mymalloc(SRAMIN, sizeof(FIL));  /* 分配内存 */
    tempbuf = mymalloc(SRAMIN, 512);

    if (pname == NULL || fftemp == NULL)
    {
        myfree(SRAMIN, fftemp);
        myfree(SRAMIN, pname);
        myfree(SRAMIN, tempbuf);
        return 5;   /* 内存申请失败 */
    }

    for (i = 0; i < 5; i++) /* 先查找SD卡中文件UNIGBK,GBK12,GBK16,GBK24,GBK32是否正常 */
    {
        strcpy((char *)pname, (char *)src);                 /* copy src内容到pname */
        strcat((char *)pname, (char *)FONT_GBK_PATH[i]);    /* 追加具体文件路径 */
        res = f_open(fftemp, (const TCHAR *)pname, FA_READ);/* 尝试打开 */

        if (res)
        {
            rval |= 1 << 7; /* 标记打开文件失败 */
            break;          /* 出错了,直接退出 */
        }
    }

    myfree(SRAMIN, fftemp); /* 释放内存 */

    if (rval == 0)          /* 字库文件都存在. */
    {

        for (i = 0; i < 5; i++) /* 依次更新UNIGBK,GBK12,GBK16,GBK24,GBK32 */
        {
            rgblcd_show_string(x, y, 240, 320, size, FONT_UPDATE_REMIND_TBL[i], color);
            strcpy((char *)pname, (char *)src);             /* copy src内容到pname */
            strcat((char *)pname, (char *)FONT_GBK_PATH[i]);/* 追加具体文件路径 */
            res = fonts_update_fontx(x + 20 * size / 2, y, size, pname, i, color);    /* 更新字库 */

            if (res)
            {
                myfree(SRAMIN, pname);
                myfree(SRAMIN, tempbuf);
                return 1 + i;
            }
        }

        /* 全部更新好了 */
        ftinfo.fontok = 0XAA;
        for (index = 0; index < sizeof(ftinfo); index++)
        {
            tempbuf[index] = ((uint8_t *)&ftinfo)[index];
        }
        sd_nand_write_disk(tempbuf, FONTINFOADDR, 1);    /* 保存字库信息 */
    }

    myfree(SRAMIN, pname);  /* 释放内存 */
    myfree(SRAMIN, tempbuf);
    return rval;            /* 无错误. */
}

/**
 * @brief       初始化字体
 * @param       无
 * @retval      0, 字库完好; 其他, 字库丢失;
 */
uint8_t fonts_init(void)
{
    uint8_t t = 0;
    uint8_t *tempbuf;
    uint16_t index;

    tempbuf = mymalloc(SRAMIN, 512);

    while (t < 10)  /* 连续读取10次,都是错误,说明确实是有问题,得更新字库了 */
    {
        t++;
        sd_nand_read_disk(tempbuf, FONTINFOADDR, 1); /* 读出ftinfo结构体数据 */
        for (index = 0; index < sizeof(ftinfo); index++)
        {
            ((uint8_t *)&ftinfo)[index] = tempbuf[index];
        }

        if (ftinfo.fontok == 0XAA)
        {
            break;
        }
        
        HAL_Delay(20);
    }

    myfree(SRAMIN, tempbuf);

    if (ftinfo.fontok != 0XAA)
    {
        return 1;
    }
    
    return 0;
}












