/**
 ****************************************************************************************************
 * @file        pwmdac.c
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2025-01-13
 * @brief       PWMDAC驱动代码
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

#include "pwmdac.h"

extern TIM_HandleTypeDef htim4; /* TIM句柄 */

/**
 * @brief   初始化PWMDAC
 * @param   arr: 自动重装载值
 * @param   psc: 预分频系数
 * @retval  无
 */
void pwmdac_init(uint16_t arr, uint16_t psc)
{
    UNUSED(arr);
    UNUSED(psc);

    /* 开启TIM PWM输出 */
    HAL_TIM_PWM_Start(&htim4, PWMDAC_TIMX_CH1);
}

/**
 * @brief   设置PWMDAC输出电压
 * @param   voltage: 输出电压（单位：mV）
 * @retval  无
 */
void pwmdac_set_voltage(uint16_t voltage)
{
    uint16_t value;

    value = (voltage * __HAL_TIM_GET_AUTORELOAD(&htim4)) / VDD_VALUE;
    __HAL_TIM_SET_COMPARE(&htim4, PWMDAC_TIMX_CH1, value);
}
