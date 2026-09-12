/**
 ****************************************************************************************************
 * @file        dts.c
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2025-01-13
 * @brief       DTS驱动代码
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

#include "dts.h"

extern DTS_HandleTypeDef hdts;  /* DTS句柄 */

/**
 * @brief   初始化DTS
 * @param   无
 * @retval  无
 */
void dts_init(void)
{
    return;
}

/**
 * @brief   获取DTS测量结果
 * @param   HAL_DTS_Sensor: DTS传感器
 * @retval  DTS测量结果
 */
float_t dts_get_result(HAL_DTS_Sensor sensor)
{
    float_t temperature;

    /* 开始DTS传感器测量 */
    HAL_DTS_Start(&hdts, sensor);

    /* 等待DTS传感器测量结束 */
    HAL_DTS_PollForTemperature(&hdts, sensor, 1000);

    /* 获取DTS传感器测量结果 */
    HAL_DTS_GetTemperature(&hdts, sensor, &temperature);

    /* 停止DTS传感器测量 */
    HAL_DTS_Stop(&hdts, sensor);

    return temperature;
}
