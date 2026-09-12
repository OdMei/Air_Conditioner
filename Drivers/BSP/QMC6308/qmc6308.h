/**
 ****************************************************************************************************
 * @file        qmc6308.h
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2025-01-13
 * @brief       QMC6308 磁力计驱动代码
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

#ifndef __QMC6308_H
#define __QMC6308_H

#include "main.h"

/*******************************************************************************************************/
#define QMC6308_ADDR                (0X2C << 1)    /* QMC6308 IIC器件地址 */
#define QMC6308_CHIP_ID_REG         0x00    /* 芯片ID寄存器 */
#define QMC6308_DEVICE_ID           0x80    /* ID号 */

/* QMC6308寄存器地址 */
#define QMC6308_DATA_X_L_REG        0x01    /* x轴低8位数据输出寄存器 */
#define QMC6308_DATA_X_H_REG        0x02    /* x轴高8位数据输出寄存器 */
#define QMC6308_DATA_Y_L_REG        0x03    /* y轴低8位数据输出寄存器 */
#define QMC6308_DATA_Y_H_REG        0x04    /* y轴高8位数据输出寄存器 */
#define QMC6308_DATA_Z_L_REG        0x05    /* z轴低8位数据输出寄存器 */
#define QMC6308_DATA_Z_H_REG        0x06    /* z轴高8位数据输出寄存器 */
#define QMC6308_STATUS_REG          0x09    /* 状态寄存器 */
#define QMC6308_CTL_REG1            0x0A    /* 控制寄存器1 */
#define QMC6308_CTL_REG2            0x0B    /* 控制寄存器2 */
#define QMC6308_CTL_REG3            0x0D    /* 控制寄存器3 */
#define QMC6308_CTL_REG4            0x0F    /* 控制寄存器4 */

/* Magnetic Sensor Operating Mode MODE[1:0]*/
#define QMC6308_SUSPEND_MODE        0x00
#define QMC6308_NORMAL_MODE         0x01
#define QMC6308_SINGLE_MODE         0x02
#define QMC6308_CONTINOUS_MODE      0x03

/*data output rate ODR[3:2]*/
#define OUTPUT_DATA_RATE_200HZ      0x03
#define OUTPUT_DATA_RATE_100HZ      0x02
#define OUTPUT_DATA_RATE_50HZ       0x01
#define OUTPUT_DATA_RATE_10HZ       0x00

/*oversample Ratio  OSR1[5:4]*/
#define OVERSAMPLE_RATE_8           0x00
#define OVERSAMPLE_RATE_4           0x01
#define OVERSAMPLE_RATE_2           0x02
#define OVERSAMPLE_RATE_1           0x03

#define SET_RESET_ON                0x00
#define SET_ONLY_ON                 0x01
#define SET_RESET_OFF               0x02

/* XYZ轴 */
enum
{
    AXIS_X = 0,
    AXIS_Y = 1,
    AXIS_Z = 2,
    AXIS_TOTAL
};

typedef struct
{
    signed char sign[3];    /* 三轴符号 */
    unsigned char map[3];   /* 三轴数据存放位置 */
} qmc6308_map;

/*******************************************************************************************************/
/* 函数声明 */
uint8_t qmc6308_init(void);                                                                     /* 初始化 */
uint8_t qmc6308_read_magdata(float *pmagx, float *pmagy, float *pmagz);                         /* 读取磁力原始数据(单次) */
uint8_t qmc6308_read_magdata_average(float *pmagx, float *pmagy, float *pmagz, uint8_t times);  /* 读取磁力原始数据(平均值) */

#endif

