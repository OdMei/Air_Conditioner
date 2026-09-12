/**
 ****************************************************************************************************
 * @file        sd_nand.h
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2025-01-13
 * @brief       SD NAND驱动代码
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

#ifndef __SD_NAND_H
#define __SD_NAND_H

#include "main.h"

/**
 * SD NAND区域划分（TotalBlkNum为SD NAND的总逻辑块数量）
 * |  Name |     Start Block     |   Number of Blocks  |
 * | FatFs |          0          | TotalBlkNum - 12313 |
 * |  Font | TotalBlkNum - 12313 |        12313        |
 */
#define SD_NAND_FONT_BLK_NUM 12313

/* SD NAND操作超时时间定义 */
#define SD_NAND_TIMEOUT ((uint32_t)0x00100000)

/* 变量导出 */
extern HAL_SD_CardInfoTypeDef g_sd_nand_info_struct;
extern HAL_SD_CardCIDTypeDef g_sd_nand_cid_struct;

/* 函数声明 */
uint8_t sd_nand_init(void);                                                 /* 初始化SD NAND */
uint8_t sd_nand_read_disk(uint8_t *buf, uint32_t address, uint32_t count);  /* 读SD NAND指定数量块的数据 */
uint8_t sd_nand_write_disk(uint8_t *buf, uint32_t address, uint32_t count); /* 写SD NAND指定数量块的数据 */

#endif
