/**
 ****************************************************************************************************
 * @file        rs485.c
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2025-01-13
 * @brief       RS485驱动代码
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

#include "rs485.h"

extern UART_HandleTypeDef huart3;   /* UART句柄 */

uint8_t g_rs485_rx_buf[RS485_RX_BUF_SIZE];
uint8_t g_rs485_rx_cnt;

/**
 * @brief   初始化RS485
 * @param   baudrate: 通信波特率（单位：bps）
 * @retval  无
 */
void rs485_init(uint32_t baudrate)
{
    UNUSED(baudrate);

    /* 配置为接收模式 */
    RS485_RE(0);

    g_rs485_rx_cnt = 0;
    HAL_UART_Receive_IT(&huart3, &g_rs485_rx_buf[g_rs485_rx_cnt], 1);
}

/**
 * @brief   RS485中断接收处理函数
 * @param   无
 * @retval  无
 */
void rs485_recv_handler(void)
{
    if (g_rs485_rx_cnt < RS485_RX_BUF_SIZE)
    {
        g_rs485_rx_cnt++;
        HAL_UART_Receive_IT(&huart3, &g_rs485_rx_buf[g_rs485_rx_cnt], 1);
    }
}

/**
 * @brief   RS485发送数据
 * @param   buffer: 数据
 * @param   size: 数据大小
 * @retval  无
 */
void rs485_send_data(uint8_t *buffer, uint8_t size)
{
    RS485_RE(1);
    HAL_UART_Transmit(&huart3, buffer, size, 1000);
    RS485_RE(0);
}

/**
 * @brief   RS485接收数据
 * @param   buffer: 数据
 * @param   size: 数据大小
 * @retval  无
 */
void rs485_recv_data(uint8_t *buffer, uint8_t *size)
{
    uint8_t i;

    for (i = 0; i < g_rs485_rx_cnt; i++)
    {
        buffer[i] = g_rs485_rx_buf[i];
    }

    *size = g_rs485_rx_cnt;

    if (g_rs485_rx_cnt != 0)
    {
        if (g_rs485_rx_cnt < RS485_RX_BUF_SIZE)
        {
            HAL_UART_AbortReceive_IT(&huart3);
        }
        g_rs485_rx_cnt = 0;
        HAL_UART_Receive_IT(&huart3, &g_rs485_rx_buf[g_rs485_rx_cnt], 1);
    }
}
