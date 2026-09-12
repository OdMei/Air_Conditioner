/**
 ****************************************************************************************************
 * @file        ws2812b.c
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2025-03-06
 * @brief       WS2812B(DMA+PWM模式) 驱动代码
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

#include "ws2812b.h"
#include "string.h"

extern TIM_HandleTypeDef htim4;                    /* 定时器x句柄 */
extern DMA_HandleTypeDef handle_GPDMA1_Channel9;   /* DMA句柄 */  

/* 用于存放10个RGBLED灯的颜色值 */
uint32_t g_grb888_color[LED_NUM] = 
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

/* 用于存放WS2812B逻辑1和逻辑0对应的CCRx值， */
uint8_t g_pixel_buf[RESET_NUM + LED_NUM][DATA_SIZE] = {0}; /* 发送数据缓冲区 */

/**
 * @brief       发送数据给WS2812B
 * @param       无
 * @retval      无
 */
void rgbled_dats_send(void)
{
	SCB_CleanDCache_by_Addr(g_pixel_buf,sizeof(g_pixel_buf)); /* 用于将数据Cache清除，清除的意思是将Cache Line中标记为dirty的数据写入到相应的存储区 */
    HAL_TIM_PWM_Start_DMA(&htim4, TIM_CHANNEL_3, (uint32_t *)g_pixel_buf, sizeof(g_pixel_buf)); /* 使用DMA将数据发送到定时器的CCRx */
}

/**
 * @brief       复位WS2812B
 * @param       无
 * @retval      无
 */
void ws2812b_reset(void)
{
    uint32_t color_buf[10];                             /* 临时存放颜色值 */
    memset(color_buf, GRB888_BLACK, sizeof(color_buf)); /* 将10个RGBLED灯全部填充为黑色（熄灭状态） */
    ws2812b_display(LED_NUM, color_buf);
}

/**
 * @brief       将GRB888的颜色值转换成逻辑1和逻辑0对应的CCRx值并存放到g_pixel_buf里
 * @note        可指定第几个灯
 * @param       led_num ： 需控制灯的数量
 * @param       color   ： GRB888颜色值
 * @retval      无
 */
void ws2812b_write_color(uint16_t led_num, uint32_t color)
{
    uint8_t i; 
    if(led_num > LED_NUM)return;                            /* 防止写入LED数量大于LED总数 */
    /* 这里是对 g_pixel_buf[led_id][i]写入一个周期内高电平的持续时间（用于存放比较值寄存器CCR4的值）*/
    for(i = 0; i < DATA_SIZE; i++) 
    {
        g_pixel_buf[RESET_NUM + led_num][i] = (((color << i) & 0x800000) ? HIGH_LEVEL : LOW_LEVEL);  
    }
}

/**
 * @brief   点亮多个灯并显示相应的颜色
 * @param   led_num ：   灯的数量
 * @param   color   ：   存放GRB888颜色值指针
 * @retval  无
 */
void ws2812b_display(uint8_t total_num, uint32_t *color)
{
    uint8_t i = 0;
    for(i = 0; i < total_num; i++)
    {
        ws2812b_write_color(i, color_change_brigh(color[i], 0.05));
    }
    rgbled_dats_send();     /* 将g_pixel_buf发送到定时器的CCRx中，生成相应波形 */
}


/**
 * @brief       初始化WS2812B相关IO口,以及配置定时器和DMA功能 并使能时钟
 * @param       无
 * @retval      无
 */
void ws2812b_init(void)
{

__HAL_TIM_ENABLE_OCxPRELOAD(&htim4, GTIM_TIMX_PWM_CHY);      /* 很关键，使能CCRX的影子寄存器，当修改CCRX的值时，等产生更新事件在生效 */
    ws2812b_reset();                  /* 复位WS2812B */
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
