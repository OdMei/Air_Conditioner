/**
 ****************************************************************************************************
 * @file        fdcan.c
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2025-01-13
 * @brief       FDCAN驱动代码
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

#include "fdcan.h"

extern FDCAN_HandleTypeDef hfdcan3; /* FDCAN句柄 */

/**
 * @brief   初始化FDCAN
 * @param   无
 * @retval  初始化结果
 * @arg     0: 初始化成功
 * @arg     1: 初始化失败
 */
uint8_t fdcan_init(void)
{
    FDCAN_FilterTypeDef fdcan_filter_struct = {0};

    fdcan_filter_struct.IdType = FDCAN_STANDARD_ID;
    fdcan_filter_struct.FilterIndex = 0;
    fdcan_filter_struct.FilterType = FDCAN_FILTER_MASK;
    fdcan_filter_struct.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
    fdcan_filter_struct.FilterID1 = 0x000;
    fdcan_filter_struct.FilterID2 = 0x000;
    if (HAL_FDCAN_ConfigFilter(&hfdcan3, &fdcan_filter_struct) != HAL_OK)
    {
        return 1;
    }

    if (HAL_FDCAN_ConfigGlobalFilter(&hfdcan3, FDCAN_REJECT, FDCAN_REJECT, FDCAN_FILTER_REMOTE, FDCAN_FILTER_REMOTE) != HAL_OK)
    {
        return 1;
    }

    if (HAL_FDCAN_Start(&hfdcan3) != HAL_OK)
    {
        return 1;
    }

    return 0;
}

/**
 * @brief   FDCAN发送数据
 * @param   buffer: 数据
 * @param   timeout: 超时时间
 * @retval  发送结果
 * @arg     0: 发送成功
 * @arg     1: 发送失败
 */
uint8_t fdcan_send_data(uint8_t *buffer, uint32_t timeout)
{
    FDCAN_TxHeaderTypeDef fdcan_txheader_struct = {0};
    uint32_t count;

    fdcan_txheader_struct.Identifier = 0x012;
    fdcan_txheader_struct.IdType = FDCAN_STANDARD_ID;
    fdcan_txheader_struct.TxFrameType = FDCAN_DATA_FRAME;
    fdcan_txheader_struct.DataLength = FDCAN_DLC_BYTES_8;
    fdcan_txheader_struct.ErrorStateIndicator = FDCAN_ESI_PASSIVE;
    fdcan_txheader_struct.BitRateSwitch = FDCAN_BRS_ON;
    fdcan_txheader_struct.FDFormat = FDCAN_FD_CAN;
    fdcan_txheader_struct.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
    if (HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan3, &fdcan_txheader_struct, buffer) != HAL_OK)
    {
        return 1;
    }

    count = 0;
    while ((HAL_FDCAN_GetTxFifoFreeLevel(&hfdcan3) != hfdcan3.Init.TxFifoQueueElmtsNbr) && (count++ < timeout))
    {
        HAL_Delay(1);
    }

    if (count >= timeout)
    {
        return 1;
    }

    return 0;
}

/**
 * @brief   FDCAN接收数据
 * @param   buffer: 数据
 * @param   timeout: 超时时间
 * @arg     0: 接收成功
 * @arg     1: 接收失败
 */
uint8_t fdcan_recv_data(uint8_t *buffer, uint32_t timeout)
{
    FDCAN_RxHeaderTypeDef fdcan_rxheader_struct = {0};
    uint32_t count;

    count = 0;
    if ((HAL_FDCAN_GetRxFifoFillLevel(&hfdcan3, FDCAN_RX_FIFO0) == 0) && (count++ < timeout))
    {
        HAL_Delay(1);
    }

    if (count >= timeout)
    {
        return 1;
    }

    if (HAL_FDCAN_GetRxMessage(&hfdcan3, FDCAN_RX_FIFO0, &fdcan_rxheader_struct, buffer) != HAL_OK)
    {
        return 1;
    }

    if (fdcan_rxheader_struct.DataLength != FDCAN_DLC_BYTES_8)
    {
        return 1;
    }

    return 0;
}
