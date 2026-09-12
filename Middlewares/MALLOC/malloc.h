/**
 ****************************************************************************************************
 * @file        malloc.h
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2025-01-13
 * @brief       内存管理驱动代码
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

#ifndef __MALLOC_H
#define __MALLOC_H

#include "main.h"

/* 内存池编号定义 */
#define SRAMIN      0   /* AXISRAM1~2内存池,AXISRAM1~2共2048KB */
#define SRAMEX      1   /* XSPI1 HyperRAM内存池,XSPI1 HyperRAM共32MB */

#define SRAMBANK    2   /* 定义支持的SRAM块数. */

/* 内存管理表类型定义,需保证(((1 << (sizeof(MT_TYPE) * 8)) - 1) * MEMx_BLOCK_SIZE) >= MEMx_MAX_SIZE */
#define MT_TYPE     uint32_t

/* mem1内存参数设定.mem1是N647内部的AXISRAM1~2 */
#define MEM1_BLOCK_SIZE         (64)                                                                                            /* 内存块大小为64字节 */
#define MEM1_MAX_SIZE           ((0x00100000 / (MEM1_BLOCK_SIZE + sizeof(MT_TYPE))) * MEM1_BLOCK_SIZE)                          /* AXISRAM1~2最大空闲0x00100000字节 */
#define MEM1_ALLOC_TABLE_SIZE   (MEM1_MAX_SIZE / MEM1_BLOCK_SIZE)                                                               /* 内存表大小 */

/* mem2内存参数设定.mem2是N647外部的XSPI1 HyperRAM */
#define MEM2_BLOCK_SIZE         (64)                                                                                            /* 内存块大小为64字节 */
#define MEM2_MAX_SIZE           (((0x02000000 - (2UL * 1280 * 800)) / (MEM2_BLOCK_SIZE + sizeof(MT_TYPE))) * MEM2_BLOCK_SIZE)   /* XSPI1 HyperRAM空闲空间 */
#define MEM2_ALLOC_TABLE_SIZE   (MEM2_MAX_SIZE / MEM2_BLOCK_SIZE)                                                               /* 内存表大小 */

/* 内存管理控制器 */
struct _m_mallco_dev
{
    void (*init)(uint8_t);          /* 初始化 */
    uint16_t (*perused)(uint8_t);   /* 内存使用率 */
    uint8_t *membase[SRAMBANK];     /* 内存池 管理SRAMBANK个区域的内存 */
    MT_TYPE *memmap[SRAMBANK];      /* 内存管理状态表 */
    uint8_t  memrdy[SRAMBANK];      /* 内存管理是否就绪 */
};

extern struct _m_mallco_dev mallco_dev; /* 在mallco.c里面定义 */


/* 用户调用函数 */
void my_mem_init(uint8_t memx);                          /* 内存管理初始化函数(外/内部调用) */
uint16_t my_mem_perused(uint8_t memx) ;                  /* 获得内存使用率(外/内部调用) */
void my_mem_set(void *s, uint8_t c, uint32_t count);     /* 内存设置函数 */
void my_mem_copy(void *des, void *src, uint32_t n);      /* 内存拷贝函数 */

void myfree(uint8_t memx, void *ptr);                    /* 内存释放(外部调用) */
void *mymalloc(uint8_t memx, uint32_t size);             /* 内存分配(外部调用) */
void *myrealloc(uint8_t memx, void *ptr, uint32_t size); /* 重新分配内存(外部调用) */

#endif













