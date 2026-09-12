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
uint8_t g_remote_sta = 0;
uint32_t g_remote_data = 0;     /* 红外接收到的数据 */
uint8_t g_remote_cnt = 0;       /* 按键按下的次数 */

/**
 * @brief   初始化红外遥控
 * @param   无
 * @retval  无
 */
void remote_init(void)
{
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
    uint8_t sta = 0;
    uint8_t t1, t2;

    if (g_remote_sta & (1 << 6))                        /* 得到一个按键的所有信息了 */
    {
        t1 = g_remote_data;                             /* 得到地址码 */
        t2 = (g_remote_data >> 8) & 0xff;               /* 得到地址反码 */

        if ((t1 == (uint8_t)~t2) && t1 == REMOTE_ID)    /* 检验遥控识别码(ID)及地址 */
        {
            t1 = (g_remote_data >> 16) & 0xff;
            t2 = (g_remote_data >> 24) & 0xff;

            if (t1 == (uint8_t)~t2)
            {
                sta = t1;                               /* 键值正确 */
            }
        }

        if ((sta == 0) || ((g_remote_sta & 0X80) == 0)) /* 按键数据错误/遥控已经没有按下了 */
        {
            g_remote_sta &= ~(1 << 6);                  /* 清除接收到有效按键标识 */
            g_remote_cnt = 0;                           /* 清除按键次数计数器 */
        }
    }

    return sta;
}
