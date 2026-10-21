/**
 ****************************************************************************************************
 * @file        ft5206.c
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
#include "ft5206.h"
#include "ctiic.h"

/**
 * @brief       向FT5206写入一次数据
 * @param       reg : 起始寄存器地址
 * @param       buf : 数据缓缓存区
 * @param       len : 写数据长度
 * @retval      0, 成功; 1, 失败;
 */
uint8_t ft5206_wr_reg(uint16_t reg, uint8_t *buf, uint8_t len)
{
    return ct_iic_write_reg(FT5206_CMD_WR, reg, CT_IIC_REG8, buf, len);
}

/**
 * @brief       从FT5206读出一次数据
 * @param       reg : 起始寄存器地址
 * @param       buf : 数据缓缓存区
 * @param       len : 读数据长度
 * @retval      0, 成功; 1, 失败;
 */
uint8_t ft5206_rd_reg(uint16_t reg, uint8_t *buf, uint8_t len)
{
    return ct_iic_read_reg(FT5206_CMD_WR, reg, CT_IIC_REG8, buf, len);
}

/**
 * @brief       初始化FT5206触摸屏
 * @param       无
 * @retval      0, 初始化成功; 1, 初始化失败;
 */
uint8_t ft5206_init(void)
{
    GPIO_InitTypeDef gpio_init_struct = {0};
    uint8_t temp[2] = {0};

    FT5206_RST_GPIO_CLK_ENABLE();   /* RST引脚时钟使能 */
    FT5206_INT_GPIO_CLK_ENABLE();   /* INT引脚时钟使能 */

    gpio_init_struct.Pin = FT5206_RST_GPIO_PIN;
    gpio_init_struct.Mode = GPIO_MODE_OUTPUT_PP;            /* 推挽输出 */
    gpio_init_struct.Pull = GPIO_PULLUP;                    /* 上拉 */
    gpio_init_struct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;     /* 高速 */
    HAL_GPIO_Init(FT5206_RST_GPIO_PORT, &gpio_init_struct); /* 初始化RST引脚 */

    gpio_init_struct.Pin = FT5206_INT_GPIO_PIN;
    gpio_init_struct.Mode = GPIO_MODE_INPUT;                /* 输入 */
    gpio_init_struct.Pull = GPIO_PULLUP;                    /* 上拉 */
    gpio_init_struct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;     /* 高速 */
    HAL_GPIO_Init(FT5206_INT_GPIO_PORT, &gpio_init_struct); /* 初始化INT引脚 */

    if (ct_iic_init() != 0U) { return 1U; }
    FT5206_RST(0);      /* 复位 */
    HAL_Delay(20);
    FT5206_RST(1);      /* 释放复位 */
    HAL_Delay(50);
    temp[0] = 0;
    if (ft5206_wr_reg(FT5206_DEVIDE_MODE, temp, 1) != 0U) { return 1U; }
    if (ft5206_wr_reg(FT5206_ID_G_MODE, temp, 1) != 0U) { return 1U; }
    temp[0] = 22;                               /* 触摸有效值，22，越小越灵敏 */
    if (ft5206_wr_reg(FT5206_ID_G_THGROUP, temp, 1) != 0U) { return 1U; }
    temp[0] = 12;                               /* 激活周期，不能小于12，最大14 */
    if (ft5206_wr_reg(FT5206_ID_G_PERIODACTIVE, temp, 1) != 0U) { return 1U; }
    
    /* 读取版本号，参考值：0x3003 */
    if (ft5206_rd_reg(FT5206_ID_G_LIB_VERSION, &temp[0], 2) != 0U)
    {
        return 1U;
    }

    if ((temp[0] == 0X30 && temp[1] == 0X03) || temp[1] == 0X01 || temp[1] == 0X02 || (temp[0] == 0x0 && temp[1] == 0X0))   /* 版本:0X3003/0X0001/0X0002/CST340 */
    {
        return 0;
    }

    return 1;
}

/* FT5206 5个触摸点 对应的寄存器表 */
const uint16_t FT5206_TPX_TBL[5] = {FT5206_TP1_REG, FT5206_TP2_REG, FT5206_TP3_REG, FT5206_TP4_REG, FT5206_TP5_REG};

/**
 * @brief       扫描触摸屏(采用查询方式)
 * @param       mode : 电容屏未用到次参数, 为了兼容电阻屏
 * @retval      当前触屏状态
 *   @arg       0, 触屏无触摸; 
 *   @arg       1, 触屏有触摸;
 */
uint8_t ft5206_scan(uint8_t mode)
{
    uint8_t sta = 0;
    uint8_t buf[4] = {0};
    uint8_t count;
    uint8_t i;
    uint8_t first_valid = 0xFFU;
    uint16_t valid_points = 0U;
    uint16_t x[5] = {0};
    uint16_t y[5] = {0};
    static uint8_t t = 0;                               /* 控制查询间隔,从而降低CPU占用率 */

    (void)mode;
    t++;
    if (t > 240U) { t = 10U; }
    /* A skipped poll is not a release report. */
    if ((t % 10U != 0U) && (t >= 10U)) { return 0U; }

    if (ft5206_rd_reg(FT5206_REG_NUM_FINGER, &sta, 1U) != 0U)
    {
        tp_dev.sta = 0U;
        return 0U;
    }
    count = sta & 0x0FU;
    if ((count == 0U) || (count > 5U))
    {
        tp_dev.sta = 0U;
        return 0U;
    }

    HAL_Delay(4);                                /* 保留原有控制器采样延时 */
    for (i = 0U; i < count; i++)
    {
        uint16_t raw_x;
        uint16_t raw_y;
        uint8_t event;
        if (ft5206_rd_reg(FT5206_TPX_TBL[i], buf, 4U) != 0U)
        {
            tp_dev.sta = 0U;
            return 0U;
        }
        raw_x = ((uint16_t)(buf[0] & 0x0FU) << 8U) + buf[1];
        raw_y = ((uint16_t)(buf[2] & 0x0FU) << 8U) + buf[3];
        event = buf[0] & 0xC0U; /* 00: DOWN, 01: UP, 10: CONTACT, 11: reserved. */
        if ((event != 0x00U) && (event != 0x80U)) { continue; }
        if ((tp_dev.touchtype & 0x01U) != 0U)
        {
            x[i] = raw_y;
            y[i] = raw_x;
        }
        else
        {
            if (raw_x >= TOUCH_LCD_WIDTH) { continue; }
            x[i] = TOUCH_LCD_WIDTH - 1U - raw_x;
            y[i] = raw_y;
        }
        if ((x[i] < TOUCH_LCD_WIDTH) && (y[i] < TOUCH_LCD_HEIGHT))
        {
            valid_points |= (uint16_t)(1U << i);
            if (first_valid == 0xFFU) { first_valid = i; }
        }
    }
    if (first_valid == 0xFFU)
    {
        tp_dev.sta = 0U;
        return 0U;
    }
    /* Publish only after the whole report has been read. (0,0) is a valid
     * screen coordinate; validity comes from event/count/bounds, not zeros. */
    for (i = 0U; i < count; i++)
    {
        tp_dev.x[i] = x[i];
        tp_dev.y[i] = y[i];
    }
    if (first_valid != 0U)
    {
        tp_dev.x[0] = x[first_valid];
        tp_dev.y[0] = y[first_valid];
    }
    tp_dev.sta = valid_points | TP_PRES_DOWN | TP_CATH_PRES;
    t = 0U;
    return 1U;
}




























