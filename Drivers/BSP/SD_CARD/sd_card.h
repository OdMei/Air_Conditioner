/**
 ****************************************************************************************************
 * @file        sd_card.h
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2025-01-13
 * @brief       SD卡驱动代码
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

#ifndef __SD_CARD_H
#define __SD_CARD_H

#include "main.h"

/* SD卡操作超时时间定义 */
#define SD_CARD_TIMEOUT ((uint32_t)0x00100000)

/* 变量导出 */
extern HAL_SD_CardInfoTypeDef g_sd_card_info_struct;
extern HAL_SD_CardCIDTypeDef g_sd_card_cid_struct;

/* 函数声明 */
uint8_t sd_card_init(void);                                                 /* 初始化SD卡 */
uint8_t sd_card_read_disk(uint8_t *buf, uint32_t address, uint32_t count);  /* 读SD卡指定数量块的数据 */
uint8_t sd_card_write_disk(uint8_t *buf, uint32_t address, uint32_t count); /* 写SD卡指定数量块的数据 */

#endif
