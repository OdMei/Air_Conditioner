/**
 ****************************************************************************************************
 * @file        sd_card.c
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

#include "sd_card.h"

extern SD_HandleTypeDef hsd1;   /* SD句柄 */

HAL_SD_CardInfoTypeDef g_sd_card_info_struct = {0};
HAL_SD_CardCIDTypeDef g_sd_card_cid_struct = {0};

/**
 * @brief   初始化SD卡
 * @param   无
 * @retval  初始化结果
 * @arg     0: 初始化成功
 * @arg     1: 初始化失败
 */
uint8_t sd_card_init(void)
{
    if (HAL_SD_GetCardInfo(&hsd1, &g_sd_card_info_struct) != HAL_OK)
    {
        return 1;
    }

    if (HAL_SD_GetCardCID(&hsd1, &g_sd_card_cid_struct) != HAL_OK)
    {
        return 1;
    }

    return 0;
}

/**
 * @brief   读SD卡指定数量块的数据
 * @param   buf: 数据保存的起始地址
 * @param   address: 块地址
 * @param   count: 块数量
 * @retval  读取结果
 * @arg     0: 读成功
 * @arg     1: 读失败
 */
uint8_t sd_card_read_disk(uint8_t *buf, uint32_t address, uint32_t count)
{
    uint8_t res = 0;
    uint32_t timeout = SD_CARD_TIMEOUT;

    if (HAL_SD_ReadBlocks(&hsd1, buf, address, count, SD_CARD_TIMEOUT) != HAL_OK)
    {
        res = 1;
    }

    if (res == 0)
    {
        while ((HAL_SD_GetCardState(&hsd1) != HAL_SD_CARD_TRANSFER) && (--timeout != 0));
        if (timeout == 0)
        {
            res = 1;
        }
    }

    return res;
}

/**
 * @brief   写SD卡指定数量块的数据
 * @param   buf: 数据保存的起始地址
 * @param   address: 块地址
 * @param   count: 块数量
 * @retval  写入结果
 * @arg     0: 成功
 * @arg     1: 失败
 */
uint8_t sd_card_write_disk(uint8_t *buf, uint32_t address, uint32_t count)
{
    uint8_t res = 0;
    uint32_t timeout = SD_CARD_TIMEOUT;

    if (HAL_SD_WriteBlocks(&hsd1, buf, address, count, SD_CARD_TIMEOUT) != HAL_OK)
    {
        res = 1;
    }

    if (res == 0)
    {
        while ((HAL_SD_GetCardState(&hsd1) != HAL_SD_CARD_TRANSFER) && (--timeout != 0));
        if (timeout == 0)
        {
            res = 1;
        }
    }

    return res;
}
