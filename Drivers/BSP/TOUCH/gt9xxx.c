/**
 ****************************************************************************************************
 * @file        gt9xxx.c
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2025-01-13
 * @brief       Touch驱动代码
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

#include "string.h"
#include "touch.h"
#include "gt9xxx.h"
#include "ctiic.h"

/* 注意: 除了GT9271支持10点触摸之外, 其他触摸芯片只支持 5点触摸 */
uint8_t g_gt_tnum = 5;      /* 默认支持的触摸屏点数(5点触摸) */

/**
 * @brief       向gt9xxx写入一次数据
 * @param       reg : 起始寄存器地址
 * @param       buf : 数据缓缓存区
 * @param       len : 写数据长度
 * @retval      0, 成功; 1, 失败;
 */
uint8_t gt9xxx_wr_reg(uint16_t reg, uint8_t *buf, uint8_t len)
{
    return ct_iic_write_reg(GT9XXX_CMD_WR, reg, CT_IIC_REG16, buf, len);
}

/**
 * @brief       从gt9xxx读出一次数据
 * @param       reg : 起始寄存器地址
 * @param       buf : 数据缓缓存区
 * @param       len : 读数据长度
 * @retval      0, 成功; 1, 失败;
 */
uint8_t gt9xxx_rd_reg(uint16_t reg, uint8_t *buf, uint8_t len)
{
    return ct_iic_read_reg(GT9XXX_CMD_WR, reg, CT_IIC_REG16, buf, len);
}

/**
 * @brief       初始化gt9xxx触摸屏
 * @param       无
 * @retval      0, 初始化成功; 1, 初始化失败;
 */
uint8_t gt9xxx_init(void)
{
    GPIO_InitTypeDef gpio_init_struct = {0};
    uint8_t temp[5] = {0};

    GT9XXX_RST_GPIO_CLK_ENABLE();   /* RST引脚时钟使能 */
    GT9XXX_INT_GPIO_CLK_ENABLE();   /* INT引脚时钟使能 */

    gpio_init_struct.Pin = GT9XXX_RST_GPIO_PIN;
    gpio_init_struct.Mode = GPIO_MODE_OUTPUT_PP;            /* 推挽输出 */
    gpio_init_struct.Pull = GPIO_PULLUP;                    /* 上拉 */
    gpio_init_struct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;     /* 高速 */
    HAL_GPIO_Init(GT9XXX_RST_GPIO_PORT, &gpio_init_struct); /* 初始化RST引脚 */

    gpio_init_struct.Pin = GT9XXX_INT_GPIO_PIN;
    gpio_init_struct.Mode = GPIO_MODE_INPUT;                /* 输入 */
    gpio_init_struct.Pull = GPIO_PULLUP;                    /* 上拉 */
    gpio_init_struct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;     /* 高速 */
    HAL_GPIO_Init(GT9XXX_INT_GPIO_PORT, &gpio_init_struct); /* 初始化INT引脚 */

    if (ct_iic_init() != 0U) { return 1U; }
    GT9XXX_RST(0);      /* 复位 */
    HAL_Delay(10);
    GT9XXX_RST(1);      /* 释放复位 */
    HAL_Delay(10);

    /* INT引脚模式设置, 输入模式, 浮空输入 */
    gpio_init_struct.Pin = GT9XXX_INT_GPIO_PIN;
    gpio_init_struct.Mode = GPIO_MODE_INPUT;                /* 输入 */
    gpio_init_struct.Pull = GPIO_NOPULL;                    /* 不带上下拉，浮空模式 */
    gpio_init_struct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;     /* 高速 */
    HAL_GPIO_Init(GT9XXX_INT_GPIO_PORT, &gpio_init_struct); /* 初始化INT引脚 */

    HAL_Delay(100);
    if (gt9xxx_rd_reg(GT9XXX_PID_REG, temp, 4) != 0U)
    {
        return 1U;
    }
    temp[4] = 0;

    /* 判断一下是否是特定的触摸屏 */
    if ( strcmp((char *)temp, "911") && strcmp((char *)temp, "9147") && strcmp((char *)temp, "1158") && strcmp((char *)temp, "9271"))
    {
        return 1;   /* 若不是触摸屏用到的GT911/9147/1158/9271，则初始化失败，需硬件查看触摸IC型号以及查看时序函数是否正确 */
    }
    
    g_gt_tnum = (strcmp((char *)temp, "9271") == 0) ? 10U : 5U;
    
    temp[0] = 0X02;
    if (gt9xxx_wr_reg(GT9XXX_CTRL_REG, temp, 1) != 0U)
    {
        return 1U;
    }
    
    HAL_Delay(10);
    
    temp[0] = 0X00;
    if (gt9xxx_wr_reg(GT9XXX_CTRL_REG, temp, 1) != 0U)
    {
        return 1U;
    }

    return 0;
}

/* GT9XXX 10个触摸点(最多) 对应的寄存器表 */
const uint16_t GT9XXX_TPX_TBL[10] =
{
    GT9XXX_TP1_REG, GT9XXX_TP2_REG, GT9XXX_TP3_REG, GT9XXX_TP4_REG, GT9XXX_TP5_REG,
    GT9XXX_TP6_REG, GT9XXX_TP7_REG, GT9XXX_TP8_REG, GT9XXX_TP9_REG, GT9XXX_TP10_REG,
};

/**
 * @brief       扫描触摸屏(采用查询方式)
 * @param       mode : 电容屏未用到次参数, 为了兼容电阻屏
 * @retval      当前触屏状态
 *   @arg       0, 触屏无触摸; 
 *   @arg       1, 触屏有触摸;
 */
uint8_t gt9xxx_scan(uint8_t mode)
{
    uint8_t points[76] = {0};
    uint8_t clear = 0U;
    uint8_t count;
    uint8_t i;
    uint8_t first_valid = 0xFFU;
    static uint8_t t;

    (void)mode;
    t++;
    if (t > 240U) { t = 10U; }
    if ((t % 10U != 0U) && (t >= 10U)) { return 0U; }

    if (gt9xxx_rd_reg(GT9XXX_GSTID_REG, &mode, 1U) != 0U)
    {
        tp_dev.sta = 0U;
        return 0U;
    }
    /* Leave the previous touch state unchanged until a new complete report. */
    if ((mode & 0x80U) == 0U) { return 0U; }
    count = mode & 0x0FU;
    if ((count > g_gt_tnum) || (count > 10U))
    {
        (void)gt9xxx_wr_reg(GT9XXX_GSTID_REG, &clear, 1U);
        tp_dev.sta = 0U;
        return 0U;
    }
    /* X/Y records are eight bytes apart. Fetch all coordinates in one
     * contiguous transaction, then acknowledge the report only after reading. */
    if ((count != 0U) &&
        (gt9xxx_rd_reg(GT9XXX_TP1_REG, points,
                       (uint8_t)((count - 1U) * 8U + 4U)) != 0U))
    {
        tp_dev.sta = 0U;
        return 0U;
    }
    if (gt9xxx_wr_reg(GT9XXX_GSTID_REG, &clear, 1U) != 0U)
    {
        tp_dev.sta = 0U;
        return 0U;
    }
    if (count == 0U)
    {
        tp_dev.sta = 0U;
        return 0U;
    }

    tp_dev.sta = (uint16_t)((1U << count) - 1U) | TP_PRES_DOWN | TP_CATH_PRES;
    for (i = 0U; i < count; i++)
    {
        const uint8_t *buf = &points[i * 8U];
        if ((tp_dev.touchtype & 0x01U) != 0U)
        {
            tp_dev.x[i] = ((uint16_t)buf[1] << 8U) + buf[0];
            tp_dev.y[i] = ((uint16_t)buf[3] << 8U) + buf[2];
        }
        else
        {
            uint16_t raw_y = ((uint16_t)buf[3] << 8U) + buf[2];
            if (raw_y >= TOUCH_LCD_WIDTH)
            {
                tp_dev.sta &= (uint16_t)~(1U << i);
                continue;
            }
            tp_dev.x[i] = TOUCH_LCD_WIDTH - 1U - raw_y;
            tp_dev.y[i] = ((uint16_t)buf[1] << 8U) + buf[0];
        }
        if ((tp_dev.x[i] < TOUCH_LCD_WIDTH) && (tp_dev.y[i] < TOUCH_LCD_HEIGHT))
        {
            if (first_valid == 0xFFU) { first_valid = i; }
        }
        else
        {
            tp_dev.sta &= (uint16_t)~(1U << i);
        }
    }
    if (first_valid == 0xFFU)
    {
        tp_dev.sta = 0U;
        return 0U;
    }
    if (first_valid != 0U)
    {
        tp_dev.x[0] = tp_dev.x[first_valid];
        tp_dev.y[0] = tp_dev.y[first_valid];
    }
    t = 0U;
    return 1U;
}




























