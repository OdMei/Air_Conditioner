/**
 ****************************************************************************************************
 * @file        remote.c
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2025-01-13
 * @brief       红外遥控驱动代码
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

#include "remote.h"

extern TIM_HandleTypeDef htim1; /* TIM句柄 */

/* 遥控器接收状态
 * [7]  : 收到了引导码标志
 * [6]  : 得到了一个按键的所有信息
 * [5]  : 保留
 * [4]  : 标记上升沿是否已经被捕获
 * [3:0]: 溢出计时器
 */
volatile uint8_t g_remote_sta = 0;
volatile uint32_t g_remote_data = 0;     /* 红外接收到的数据 */
volatile uint8_t g_remote_cnt = 0;       /* 按键按下的次数 */

static volatile uint8_t s_remote_bits;
static volatile uint8_t s_remote_repeat_pending;
static uint8_t s_remote_last_key;
static uint8_t s_remote_key_active;
static uint32_t s_remote_last_seen_ms;
static uint32_t s_remote_last_action_ms;

/**
 * @brief   初始化红外遥控
 * @param   无
 * @retval  无
 */
void remote_init(void)
{
    uint32_t state = __get_PRIMASK();
    __disable_irq();
    g_remote_sta = 0U;
    g_remote_data = 0U;
    g_remote_cnt = 0U;
    s_remote_bits = 0U;
    s_remote_repeat_pending = 0U;
    s_remote_key_active = 0U;
    s_remote_last_key = 0U;
    s_remote_last_seen_ms = 0U;
    s_remote_last_action_ms = 0U;
    __set_PRIMASK(state);
    HAL_TIM_IC_Start_IT(&htim1, REMOTE_TIMX_CH1);
    __HAL_TIM_ENABLE_IT(&htim1, TIM_IT_UPDATE);
}

/**
 * @brief   扫描红外遥控键值
 * @param   无
 * @retval  红外遥控键值
 * @arg     0: 没有收到红外遥控信号
 * @arg     其他: 红外遥控键值
 */
uint8_t remote_scan(void)
{
    uint8_t key = 0U;
    uint8_t t1, t2;
    uint8_t complete;
    uint8_t repeat;
    uint32_t frame;
    uint32_t now = HAL_GetTick();
    uint32_t state = __get_PRIMASK();

    /* Snapshot and consume together, so an ISR cannot tear the frame or lose
     * a new completion while the foreground clears its previous event. */
    __disable_irq();
    complete = (uint8_t)(g_remote_sta & 0x40U);
    repeat = s_remote_repeat_pending;
    frame = g_remote_data;
    g_remote_sta &= (uint8_t)~0x40U;
    s_remote_repeat_pending = 0U;
    __set_PRIMASK(state);

    if ((s_remote_key_active != 0U) &&
        ((uint32_t)(now - s_remote_last_seen_ms) > REMOTE_RELEASE_MS))
    {
        s_remote_key_active = 0U;
    }
    if (complete != 0U)
    {
        t1 = (uint8_t)frame;
        t2 = (uint8_t)(frame >> 8U);
        if ((t1 == (uint8_t)~t2) && t1 == REMOTE_ID)    /* 检验遥控识别码(ID)及地址 */
        {
            t1 = (uint8_t)(frame >> 16U);
            t2 = (uint8_t)(frame >> 24U);
            if (t1 == (uint8_t)~t2)
            {
                key = t1;
            }
        }
        if (key == 0U)
        {
            s_remote_key_active = 0U;
            return 0U;
        }
        if ((s_remote_key_active == 0U) || (key != s_remote_last_key))
        {
            s_remote_last_key = key;
            s_remote_key_active = 1U;
            s_remote_last_seen_ms = now;
            s_remote_last_action_ms = now;
            return key;
        }
        /* NEC2 remotes repeat whole frames instead of a short repeat code.
         * Treat a same-key full frame within the release window as held too. */
        repeat = 1U;
    }
    if ((repeat != 0U) && (s_remote_key_active != 0U))
    {
        s_remote_last_seen_ms = now;
        if (((s_remote_last_key == REMOTE_REPEAT_UP_KEY) ||
             (s_remote_last_key == REMOTE_REPEAT_DOWN_KEY)) &&
            ((uint32_t)(now - s_remote_last_action_ms) >= REMOTE_REPEAT_PERIOD_MS))
        {
            s_remote_last_action_ms = now;
            return s_remote_last_key;
        }
    }
    return 0U;
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == REMOTE_TIMX && (g_remote_sta & 0x80U) != 0U)
    {
        g_remote_sta &= (uint8_t)~0x10U;
        if ((g_remote_sta & 0x0FU) < 14U) { g_remote_sta++; }
        else { g_remote_sta &= (uint8_t)~0x80U; g_remote_sta &= 0xF0U; }
    }
}

void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim)
{
    uint16_t dval;
    if (htim->Instance != REMOTE_TIMX) { return; }
    if (REMOTE_DATA())
    {
        TIM_RESET_CAPTUREPOLARITY(&htim1, REMOTE_TIMX_CH1);
        TIM_SET_CAPTUREPOLARITY(&htim1, REMOTE_TIMX_CH1,
                                TIM_ICPOLARITY_FALLING);
        __HAL_TIM_SET_COUNTER(&htim1, 0);
        g_remote_sta |= 0x10U;
        return;
    }
    dval = HAL_TIM_ReadCapturedValue(&htim1, REMOTE_TIMX_CH1);
    TIM_RESET_CAPTUREPOLARITY(&htim1, REMOTE_TIMX_CH1);
    TIM_SET_CAPTUREPOLARITY(&htim1, REMOTE_TIMX_CH1, TIM_ICPOLARITY_RISING);
    if ((g_remote_sta & 0x10U) == 0U) { return; }
    if ((g_remote_sta & 0x80U) == 0U)
    {
        if (dval > 4200U && dval < 4700U)
        {
            g_remote_sta = (uint8_t)((g_remote_sta & 0x10U) | 0x80U);
            g_remote_data = 0U;
            g_remote_cnt = 0U;
            s_remote_bits = 0U;
            s_remote_repeat_pending = 0U;
        }
    }
    else if (dval > 4200U && dval < 4700U)
    {
        /* A fresh leader can arrive before the old release timer expires. */
        g_remote_sta = (uint8_t)((g_remote_sta & 0x10U) | 0x80U);
        g_remote_data = 0U;
        g_remote_cnt = 0U;
        s_remote_bits = 0U;
        s_remote_repeat_pending = 0U;
    }
    else if (dval > 300U && dval < 800U && s_remote_bits < 32U)
    {
        g_remote_data >>= 1; g_remote_data &= 0x7FFFFFFFU;
        s_remote_bits++;
        if (s_remote_bits == 32U) { g_remote_sta |= 0x40U; }
    }
    else if (dval > 1400U && dval < 1800U && s_remote_bits < 32U)
    {
        g_remote_data >>= 1; g_remote_data |= 0x80000000U;
        s_remote_bits++;
        if (s_remote_bits == 32U) { g_remote_sta |= 0x40U; }
    }
    else if (dval > 2000U && dval < 3000U)
    {
        g_remote_cnt++; g_remote_sta &= 0xF0U;
        s_remote_repeat_pending = 1U;
    }
}
