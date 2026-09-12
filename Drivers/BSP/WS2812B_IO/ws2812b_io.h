/**
 ****************************************************************************************************
 * @file        ws2812b.h
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2025-03-06
 * @brief       WS2812B(RGBLED) 驱动代码
 * @license     Copyright (c) 2023-2035, 广州市星翼电子科技有限公司
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

#ifndef __WS2812B_IO_H
#define __WS2812B_IO_H

#include "main.h"

/*************************************RGBLED端口定义 实验相关宏定义*************************************/
#define WS2812B_PORT                    GPIOF
#define WS2812B_PIN                     GPIO_PIN_1
#define WS2812B_GPIO_CLK_ENABLE()       do{ __HAL_RCC_GPIOF_CLK_ENABLE(); }while(0)   /* PB口时钟使能 */

#define WS2812B(x)                      do{ x ? \
                                        HAL_GPIO_WritePin(WS2812B_PORT, WS2812B_PIN, GPIO_PIN_SET) : \
                                        HAL_GPIO_WritePin(WS2812B_PORT, WS2812B_PIN, GPIO_PIN_RESET); \
                                        }while(0)
/*****************************************************************************************************/
/* RGBLED的数据相关宏定义 */
#define DELAY_15nS()         __NOP()                                    /* 宏定义空指令的方法不好计算，只能大致计算，并且受编译器优化等级影响 */
#define DELAY_400nS()       for(uint16_t i = 0; i < 41; i++) __NOP()   /* 约400ns*/
#define DELAY_800nS()       for(uint16_t i = 0; i < 101; i++) __NOP()   /* 约800ns */
#define LED_NUM             10                                          /* 灯珠的个数 */

/*****************************************************************************************************/
/* GRB888颜色值 */
#define GRB888_RED          0x00FF00        /* 红色 */
#define GRB888_GREEN        0xFF0000        /* 绿色 */
#define GRB888_BLUE         0x0000FF        /* 蓝色 */
#define GRB888_BLACK        0x000000        /* 黑色 */
#define GRB888_WHITE        0xFFFFFF        /* 白色 */
#define GRB888_YELLOW       0xFFFF00        /* 黄色 */
#define GRB888_IRED         0x5CCD5C        /* 浅红色 */
#define GRB888_ORANGE       0xA5FF00        /* 橙色 */
#define GRB888_PURPLE       0x008080        /* 紫色 */        
#define GRB888_PING         0xB6FFC1        /* 浅粉色 */
#define GRB888_CYAN         0xFF00FF        /* 青色 */
#define GRB888_PBLUE        0x80008C        /* 孔雀蓝 */
#define GRB888_VIOLET       0x008BFF        /* 紫罗兰 */

/*****************************************************************************************************/
extern const uint32_t g_grb888_color[LED_NUM];

void ws2812b_init(void);                                    /* 初始化W2812B */
void ws2812b_reset(void);                                   /* 复位W2812B */
void ws2812b_write_color(uint32_t color);                   /* 写入一个颜色值到W2812B（单灯） */
void ws2812b_display(uint8_t led_num, uint32_t *color);     /* 写入多个颜色值到W2812B（控制多个灯） */
uint32_t color_change_brigh(uint32_t rgb, float k);         /* 改变颜色的亮度值 */
#endif













