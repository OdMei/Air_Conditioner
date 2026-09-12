/**
 ****************************************************************************************************
 * @file        oled.c
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

#include "oled.h"
#include "oledfont.h"

/* 
 * OLED的显存
 * 每个字节表示8个像素，128表示有128列，8表示有64行，高位表示高行数。
 * 比如：g_oled_gram[0][0]，包含了第一列，第1~8行的数据，g_oled_gram[0][0].0，即表示坐标(0,0)
 * 类似的：g_oled_gram[1][0].1，表示坐标(1,1)，g_oled_gram[10][1].2，表示坐标(10,10)。
 * 
 * 存放格式如下（高位表示高行数）
 * [0]0 1 2 3 ... 127
 * [1]0 1 2 3 ... 127
 * [2]0 1 2 3 ... 127
 * [3]0 1 2 3 ... 127
 * [4]0 1 2 3 ... 127
 * [5]0 1 2 3 ... 127
 * [6]0 1 2 3 ... 127
 * [7]0 1 2 3 ... 127
 */
static uint8_t g_oled_gram[128][8];

#if (OLED_MODE == 1)    /* OLED使用8080并口模式 */
/**
 * @brief   通过拼凑的方法向OLED输出一个8位数据
 * @param   data: 要输出的数据
 * @retval  无
 */
static void oled_data_out(uint8_t data)
{
    GPIOB->ODR &= ~((0x1 << 7) | (0x1 << 9));                                                                                           /* GPIOB[7,9]清零 */
    GPIOB->ODR |= ((((data >> 7) & 0x1) << 7) | (((data >> 3) & 0x1) << 9));                                                            /* data[7,3] --> GPIOB[7,9] */

    GPIOD->ODR &= ~(0x1 << 7);                                                                                                          /* GPIOD[7]清零 */
    GPIOD->ODR |= (((data >> 0) & 0x1) << 7);                                                                                           /* data[0] --> GPIOD[7] */

    GPIOE->ODR &= ~((0x1 << 0) | (0x1 << 5) | (0x1 << 6) | (0x1 << 8));                                                                 /* GPIOE[0,5,6,8]清零 */
    GPIOE->ODR |= ((((data >> 2) & 0x1) << 0) | (((data >> 5) & 0x1) << 5) | (((data >> 1) & 0x1) << 6) | (((data >> 4) & 0x1) << 8));  /* data[2,5,1,4] --> GPIOE[0,5,6,8] */

    GPIOH->ODR &= ~(0x1 << 9);                                                                                                          /* GPIOH[9]清零 */
    GPIOH->ODR |= (((data >> 6) & 0x1) << 9);                                                                                           /* data[6] --> GPIOH[9] */
}

/**
 * @brief   向OLED写入一个字节
 * @param   data: 要输出的数据
 * @param   cmd : 命令/数据标志
 * @arg     OLED_CMD : 命令
 * @arg     OLED_DATA: 数据
 * @retval  无
 */
static void oled_wr_byte(uint8_t data, uint8_t cmd)
{
    oled_data_out(data);
    OLED_RS(cmd);
    OLED_CS(0);
    OLED_WR(0);
    OLED_WR(1);
    OLED_CS(1);
    OLED_RS(1);
}
#else   /* OLED使用4线SPI模式 */
/**
 * @brief   向OLED写入一个字节
 * @param   data: 要输出的数据
 * @param   cmd : 命令/数据标志
 * @arg     OLED_CMD : 命令
 * @arg     OLED_DATA: 数据
 * @retval  无
 */
static void oled_wr_byte(uint8_t data, uint8_t cmd)
{
    uint8_t i;

    OLED_RS(cmd);
    OLED_CS(0);

    for (i=0; i<8; i++)
    {
        OLED_SCLK(0);
        if ((data & 0x80) != 0)
        {
            OLED_SDIN(1);
        }
        else
        {
            OLED_SDIN(0);
        }
        OLED_SCLK(1);
        data <<= 1;
    }

    OLED_CS(1);
    OLED_RS(1);
}
#endif

/**
 * @brief   更新显存到OLED
 * @param   无
 * @retval  无
 */
void oled_refresh_gram(void)
{
    uint8_t i;
    uint8_t n;

    for (i=0; i<8; i++)
    {
        oled_wr_byte(0xB0 + i, OLED_CMD);   /* 设置页地址（0~7） */
        oled_wr_byte(0x00, OLED_CMD);       /* 设置显示位置——列低地址 */
        oled_wr_byte(0x10, OLED_CMD);       /* 设置显示位置——列高地址 */
        for (n=0; n<128; n++)
        {
            oled_wr_byte(g_oled_gram[n][i], OLED_DATA);
        }
    }
}

/**
 * @brief   开启OLED显示
 * @param   无
 * @retval  无
 */
void oled_display_on(void)
{
    oled_wr_byte(0x8D, OLED_CMD);   /* SET DCDC命令 */
    oled_wr_byte(0x14, OLED_CMD);   /* DCDC ON */
    oled_wr_byte(0xAF, OLED_CMD);   /* DISPLAY ON */
}

/**
 * @brief   关闭OLED显示
 * @param   无
 * @retval  无
 */
void oled_display_off(void)
{
    oled_wr_byte(0x8D, OLED_CMD);   /* SET DCDC命令 */
    oled_wr_byte(0x10, OLED_CMD);   /* DCDC OFF */
    oled_wr_byte(0xAE, OLED_CMD);   /* DISPLAY OFF */
}

/**
 * @brief   清屏函数
 * @note    清完屏，整个屏幕就是黑色的，跟没点亮一样
 * @param   无
 * @retval  无
 */
void oled_clear(void)
{
    uint8_t i;
    uint8_t n;

    for (i=0; i<8; i++)                 /* 显存清零 */
    {
        for (n=0; n<128; n++)
        {
            g_oled_gram[n][i] = 0x00;
        }
    }

    oled_refresh_gram();                /* 更新显示 */
}

/**
 * @brief   OLED画点
 * @param   x  : 点的X坐标，范围：0~127
 * @param   y  : 点的Y坐标，范围：0~63
 * @param   dot: 点的颜色
 * @arg     0: 清零
 * @arg     1: 填充
 * @retval  无
 */
void oled_draw_point(uint8_t x, uint8_t y, uint8_t dot)
{
    uint8_t pos;
    uint8_t bx;
    uint8_t temp;

    if ((x > 127) || (y > 63))      /* 坐标超出范围 */
    {
        return;
    }

    pos = y >> 3;                   /* 计算GRAM里面的Y坐标所在的字节，每个字节可以存储8个行坐标 */
    bx = y & 0x7;                   /* 取余数，方便计算Y坐标在对应字节里面的位置，即行坐标（Y）的位置 */
    temp = 1 << bx;                 /* 高位表示高行号，得到Y对应的bit位置，将改bit先置1 */

    if (dot == 1)                   /* 填充点 */
    {
        g_oled_gram[x][pos] |= temp;
    }
    else                            /* 清零点 */
    {
        g_oled_gram[x][pos] &= ~temp;
    }
}

/**
 * @brief   OLED填充区域填充
 * @param   x1 : 起点X坐标，范围：0~x2
 * @param   y1 : 起点Y坐标，范围：0~y2
 * @param   x2 : 终点X坐标，范围：x1~127
 * @param   y2 : 终点Y坐标，范围：y1~64
 * @param   dot: 点的颜色
 * @arg     0: 清零
 * @arg     1: 填充
 * @retval  无
 */
void oled_fill(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, uint8_t dot)
{
    uint8_t x;
    uint8_t y;

    for (x=x1; x<=x2; x++)              /* 区域内逐个画点 */
    {
        for (y=y1; y<=y2; y++)
        {
            oled_draw_point(x, y, dot);
        }
    }

    oled_refresh_gram();                /* 更新显示 */
}

/**
 * @brief   在指定位置显示一个字符
 * @param   x   : 指定位置的X坐标，范围：0~127
 * @param   y   : 指定位置的Y坐标，范围：0~63
 * @param   chr : 要显示的字符，范围：' '~'~'
 * @param   size: 字体
 * @arg     12: 12*12 ASCII字符
 * @arg     16: 16*16 ASCII字符
 * @arg     24: 24*24 ASCII字符
 * @param   mode: 显示模式
 *  @arg    0: 反白显示
 *  @arg    1: 正常显示
 * @retval  无
 */
void oled_show_char(uint8_t x, uint8_t y, uint8_t chr, uint8_t size, uint8_t mode)
{
    uint8_t csize;
    uint8_t *pfont;
    uint8_t t;
    uint8_t t1;
    uint8_t temp;
    uint8_t y0;

    csize = ((size >> 3) + (((size & 0x7) != 0) ? 1 : 0)) * (size >> 1);    /* 计算所选字体对应一个字符所占的字节数 */
    chr -= ' ';                                                             /* 计算偏移后的值，因为字库是从空格开始的 */

    if (size == 12)                                                         /* 12*12 ASCII字符 */
    {
        pfont = (uint8_t *)oled_asc2_1206[chr];
    }
    else if (size == 16)                                                    /* 16*16 ASCII字符 */
    {
        pfont = (uint8_t *)oled_asc2_1608[chr];
    }
    else if (size == 24)                                                    /* 24*24 ASCII字符 */
    {
        pfont = (uint8_t *)oled_asc2_2412[chr];
    }
    else                                                                    /* 没有对应的字体 */
    {
        return;
    }

    y0 = y;
    for (t=0; t<csize; t++)
    {
        temp = pfont[t];
        for (t1=0; t1<8; t1++)
        {
            if ((temp & 0x80) != 0)
            {
                oled_draw_point(x, y, mode);
            }
            else
            {
                oled_draw_point(x, y, !mode);
            }

            temp <<= 1;
            y++;
            if ((y - y0) == size)
            {
                y = y0;
                x++;
                break;
            }
        }
    }
}

/**
 * @brief   平方函数
 * @param   m: 底数
 * @param   n: 指数
 * @retval  m^n
 */
static uint32_t oled_pow(uint8_t m, uint8_t n)
{
    uint32_t result = 1;

    while (n--)
    {
        result *= m;
    }

    return result;
}

/**
 * @brief   显示len个数字
 * @param   x   : 起始X坐标，范围：0~127
 * @param   y   : 起始Y坐标，范围：0~63
 * @param   num : 数值，范围：0~2^32
 * @param   len : 显示数字的位数
 * @param   size: 字体
 * @arg     12: 12*12 ASCII字符
 * @arg     16: 16*16 ASCII字符
 * @arg     24: 24*24 ASCII字符
 * @retval  无
 */
void oled_show_num(uint8_t x, uint8_t y, uint32_t num, uint8_t len, uint8_t size)
{
    uint8_t t;
    uint8_t temp;
    uint8_t enshow = 0;

    for (t=0; t<len; t++)                                               /* 按总显示位数循环 */
    {
        temp = (num / oled_pow(10, len - t - 1)) % 10;                  /* 获取对应位的数字 */
        if((enshow == 0) && (t < (len - 1)))                            /* 没有使能显示，且还有位要显示 */
        {
            if (temp == 0)
            {
                oled_show_char(x + (size >> 1) * t, y, ' ', size, 1);   /* 显示空格，占位 */
                continue;                                               /* 继续下一个位 */
            }
            else
            {
                enshow = 1;                                             /* 使能显示 */
            }
        }

        oled_show_char(x + (size >> 1) * t, y, temp + '0', size, 1);    /* 显示字符 */
    }
}

/**
 * @brief   显示字符串
 * @param   x   : 起始X坐标，范围：0~127
 * @param   y   : 起始Y坐标，范围：0~63
 * @param   *p  : 字符串指针
 * @param   size: 字体
 * @arg     12: 12*12 ASCII字符
 * @arg     16: 16*16 ASCII字符
 * @arg     24: 24*24 ASCII字符
 * @retval  无
 */
void oled_show_string(uint8_t x, uint8_t y, const char *p, uint8_t size)
{
    while ((*p <= '~') && (*p >= ' '))      /* 判断是否为非法字符 */
    {
        if (x > (128 - (size >> 1)))        /* 宽度越界 */
        {
            x = 0;                          /* 换行 */
            y += size;
        }

        if (y > (64 - size))                /* 高度越界 */
        {
            x = 0;
            y = 0;
            oled_clear();
        }

        oled_show_char(x, y, *p, size, 1);  /* 显示一个字符 */
        x += (size >> 1);                   /* ASCII字符宽度为高度的一半 */
        p++;
    }
}

/**
 * @brief   初始化OLED
 * @param   无
 * @retval  无
 */
void oled_init(void)
{
#if (OLED_MODE == 1)                /* OLED使用8080并口模式 */
    OLED_WR(1);
    OLED_RD(1);
#else                               /* OLED使用4线SPI模式 */
    OLED_SDIN(1);
    OLED_SCLK(1);
#endif

    OLED_CS(1);
    OLED_RS(1);

    OLED_RST(0);
    HAL_Delay(100);
    OLED_RST(1);

    oled_wr_byte(0xAE, OLED_CMD);   /* 关闭显示 */
    oled_wr_byte(0xD5, OLED_CMD);   /* 设置时钟分频因子，震荡频率 */
    oled_wr_byte(0x50, OLED_CMD);   /* [3:0]：分频因子；[7:4]：震荡频率 */
    oled_wr_byte(0xA8, OLED_CMD);   /* 设置驱动路数 */
    oled_wr_byte(0x3F, OLED_CMD);   /* 默认0x3F（1/64） */
    oled_wr_byte(0xD3, OLED_CMD);   /* 设置显示偏移 */
    oled_wr_byte(0x00, OLED_CMD);   /* 默认为0 */
    oled_wr_byte(0x40, OLED_CMD);   /* 设置显示开始行，[5:0]：行数 */
    oled_wr_byte(0x8D, OLED_CMD);   /* 电荷泵设置 */
    oled_wr_byte(0x14, OLED_CMD);   /* bit2：开启/关闭 */
    oled_wr_byte(0x20, OLED_CMD);   /* 设置内存地址模式 */
    oled_wr_byte(0x02, OLED_CMD);   /* [1:0]：00：列地址模式；01：行地址模式；10：页地址模式，默认10 */
    oled_wr_byte(0xA1, OLED_CMD);   /* 段重定义设置，bit0：0：0->0；1：0->127 */
    oled_wr_byte(0xC8, OLED_CMD);   /* 设置COM扫描方向，bit3：0：普通模式；1：重定义模式COM[N-1]->COM0，N：驱动路数 */
    oled_wr_byte(0xDA, OLED_CMD);   /* 设置COM硬件引脚配置 */
    oled_wr_byte(0x12, OLED_CMD);   /* [5:4]：配置 */
    oled_wr_byte(0x81, OLED_CMD);   /* 对比度设置 */
    oled_wr_byte(0xEF, OLED_CMD);   /* 1~255，默认0x7F（亮度设置，越大越亮） */
    oled_wr_byte(0xD9, OLED_CMD);   /* 设置预充电周期 */
    oled_wr_byte(0xF1, OLED_CMD);   /* [3:0]：PHASE 1;[7:4]：PHASE 2; */
    oled_wr_byte(0xDB, OLED_CMD);   /* 设置VCOMH电压倍率 */
    oled_wr_byte(0x30, OLED_CMD);   /* [6:4]：000：0.65*vcc；001：0.77*vcc；011：0.83*vcc */
    oled_wr_byte(0xA4, OLED_CMD);   /* 全局显示开启，bit0：1：开启；0：关闭（白屏/黑屏） */
    oled_wr_byte(0xA6, OLED_CMD);   /* 设置显示方式，bit0：1：反相显示；0：正常显示 */
    oled_wr_byte(0xAF, OLED_CMD);   /* 开启显示 */
    oled_clear();                   /* 清屏 */
}
