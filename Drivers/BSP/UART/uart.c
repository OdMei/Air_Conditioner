/**
 ****************************************************************************************************
 * @file        uart.c
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2025-01-13
 * @brief       串口驱动代码
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

#include "uart.h"
#include <string.h>

/* UART句柄 */
extern UART_HandleTypeDef huart1;

#define UART_TX_CAPACITY        (4096U)
#define UART_TX_CHUNK           (64U)
#define UART_TX_TIMEOUT_MS      (10U)
#define UART_TX_EARLY_TIMEOUT_MS (2U)

static uint8_t s_tx_buffer[UART_TX_CAPACITY];
static uint16_t s_tx_head;
static uint16_t s_tx_tail;
static uint16_t s_tx_count;
static uint8_t s_tx_started;
static uint8_t s_tx_processing;
static uart_tx_stats_t s_tx_stats;

static uint32_t uart_tx_lock(void)
{
    uint32_t state = __get_PRIMASK();
    __disable_irq();
    return state;
}

static void uart_tx_unlock(uint32_t state)
{
    __set_PRIMASK(state);
}

void uart_tx_start(void)
{
    uint32_t state = uart_tx_lock();
    /* Starting twice must not discard queued diagnostics. */
    if (s_tx_started == 0U)
    {
        s_tx_head = s_tx_tail = s_tx_count = 0U;
        s_tx_processing = 0U;
        memset(&s_tx_stats, 0, sizeof(s_tx_stats));
        s_tx_started = 1U;
    }
    uart_tx_unlock(state);
}

uint8_t uart_tx_enqueue_line(const char *data, uint16_t length)
{
    uint16_t index;
    uint32_t state = uart_tx_lock();
    if ((s_tx_started == 0U) || (data == NULL) || (length == 0U) ||
        (length > UART_TX_LINE_MAX) ||
        (length > UART_TX_CAPACITY - s_tx_count))
    {
        s_tx_stats.dropped_lines++;
        s_tx_stats.dropped_bytes += length;
        uart_tx_unlock(state);
        return 0U;
    }
    for (index = 0U; index < length; index++)
    {
        s_tx_buffer[s_tx_head] = (uint8_t)data[index];
        s_tx_head = (uint16_t)((s_tx_head + 1U) % UART_TX_CAPACITY);
    }
    s_tx_count = (uint16_t)(s_tx_count + length);
    s_tx_stats.queued_bytes = s_tx_count;
    if (s_tx_count > s_tx_stats.high_water_bytes)
    {
        s_tx_stats.high_water_bytes = s_tx_count;
    }
    uart_tx_unlock(state);
    return 1U;
}

void uart_tx_process(void)
{
    uint8_t chunk[UART_TX_CHUNK];
    uint16_t length;
    uint16_t index;
    HAL_StatusTypeDef status;
    uint32_t state = uart_tx_lock();
    if ((state != 0U) || (s_tx_started == 0U) || (s_tx_count == 0U) ||
        (s_tx_processing != 0U) || (__get_IPSR() != 0U))
    {
        uart_tx_unlock(state);
        return;
    }
    s_tx_processing = 1U;
    length = s_tx_count < UART_TX_CHUNK ? s_tx_count : UART_TX_CHUNK;
    for (index = 0U; index < length; index++)
    {
        chunk[index] = s_tx_buffer[s_tx_tail];
        s_tx_tail = (uint16_t)((s_tx_tail + 1U) % UART_TX_CAPACITY);
    }
    s_tx_count = (uint16_t)(s_tx_count - length);
    s_tx_stats.queued_bytes = s_tx_count;
    uart_tx_unlock(state);

    /* Never hold the IRQ lock while polling HAL. A failed chunk may have
     * partially reached the wire, so do not replay it and duplicate bytes. */
    status = HAL_UART_Transmit(&huart1, chunk, length, UART_TX_TIMEOUT_MS);
    state = uart_tx_lock();
    s_tx_stats.last_hal_status = (uint32_t)status;
    if (status == HAL_OK)
    {
        s_tx_stats.sent_bytes += length;
    }
    else
    {
        s_tx_stats.tx_errors++;
        s_tx_stats.dropped_bytes += length;
    }
    s_tx_processing = 0U;
    uart_tx_unlock(state);
}

void uart_tx_get_stats(uart_tx_stats_t *stats)
{
    uint32_t state;
    if (stats == NULL) { return; }
    state = uart_tx_lock();
    *stats = s_tx_stats;
    uart_tx_unlock(state);
}

#if UART_EN_RX
/* 串口中断接收缓冲区 */
uint8_t g_rx_buffer[RXBUFFERSIZE];
/* 串口接收缓冲区 */
uint8_t g_uart_rx_buf[UART_REC_LEN];
/* 串口接收状态标记 */
uint16_t g_uart_rx_sta = 0;
#endif

/**
 * @brief   初始化串口
 * @param   baudrate: 通信波特率（单位：bps）
 * @retval  无
 */
void uart_init(uint32_t baudrate)
{
    UNUSED(baudrate);

    setvbuf(stdout, NULL, _IONBF, 0);

#if UART_EN_RX
    /* UART中断接收数据 */
    HAL_UART_Receive_IT(&huart1, g_rx_buffer, sizeof(g_rx_buffer));
#endif
}

#if UART_EN_RX
/**
 * @brief   HAL库UART接收完成回调函数
 * @param   huart: UART句柄指针
 * @retval  无
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == UART_UX)
    {
        if((g_uart_rx_sta & 0x8000) == 0)
        {
            if(g_uart_rx_sta & 0x4000)
            {
                if(g_rx_buffer[0] != 0x0A)
                {
                    g_uart_rx_sta = 0;
                }
                else
                {
                    g_uart_rx_sta |= 0x8000;
                }
            }
            else
            {
                if(g_rx_buffer[0] == 0x0D)
                {
                    g_uart_rx_sta |= 0x4000;
                }
                else
                {
                    g_uart_rx_buf[g_uart_rx_sta & 0x3FFF] = g_rx_buffer[0];
                    g_uart_rx_sta++;
                    if(g_uart_rx_sta > (UART_REC_LEN - 1))
                    {
                        g_uart_rx_sta = 0;
                    }
                }
            }
        }

        HAL_UART_Receive_IT(&huart1, g_rx_buffer, sizeof(g_rx_buffer));
    }
}
#endif

/**
 * @brief   重定向C库的printf函数到串口
 * @param   无
 * @retval  无
 */
int __io_putchar(int ch)
{
    uint8_t value = (uint8_t)ch;
    if (s_tx_started != 0U)
    {
        return uart_tx_enqueue_line((const char *)&value, 1U) != 0U ? ch : EOF;
    }
    /* Startup diagnostics have a finite timeout. IRQ callers never poll HAL. */
    if ((__get_IPSR() != 0U) || (__get_PRIMASK() != 0U) ||
        HAL_UART_Transmit(&huart1, &value, 1U, UART_TX_EARLY_TIMEOUT_MS) != HAL_OK)
    {
        return EOF;
    }
    return ch;
}
