/**
 ****************************************************************************************************
 * @file        tpad.c
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2025-01-13
 * @brief       电容触摸按键驱动代码
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

#include "tpad.h"

/* TPAD默认捕获值 */
static uint32_t g_tpad_default_val;

/* 捕获定时器句柄 */
extern TIM_HandleTypeDef htim2;

/**
 * @brief   初始化定时器输入捕获
 * @param   arr: 自动重装载值
 * @param   psc: 预分频系数
 * @retval  无
 */
static void tpad_timx_cap_init(uint32_t arr, uint16_t psc)
{
    UNUSED(arr);
    UNUSED(psc);

    /* 开启定时器输入捕获 */
    HAL_TIM_IC_Start(&htim2, TPAD_TIMX_CAP_CHY);
}

/**
 * @brief   复位TPAD
 * @param   无
 * @retval  无
 */
static void tpad_reset(void)
{
    GPIO_InitTypeDef gpio_init_struct = {0};

    /* 配置TPAD引脚为输出引脚 */
    gpio_init_struct.Pin = TPAD_GPIO_PIN;
    gpio_init_struct.Mode = GPIO_MODE_OUTPUT_PP;
    gpio_init_struct.Pull = GPIO_PULLDOWN;
    gpio_init_struct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(TPAD_GPIO_PORT, &gpio_init_struct);

    /* 对电容触摸按键放电 */
    HAL_GPIO_WritePin(TPAD_GPIO_PORT, TPAD_GPIO_PIN, GPIO_PIN_RESET);
    HAL_Delay(5);

    __HAL_TIM_CLEAR_FLAG(&htim2, (TPAD_TIMX_CAP_CHY == TIM_CHANNEL_1) ? TIM_FLAG_CC1 :
                                 (TPAD_TIMX_CAP_CHY == TIM_CHANNEL_2) ? TIM_FLAG_CC2 :
                                 (TPAD_TIMX_CAP_CHY == TIM_CHANNEL_3) ? TIM_FLAG_CC3 :
                                 (TPAD_TIMX_CAP_CHY == TIM_CHANNEL_4) ? TIM_FLAG_CC4 :
                                 (TPAD_TIMX_CAP_CHY == TIM_CHANNEL_5) ? TIM_FLAG_CC5 :
                                 TIM_FLAG_CC6);
    __HAL_TIM_SET_COUNTER(&htim2, 0);

    /* 配置TPAD引脚为定时器输入捕获引脚 */
    gpio_init_struct.Pin = TPAD_GPIO_PIN;
    gpio_init_struct.Mode = GPIO_MODE_AF_PP;
    gpio_init_struct.Pull = GPIO_NOPULL;
    gpio_init_struct.Speed = GPIO_SPEED_FREQ_HIGH;
    gpio_init_struct.Alternate = TPAD_GPIO_AF;
    HAL_GPIO_Init(TPAD_GPIO_PORT, &gpio_init_struct);
}

/**
 * @brief   获取定时器捕获值
 * @param   无
 * @retval  捕获值或计数值
 * @arg     等待捕获未超时: 捕获值
 * @arg     等待捕获超时: 计数值
 */
static uint32_t tpad_get_val(void)
{
    uint32_t flag;
    uint32_t count;

    flag = (TPAD_TIMX_CAP_CHY == TIM_CHANNEL_1) ? TIM_FLAG_CC1 :
           (TPAD_TIMX_CAP_CHY == TIM_CHANNEL_2) ? TIM_FLAG_CC2 :
           (TPAD_TIMX_CAP_CHY == TIM_CHANNEL_3) ? TIM_FLAG_CC3 :
           (TPAD_TIMX_CAP_CHY == TIM_CHANNEL_4) ? TIM_FLAG_CC4 :
           (TPAD_TIMX_CAP_CHY == TIM_CHANNEL_5) ? TIM_FLAG_CC5 :
           TIM_FLAG_CC6;

    /* 复位TPAD */
    tpad_reset();

    /* 等待捕获到上升沿 */
    while (__HAL_TIM_CLEAR_FLAG(&htim2, flag) == RESET)
    {
        /* 等待超时，则直接返回计数值 */
        count = __HAL_TIM_GET_COUNTER(&htim2);
        if (count > (TPAD_TIMX_ARR_MAX_VAL - 500))
        {
            return count;
        }
    }

    /* 返回捕获比较值 */
    return __HAL_TIM_GET_COMPARE(&htim2, TPAD_TIMX_CAP_CHY);
}

/**
 * @brief   获取定时器连续指定次数捕获值的最大值
 * @param   times: 指定定时器连续捕获的次数
 * @retval  定时器连续指定次数捕获值的最大值
 */
static uint32_t tpad_get_maxval(uint8_t times)
{
    uint32_t value;
    uint32_t value_max = 0;

    while (times--)
    {
        value = tpad_get_val();
        if (value > value_max)
        {
            value_max = value;
        }
    }

    return value_max;
}

/**
 * @brief   初始化电容触摸按键
 * @param   psc: 捕获定时器分频系数（范围1~65535，越小灵敏度越高）
 * @retval  初始化结果
 * @arg     0: 初始化成功
 * @arg     1: 初始化失败
 */
uint8_t tpad_init(uint16_t psc)
{
    uint32_t values[10];
    uint32_t value = 0;
    uint8_t i;
    uint8_t j;

    /* 初始化定时器输入捕获 */
    tpad_timx_cap_init(TPAD_TIMX_ARR_MAX_VAL, psc - 1);

    /* 连续获取10次捕获值 */
    for (i=0; i<10; i++)
    {
        values[i] = tpad_get_val();
        HAL_Delay(10);
    }

    /* 升序排序 */
    for (i=0; i<9; i++)
    {
        for (j=i+1; j<10; j++)
        {
            if (values[i] > values[j])
            {
                values[i] = values[i] ^ values[j];
                values[j] = values[i] ^ values[j];
                values[i] = values[i] ^ values[j];
            }
        }
    }

    /* 对中间的6个数据进行均值滤波得到默认捕获值 */
    for (i=2; i<8; i++)
    {
        value += values[i];
    }
    g_tpad_default_val = value / 6;

    /* 检验默认捕获值的合法性 */
    if (g_tpad_default_val > (TPAD_TIMX_ARR_MAX_VAL >> 1))
    {
        return 1;
    }

    return 0;
}

/**
 * @brief   扫描TPAD
 * @param   mode: 扫描模式
 * @arg     0: 不支持连续按
 * @arg     1: 支持连续按
 * @retval  扫描结果
 * @arg     0: 没有按下
 * @arg     1: 按下有效
 */
uint8_t tpad_scan(uint8_t mode)
{
    static uint8_t keyen = 0;
    uint8_t res = 0;
    uint8_t sample = 3;
    uint32_t value;

    /* 需要支持连续按时，每次进行6次采样 */
    if (mode != 0)
    {
        sample = 6;
        keyen = 0;
    }

    /* 获取捕获值 */
    value = tpad_get_maxval(sample);

    /* 检验捕获值是否有效 */
    if (value > (g_tpad_default_val + TPAD_GATE_VAL))
    {
        /* 连续调用时才返回有效 */
        if (keyen == 0)
        {
            res = 1;
        }

        /* 重新调用次数计数 */
        keyen = 3;
    }

    /* 调用次数计数 */
    if (keyen != 0)
    {
        keyen--;
    }

    return res;
}
