/**
 ****************************************************************************************************
 * @file        ws2812b.c
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

#include "ws2812b_io.h"
#include "./SYS/sys.h"
#include "string.h"


/* 用于存放10个RGBLED灯的颜色值 */
const uint32_t g_grb888_color[LED_NUM] = 
{
    GRB888_RED,         /* 红色 */
    GRB888_GREEN,       /* 绿色 */
    GRB888_BLUE,        /* 蓝色 */
    GRB888_VIOLET,      /* 紫罗兰 */
    GRB888_YELLOW,      /* 黄色 */
    GRB888_IRED,        /* 浅红色 */
    GRB888_ORANGE,      /* 橙色 */
    GRB888_PURPLE,      /* 紫色 */
    GRB888_PING,        /* 粉色 */
    GRB888_CYAN,        /* 青色 */
};

/**
 * @brief       复位WS2812B
 * @note        保持低电平280us以上即可 （允许些许偏差）
 * @param       无
 * @retval      无
 */
void ws2812b_reset(void)
{
    uint32_t color_buf[10];                                 /* 临时存放颜色值 */
    memset(color_buf, GRB888_BLACK, sizeof(color_buf));     /* 将10个RGBLED灯全部填充为黑色（熄灭状态） */
    ws2812b_display(LED_NUM, color_buf);                    /* 黑色填充显示（熄灭） */
}

/**
 * @brief       WS2812B的逻辑1
 * @note        T1H / T1L > 2 / 1 且T1H + T1L = 1.25us （允许些许偏差）
 * @param       无
 * @retval      无
 */
static void ws2812b_send_high(void)
{
    WS2812B(1);
    DELAY_800nS();
    WS2812B(0);
    DELAY_400nS();
}

/**
 * @brief       WS2812B的逻辑0
 * @note        T1H / T1L < 1 / 2 且T1H + T1L = 1.25us （允许些许偏差）
 * @param       无
 * @retval      无
 */
static void ws2812b_send_low(void)
{
    WS2812B(1);
    DELAY_400nS();
    WS2812B(0);
    DELAY_800nS();
}

/**
 * @brief       向WS2812B写入一个GRB888的颜色值（点亮一个灯）
 * @param       color ： GRB888颜色值
 * @retval      无
 */
void ws2812b_write_color(uint32_t color)
{
    uint8_t i;
    for (i = 0; i < 24; i++)
    {
        /* 每次左移后，判断最高位，需拉高或拉低 */
        ((color << i) & 0x800000) ? ws2812b_send_high() : ws2812b_send_low(); 
    }
}

/**
 * @brief   可同时点亮多个灯并显示相应的颜色
 * @param   led_num ：   灯的数量
 * @param   color   ：   存放GRB888颜色值指针
 * @retval  无
 */
void ws2812b_display(uint8_t led_num, uint32_t *color)
{
    uint8_t i;
    
    ws2812b_send_low();
    sys_delay_us(100);
    
    for(i = 0; i < led_num; i++)
    {
        ws2812b_write_color(color_change_brigh(color[i], 0.05));
    }
}

/**
 * @brief       初始化WS2812B相关IO口, 并使能时钟
 * @param       无
 * @retval      无
 */
void ws2812b_init(void)
{
    ws2812b_reset();                                        /* 复位WS2812B */
}

/**
 * @brief   改变所要显示的颜色亮度（RGB->HSV->RGB）
 * @param   rgb : GRB888颜色值
 * @param   k   : 亮度值（0.0 ~ 1.0）
 * @retval  改变亮度后的颜色值
 */
uint32_t color_change_brigh(uint32_t rgb, float k)
{
    uint8_t r, g, b;
    float h, s, v;
    uint8_t cmax, cmin, cdes;
    uint32_t color;

    r = (uint8_t) (rgb >> 16);
    g = (uint8_t) (rgb >> 8);
    b = (uint8_t) (rgb);
    
    cmax = r > g ? r : g;
    if (b > cmax)
    {
        cmax = b;
    }
    cmin = r < g ? r : g;
    if (b < cmin)
    {
        cmin = b;
    }
    
    cdes = cmax - cmin;

    v = cmax / 255.0f;
    s = cmax == 0 ? 0 : cdes / (float) cmax;
    h = 0;

    if (cmax == r && g >= b)
    {
        h = ((g - b) * 60.0f / cdes) + 0;
    }
    else if (cmax == r && g < b)
    {
        h = ((g - b) * 60.0f / cdes) + 360;
    }
    else if (cmax == g)
    {
        h = ((b - r) * 60.0f / cdes) + 120;
    }
    else
    {
        h = ((r - g) * 60.0f / cdes) + 240;
    }
    
    v *= k;

    float f, p, q, t;
    float rf = 0, gf = 0, bf = 0;
    int i = ((int) (h / 60) % 6);
    f = (h / 60) - i;
    p = v * (1 - s);
    q = v * (1 - f * s);
    t = v * (1 - (1 - f) * s);
    switch (i) 
    {
        case 0:
        {
            rf = v;
            gf = t;
            bf = p;
            break;
        }
        case 1:
        {
            rf = q;
            gf = v;
            bf = p;
            break;
        }
        case 2:
        {
            rf = p;
            gf = v;
            bf = t;
            break;
        }
        case 3:
        {
            rf = p;
            gf = q;
            bf = v;
            break;
        }
        case 4:
        {
            rf = t;
            gf = p;
            bf = v;
            break;
        }
        case 5:
        {
            rf = v;
            gf = p;
            bf = q;
            break;
        }
        default:
        break;
    }

    r = (uint8_t) (rf * 255.0);
    g = (uint8_t) (gf * 255.0);
    b = (uint8_t) (bf * 255.0);

    color = ((uint32_t) r << 16) | ((uint32_t) g << 8) | b;

    return color;
}
