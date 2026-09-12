/**
 ****************************************************************************************************
 * @file        spdifrx.c
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2025-01-13
 * @brief       SPDIFRX驱动代码
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

#include "spdifrx.h"

extern SPDIFRX_HandleTypeDef hspdif1;   /* SPDIFRX句柄 */

static __IO uint8_t spdifrx_sync_done_flag = 0;
static __IO uint8_t spdifrx_rx_dma_cplt_flag = 1;

static void (*spdifrx_rx_dma_cplt_user_cb)(void) = NULL;

void spdifrx_init(void)
{
    __HAL_SPDIFRX_ENABLE_IT(&hspdif1, SPDIFRX_IT_IFEIE);

    __HAL_SPDIFRX_IDLE(&hspdif1);
}

void spdifrx_sync(void)
{
    __HAL_SPDIFRX_ENABLE_IT(&hspdif1, SPDIFRX_IT_SYNCDIE);

    __HAL_SPDIFRX_SYNC(&hspdif1);
}

void spdifrx_stop_sync(void)
{
    __HAL_SPDIFRX_DISABLE_IT(&hspdif1, SPDIFRX_IT_SYNCDIE);

    HAL_DMA_Abort(hspdif1.hdmaDrRx);
    HAL_SPDIFRX_DMAStop(&hspdif1);
}

/**
 * @brief   SPDIFRX同步是否完成
 * @param   无
 * @retval  结果
 * @arg     0: 否
 * @arg     1: 是
 */
uint8_t spdifrx_sync_is_done(void)
{
    return spdifrx_sync_done_flag ? 1 : 0;
}

/**
 * @brief   SPDIFRX 注册DMA接收完成回调
 * @param   cb: 回调
 * @retval  无
 */
void spdifrx_rx_dma_cplt_user_cb_register(void (*cb)(void))
{
    spdifrx_rx_dma_cplt_user_cb = cb;
}

/**
 * @brief   SPDIFRX DMA接收
 * @param   buffer: 数据
 * @param   data_amount: 数据量
 * @retval  接收结果
 * @arg     0: 接收成功
 * @arg     1: 接收失败
 */
uint8_t spdifrx_rx_dma(uint8_t *buffer, uint32_t data_amount)
{
    if (HAL_SPDIFRX_ReceiveDataFlow_DMA(&hspdif1, (uint32_t *)buffer, data_amount) != HAL_OK)
    {
        return 1;
    }

    return 0;
}

/**
 * @brief   SPDIFRX DMA接收完成处理函数
 * @param   无
 * @retval  无
 */
void spdifrx_rx_dma_cplt_handler(void)
{
    spdifrx_rx_dma_cplt_flag = 1;
    if (spdifrx_rx_dma_cplt_user_cb != NULL)
    {
        spdifrx_rx_dma_cplt_user_cb();
    }
}

/**
 * @brief   SPDIFRX DMA接收是否忙
 * @param   无
 * @retval  结果
 * @arg     0: 否
 * @arg     1: 是
 */
uint8_t spdifrx_rx_dma_is_busy(void)
{
    return spdifrx_rx_dma_cplt_flag ? 0 : 1;
}

/**
 * @brief   获取SPDIFRX采样率
 * @param   无
 * @retval  采样率
 */
uint32_t spdifrx_get_sample_rate(void)
{
    uint32_t frequency;
    uint32_t sample_rate;
    uint32_t spdifrx_sr_width5;

    spdifrx_sr_width5 = (hspdif1.Instance->SR & SPDIFRX_SR_WIDTH5) >> SPDIFRX_SR_WIDTH5_Pos;
    frequency = HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_SPDIFRX1);
    if (frequency == 0)
    {
        return 0;
    }

    sample_rate = ((5 * frequency) / spdifrx_sr_width5) >> 6;

    return sample_rate;
}

/**
 * @brief   SPDIFRX中断处理函数
 * @param   无
 * @retval  无
 */
void spdifrx_irq_handler(void)
{
    if (__HAL_SPDIFRX_GET_IT_SOURCE(&hspdif1, SPDIFRX_IT_SYNCDIE) && __HAL_SPDIFRX_GET_FLAG(&hspdif1, SPDIFRX_FLAG_SYNCD))
    {
        __HAL_SPDIFRX_CLEAR_IT(&hspdif1, SPDIFRX_SR_SYNCD);

        spdifrx_sync_done_flag = 1;
    }

    if (__HAL_SPDIFRX_GET_IT_SOURCE(&hspdif1, SPDIFRX_IT_IFEIE) && ((__HAL_SPDIFRX_GET_FLAG(&hspdif1, SPDIFRX_FLAG_FERR)) || (__HAL_SPDIFRX_GET_FLAG(&hspdif1, SPDIFRX_FLAG_SERR)) || (__HAL_SPDIFRX_GET_FLAG(&hspdif1, SPDIFRX_FLAG_TERR))))
    {
        __HAL_SPDIFRX_IDLE(&hspdif1);
        __HAL_SPDIFRX_CLEAR_IT(&hspdif1, SPDIFRX_SR_SYNCD);

        spdifrx_sync_done_flag = 0;
    }
}
