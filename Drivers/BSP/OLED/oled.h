/**
 ****************************************************************************************************
 * @file        oled.h
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2025-01-13
 * @brief       OLED驱动代码
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

#ifndef __OLED_H
#define __OLED_H

#include "main.h"

/* OLED接口模式设置
 * 0: 4线SPI模式（OLED模块的BS1、BS2均接GND）
 * 1: 8080并口模式（OLED模块的BS1、BS2均接VCC）
 */
#define OLED_MODE 1 /* 默认使用8080并口模式 */

/* OLED 4线SPI引脚定义
 * 注意：这里仅定义了OLED 4线SPI模式的引脚，8080并口模式的引脚太多，就不单独定义了
 */
#define OLED_SPI_RST_PORT   GPIOQ
#define OLED_SPI_RST_PIN    GPIO_PIN_2

#define OLED_SPI_CS_PORT    GPIOB
#define OLED_SPI_CS_PIN     GPIO_PIN_8

#define OLED_SPI_RS_PORT    GPIOE
#define OLED_SPI_RS_PIN     GPIO_PIN_13

#define OLED_SPI_SCLK_PORT  GPIOD
#define OLED_SPI_SCLK_PIN   GPIO_PIN_7

#define OLED_SPI_SDIN_PORT  GPIOE
#define OLED_SPI_SDIN_PIN   GPIO_PIN_6

/* OLED 4线SPI模式引脚操作定义
 * 注意：OLED_RST、OLED_CS、OLED_RS这三个引脚是4线SPI模式和8080并口模式共用的
 */
#define OLED_RST(x)         do { (x) ?                                                                      \
                                HAL_GPIO_WritePin(OLED_SPI_RST_PORT, OLED_SPI_RST_PIN, GPIO_PIN_SET):       \
                                HAL_GPIO_WritePin(OLED_SPI_RST_PORT, OLED_SPI_RST_PIN, GPIO_PIN_RESET);     \
                            } while (0)

#define OLED_CS(x)          do { (x) ?                                                                      \
                                HAL_GPIO_WritePin(OLED_SPI_CS_PORT, OLED_SPI_CS_PIN, GPIO_PIN_SET):         \
                                HAL_GPIO_WritePin(OLED_SPI_CS_PORT, OLED_SPI_CS_PIN, GPIO_PIN_RESET);       \
                            } while (0)

#define OLED_RS(x)          do { (x) ?                                                                      \
                                HAL_GPIO_WritePin(OLED_SPI_RS_PORT, OLED_SPI_RS_PIN, GPIO_PIN_SET):         \
                                HAL_GPIO_WritePin(OLED_SPI_RS_PORT, OLED_SPI_RS_PIN, GPIO_PIN_RESET);       \
                            } while (0)

#define OLED_SCLK(x)        do { (x) ?                                                                      \
                                HAL_GPIO_WritePin(OLED_SPI_SCLK_PORT, OLED_SPI_SCLK_PIN, GPIO_PIN_SET):     \
                                HAL_GPIO_WritePin(OLED_SPI_SCLK_PORT, OLED_SPI_SCLK_PIN, GPIO_PIN_RESET);   \
                            } while (0)

#define OLED_SDIN(x)        do { (x) ?                                                                      \
                                HAL_GPIO_WritePin(OLED_SPI_SDIN_PORT, OLED_SPI_SDIN_PIN, GPIO_PIN_SET):     \
                                HAL_GPIO_WritePin(OLED_SPI_SDIN_PORT, OLED_SPI_SDIN_PIN, GPIO_PIN_RESET);   \
                            } while (0)

/* OLED 8080并口模式引脚操作定义 */
#define OLED_WR(x)          do { (x) ?                                                                      \
                                HAL_GPIO_WritePin(GPIOD, GPIO_PIN_0, GPIO_PIN_SET):                         \
                                HAL_GPIO_WritePin(GPIOD, GPIO_PIN_0, GPIO_PIN_RESET);                       \
                            } while (0)

#define OLED_RD(x)          do { (x) ?                                                                      \
                                HAL_GPIO_WritePin(GPIOE, GPIO_PIN_14, GPIO_PIN_SET):                        \
                                HAL_GPIO_WritePin(GPIOE, GPIO_PIN_14, GPIO_PIN_RESET);                      \
                            } while (0)

/* 命令/数据定义 */
#define OLED_CMD    0   /* 写命令 */
#define OLED_DATA   1   /* 写数据 */

/* 函数声明 */
void oled_refresh_gram(void);                                                       /* 更新显存到OLED */
void oled_display_on(void);                                                         /* 开启OLED显示 */
void oled_display_off(void);                                                        /* 关闭OLED显示 */
void oled_clear(void);                                                              /* 清屏函数 */
void oled_draw_point(uint8_t x, uint8_t y, uint8_t dot);                            /* OLED画点 */
void oled_fill(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, uint8_t dot);        /* OLED填充区域填充 */
void oled_show_char(uint8_t x, uint8_t y, uint8_t chr, uint8_t size, uint8_t mode); /* 在指定位置显示一个字符 */
void oled_show_num(uint8_t x, uint8_t y, uint32_t num, uint8_t len, uint8_t size);  /* 显示len个数字 */
void oled_show_string(uint8_t x, uint8_t y, const char *p, uint8_t size);           /* 显示字符串 */
void oled_init(void);                                                               /* 初始化OLED */

#endif
