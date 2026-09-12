/**
 ****************************************************************************************************
 * @file        qmc6308.c
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2024-05-21
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


#include "qmc6308.h"
#include "./UART/uart.h"
#include <math.h>

extern I2C_HandleTypeDef hi2c4; /* I2C句柄 */

static qmc6308_map c_map;   /* 符号存储 */
/**
 * @brief       从QMC6308读取N字节数据
 * @note        QMC6308的命令发送, 也是用该函数实现(不带参数的命令, 也会有一个状态寄存器需要读取)
 * @param       addr  : 寄存器地址/命令
 * @param       buf   : 数据存储buf
 * @param       length: 读取长度
 * @retval      0, 操作成功
 *              其他, 操作失败
 */
uint8_t qmc6308_read_nbytes(uint8_t addr, uint8_t *buf, uint8_t length)
{
	if (HAL_I2C_Mem_Read(&hi2c4, QMC6308_ADDR, addr, I2C_MEMADD_SIZE_8BIT, buf, length, 1000)!= HAL_OK)
	{
		return 1;
	}
    return 0;
}

/**
 * @brief       QMC6308写寄存器
 * @param       reg  : 寄存器地址
 * @param       data : 写入的值
 * @retval      0, 操作成功
 *              其他, 操作失败
 */
uint8_t qmc6308_write_register(uint8_t reg, uint8_t data)
{
    if (HAL_I2C_Mem_Write(&hi2c4, QMC6308_ADDR, reg, I2C_MEMADD_SIZE_8BIT, &data, 1, 1000) != HAL_OK)
    {
    	return 1;
    }
    return 0;
}

/**
 * @brief       QMC6308读寄存器
 * @param       reg  : 寄存器地址
 * @retval      读取到的值
 *              0XFFFF, 则可能表示错误
 */
uint16_t qmc6308_read_register(uint8_t reg)
{
	uint8_t temp = 0;
    if (HAL_I2C_Mem_Read(&hi2c4, QMC6308_ADDR, reg, I2C_MEMADD_SIZE_8BIT, &temp, 1, 1000)!= HAL_OK)
	{
		return 1;
	}
    return temp;
}

/**
 * @brief       磁力计三轴符号设置
 * @param       layout ：0~8 9种方式
 * @retval      无
 */
static void qmc6308_set_layout(int layout)
{
    if(layout == 0)
    {
        c_map.sign[AXIS_X] = 1;
        c_map.sign[AXIS_Y] = 1;
        c_map.sign[AXIS_Z] = 1;
        c_map.map[AXIS_X] = AXIS_X;
        c_map.map[AXIS_Y] = AXIS_Y;
        c_map.map[AXIS_Z] = AXIS_Z;
    }
    else if(layout == 1)
    {
        c_map.sign[AXIS_X] = -1;
        c_map.sign[AXIS_Y] = 1;
        c_map.sign[AXIS_Z] = 1;
        c_map.map[AXIS_X] = AXIS_Y;
        c_map.map[AXIS_Y] = AXIS_X;
        c_map.map[AXIS_Z] = AXIS_Z;
    }
    else if(layout == 2)
    {
        c_map.sign[AXIS_X] = -1;
        c_map.sign[AXIS_Y] = -1;
        c_map.sign[AXIS_Z] = 1;
        c_map.map[AXIS_X] = AXIS_X;
        c_map.map[AXIS_Y] = AXIS_Y;
        c_map.map[AXIS_Z] = AXIS_Z;
    }
    else if(layout == 3)
    {
        c_map.sign[AXIS_X] = 1;
        c_map.sign[AXIS_Y] = -1;
        c_map.sign[AXIS_Z] = 1;
        c_map.map[AXIS_X] = AXIS_Y;
        c_map.map[AXIS_Y] = AXIS_X;
        c_map.map[AXIS_Z] = AXIS_Z;
    }
    else if(layout == 4)
    {
        c_map.sign[AXIS_X] = -1;
        c_map.sign[AXIS_Y] = 1;
        c_map.sign[AXIS_Z] = -1;
        c_map.map[AXIS_X] = AXIS_X;
        c_map.map[AXIS_Y] = AXIS_Y;
        c_map.map[AXIS_Z] = AXIS_Z;
    }
    else if(layout == 5)
    {
        c_map.sign[AXIS_X] = 1;
        c_map.sign[AXIS_Y] = 1;
        c_map.sign[AXIS_Z] = -1;
        c_map.map[AXIS_X] = AXIS_Y;
        c_map.map[AXIS_Y] = AXIS_X;
        c_map.map[AXIS_Z] = AXIS_Z;
    }
    else if(layout == 6)
    {
        c_map.sign[AXIS_X] = 1;
        c_map.sign[AXIS_Y] = -1;
        c_map.sign[AXIS_Z] = -1;
        c_map.map[AXIS_X] = AXIS_X;
        c_map.map[AXIS_Y] = AXIS_Y;
        c_map.map[AXIS_Z] = AXIS_Z;
    }
    else if(layout == 7)
    {
        c_map.sign[AXIS_X] = -1;
        c_map.sign[AXIS_Y] = -1;
        c_map.sign[AXIS_Z] = -1;
        c_map.map[AXIS_X] = AXIS_Y;
        c_map.map[AXIS_Y] = AXIS_X;
        c_map.map[AXIS_Z] = AXIS_Z;
    }
    else
    {
        c_map.sign[AXIS_X] = -1;
        c_map.sign[AXIS_Y] = 1;
        c_map.sign[AXIS_Z] = -1;
        c_map.map[AXIS_X] = AXIS_X;
        c_map.map[AXIS_Y] = AXIS_Y;
        c_map.map[AXIS_Z] = AXIS_Z;
    }
}
/**
 * @brief       初始化QMC6308接口
 * @param       无
 * @retval      0, 成功
 *              其他, 异常
 */
uint8_t qmc6308_init(void)
{
    uint8_t res = 0XFF;
    uint8_t retry = 10;
    
    res = qmc6308_read_register(QMC6308_CHIP_ID_REG);   /* 读取芯片ID */
    if(res != QMC6308_DEVICE_ID)
    {
        printf("初始化失败\r\n");
        return 1;
    }
    qmc6308_write_register(QMC6308_CTL_REG3, 0x40);     /* 增强复位电流 */
    qmc6308_write_register(QMC6308_CTL_REG1, 0xC3);     /* 10HZ的输出速率，连续采样模式 */
    qmc6308_write_register(QMC6308_CTL_REG2, 0x00);     /* 无复位 */
    qmc6308_set_layout(8);                              /* 设定三轴符号 */
    while ((retry --) && res)                           /* 多次尝试, 直到 res == 0xC3, 即等到QMC6308有 ACK反应 */
    {
        res = qmc6308_read_register(QMC6308_CTL_REG1);
        if(res == 0xC3)                                 /* 初始化成功 */
        {
            return 0;
        }
        HAL_Delay(10);
    }
    return 1;
}

/**
 * @brief       QMC6308读取磁力计数据(只读 X,Y,Z轴数据)
 * @param       pmagx    : X轴磁力计原始值指针
 * @param       pmagy    : Y轴磁力计原始值指针
 * @param       pmagz    : Z轴磁力计原始值指针
 * @retval      0, 成功
 *              其他, 异常
 */
uint8_t qmc6308_read_magdata(float *pmagx, float *pmagy, float *pmagz)
{
    int res = 0;
    unsigned char mag_data[6];
    short hw_d[3] = {0};
    short raw_c[3];
    int t = 0;
    unsigned char rdy = 0;
    
    /* Check status register for data availability */
    while(!(rdy & 0x01) && (t < 5))
    {
        qmc6308_read_nbytes(QMC6308_STATUS_REG, &rdy, 1);
        t++;
    }
    res = qmc6308_read_nbytes(QMC6308_DATA_X_L_REG, mag_data, 6);   /* 读取XYZ轴数据 */
    if(res != 0)
    {
        return 1;                                                   /* 读取失败直接返回 */
    }
    /* 组合数据 */
    hw_d[0] = (short)(((mag_data[1]) << 8) | mag_data[0]);
    hw_d[1] = (short)(((mag_data[3]) << 8) | mag_data[2]);
    hw_d[2] = (short)(((mag_data[5]) << 8) | mag_data[4]);
    
    /* Unit:mG  1G = 100uT = 1000mG */
    raw_c[AXIS_X] = (int)(c_map.sign[AXIS_X] * hw_d[c_map.map[AXIS_X]]);
    raw_c[AXIS_Y] = (int)(c_map.sign[AXIS_Y] * hw_d[c_map.map[AXIS_Y]]);
    raw_c[AXIS_Z] = (int)(c_map.sign[AXIS_Z] * hw_d[c_map.map[AXIS_Z]]);
    
    *pmagx = (float)raw_c[AXIS_X] / 10.0f;
    *pmagy = (float)raw_c[AXIS_Y] / 10.0f;
    *pmagz = (float)raw_c[AXIS_Z] / 10.0f;
    return res;
    
}

/**
 * @brief       QMC6308读取磁力计数据(只读 X,Y,Z轴数据),读times次取平均
 * @note        本函数耗时 ≈ 15 * times ms
 *
 * @param       pmagx    : X轴磁力计原始值指针
 * @param       pmagy    : Y轴磁力计原始值指针
 * @param       pmagz    : Z轴磁力计原始值指针
 * @param       times    : 读取多少次取平均
 * @retval      0, 成功
 *              其他, 异常
 */
uint8_t qmc6308_read_magdata_average(float *pmagx, float *pmagy, float *pmagz, uint8_t times)
{
    uint8_t i = 0;
    uint8_t error_cnt = 0;
    int32_t magx = 0;
    int32_t magy = 0;
    int32_t magz = 0;

    while (i < times)                                          /* 连续读取times次 */
    {
        if (qmc6308_read_magdata(pmagx, pmagy, pmagz) == 0)    /* 读取数据是否正常? */
        {
            magx += *pmagx;                                    /* 累加 */
            magy += *pmagy;
            magz += *pmagz;
            
            i++;
            error_cnt = 0;
        }
        else
        {
            error_cnt++;                                       /* 错误计数器 */
            HAL_Delay(10);
            if (error_cnt > 100)                               /* 连续100次出错, 直接返回异常 */
            {
                return 0XFF;
            }
        }
    }
    *pmagx = magx / times;                                     /* 取平均值 */
    *pmagy = magy / times;                                     /* 取平均值 */
    *pmagz = magz / times;                                     /* 取平均值 */
    
    return 0;
}

