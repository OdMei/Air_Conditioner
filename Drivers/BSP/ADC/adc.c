/**
 ****************************************************************************************************
 * @file        adc.c
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2025-01-13
 * @brief       ADC驱动代码
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

#include "adc.h"

extern ADC_HandleTypeDef hadc1;

/**
 * @brief   初始化ADC
 * @param   无
 * @retval  无
 */
void adc_init(void)
{
    /* 使能VDDA */
    HAL_PWREx_EnableVddA();

    /* ADC自动自校准 */
    HAL_ADCEx_Calibration_Start(&hadc1, ADC_SINGLE_ENDED);
}

/**
 * @brief   获取ADC结果
 * @param   channel: ADC通道
 * @retval  ADC结果
 */
uint32_t adc_get_result(uint32_t channel)
{
    uint32_t result;

    UNUSED(channel);

    /* 开启ADC转换 */
    HAL_ADC_Start(&hadc1);

    /* 等待ADC转换结束 */
    HAL_ADC_PollForConversion(&hadc1, 1000);

    /* 获取ADC转换结果 */
    result = HAL_ADC_GetValue(&hadc1);

    /* 停止ADC转换 */
    HAL_ADC_Stop(&hadc1);

    return result;
}

/**
 * @brief   均值滤波获取ADC结果
 * @param   channel: ADC通道
 * @param   times: 均值滤波的原始数据个数
 * @retval  ADC结果
 */
uint32_t adc_get_result_average(uint32_t channel, uint8_t times)
{
    uint32_t sum_result = 0;
    uint8_t i;

    for (i = 0; i < times; i++)
    {
        sum_result += adc_get_result(channel);
    }

    return sum_result / times;
}
