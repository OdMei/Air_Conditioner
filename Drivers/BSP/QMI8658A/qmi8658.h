/**
 ****************************************************************************************************
 * @file        qmi8658.h
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2025-01-13
 * @brief       QMI8658A六轴传感器驱动代码
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

#ifndef __QMI8658_H
#define __QMI8658_H

#include "main.h"
#include <stdio.h>
#include <string.h>

/****************************************************************************************************************/
/* QMI8658A相关宏定义 */

#define QMI8658_ADDR            (0X6A << 1)                        /* SA0=1 地址为：0X6A, SA0=0 地址为：0X6B */
#define ONE_G                   9.807f                      /* 加速度单位转换使用 */
#define MAX_CALI_COUNT          100                         /* 采样次数 */

#define QMI8658_DISABLE_ALL     (0x0)                       /* 陀螺仪和加速度计均不使能 */
#define QMI8658_ACC_ENABLE      (0x1)                       /* 使能加速度计 */
#define QMI8658_GYR_ENABLE      (0x2)                       /* 使能陀螺仪 */
#define QMI8658_ACCGYR_ENABLE   (QMI8658_ACC_ENABLE | QMI8658_GYR_ENABLE)/* 陀螺仪和加速度计均使能 */
//#define QMI8658_SYNC_SAMPLE_MODE                          /* 同步采样模式 */
extern uint8_t g_imu_init;      

/****************************************************************************************************************/
/* QMI8658A的寄存器地址 */
/* 详细说明参照QMI8658A数据手册 */
enum Qmi8658AReg
{
    Register_WhoAmI = 0,
    Register_Revision,
    Register_Ctrl1,
    Register_Ctrl2,
    Register_Ctrl3,
    Register_Reserved,
    Register_Ctrl5,
    Register_Reserved1,
    Register_Ctrl7,
    Register_Ctrl8,
    Register_Ctrl9,
    Register_Cal1_L = 11,
    Register_Cal1_H,
    Register_Cal2_L,
    Register_Cal2_H,
    Register_Cal3_L,
    Register_Cal3_H,
    Register_Cal4_L,
    Register_Cal4_H,
    Register_FifoWmkTh = 19, 
    Register_FifoCtrl = 20,
    Register_FifoCount = 21,
    Register_FifoStatus = 22,
    Register_FifoData = 23,
    Register_StatusInt = 45,
    Register_Status0,
    Register_Status1,
    Register_Timestamp_L = 48,
    Register_Timestamp_M,
    Register_Timestamp_H,
    Register_Tempearture_L = 51,
    Register_Tempearture_H,
    Register_Ax_L = 53,
    Register_Ax_H,
    Register_Ay_L,
    Register_Ay_H,
    Register_Az_L,
    Register_Az_H,
    Register_Gx_L = 59,
    Register_Gx_H,
    Register_Gy_L,
    Register_Gy_H,
    Register_Gz_L,
    Register_Gz_H,
    Register_COD_Status = 70,
    Register_dQW_L = 73,
    Register_dQW_H,
    Register_dQX_L,
    Register_dQX_H,
    Register_dQY_L,
    Register_dQY_H,
    Register_dQZ_L,
    Register_dQZ_H,
    Register_dVX_L,
    Register_dVX_H,
    Register_dVY_L,
    Register_dVY_H,
    Register_dVZ_L,
    Register_dVZ_H,
    Register_TAP_Status = 89,
    Register_Step_Cnt_L = 90,
    Register_Step_Cnt_M = 91,
    Register_Step_Cnt_H = 92,
    Register_Reset = 96
};

/****************************************************************************************************************/
/* 详细说明参照QMI8658A数据手册 Ctrl9详细命令说明 */
enum Ctrl9Command
{
    Ctrl9_Cmd_Ack                   = 0X00,
    Ctrl9_Cmd_RstFifo               = 0X04,
    Ctrl9_Cmd_ReqFifo               = 0X05, /* Get FIFO data from Device */
    Ctrl9_Cmd_WoM_Setting           = 0x08, /* 设置并启用运动唤醒 */
    Ctrl9_Cmd_AccelHostDeltaOffset  = 0x09, /* 更改加速度计偏移 */
    Ctrl9_Cmd_GyroHostDeltaOffset   = 0x0A, /* 更改陀螺仪偏移 */
    Ctrl9_Cmd_CfgTap                = 0x0C, /* 配置TAP检测 */
    Ctrl9_Cmd_CfgPedometer          = 0x0D, /* 配置计步器 */
    Ctrl9_Cmd_Motion                = 0x0E, /* 配置任何运动/无运动/显着运动检测 */
    Ctrl9_Cmd_RstPedometer          = 0x0F, /* 重置计步器计数（步数） */
    Ctrl9_Cmd_CopyUsid              = 0x10, /* 将 USID 和 FW 版本复制到 UI 寄存器 */
    Ctrl9_Cmd_SetRpu                = 0x11, /* 配置 IO 上拉 */
    Ctrl9_Cmd_AHBClockGating        = 0x12, /* 内部 AHB 时钟门控开关 */
    Ctrl9_Cmd_OnDemandCalivration   = 0xA2, /* 陀螺仪按需校准 */
    Ctrl9_Cmd_ApplyGyroGains        = 0xAA  /* 恢复保存的陀螺仪增益 */
};

/* 低通滤波器的开启和关闭 */
enum qmi8658_LpfConfig
{
    Qmi8658Lpf_Disable,
    Qmi8658Lpf_Enable
};

/* 自检模式的开启和关闭 */
enum qmi8658_StConfig
{
    Qmi8658St_Disable,
    Qmi8658St_Enable
};

/* 加速度计和陀螺仪的低通过滤器模式选择 */
enum qmi8658_LpfMode
{
    A_LSP_MODE_0 = 0x00 << 1,
    A_LSP_MODE_1 = 0x01 << 1,
    A_LSP_MODE_2 = 0x02 << 1,
    A_LSP_MODE_3 = 0x03 << 1,

    G_LSP_MODE_0 = 0x00 << 5,
    G_LSP_MODE_1 = 0x01 << 5,
    G_LSP_MODE_2 = 0x02 << 5,
    G_LSP_MODE_3 = 0x03 << 5
};

/* 加速度计量程选择 */
enum qmi8658_accrange
{
    Qmi8658accrange_2g = 0x00 << 4,
    Qmi8658accrange_4g = 0x01 << 4,
    Qmi8658accrange_8g = 0x02 << 4,
    Qmi8658accrange_16g = 0x03 << 4
};

/* 加速度计ODR输出速率选择 */
enum qmi8658_accodr
{
    Qmi8658accodr_8000Hz = 0x00,
    Qmi8658accodr_4000Hz = 0x01,
    Qmi8658accodr_2000Hz = 0x02,
    Qmi8658accodr_1000Hz = 0x03,
    Qmi8658accodr_500Hz = 0x04,
    Qmi8658accodr_250Hz = 0x05,
    Qmi8658accodr_125Hz = 0x06,
    Qmi8658accodr_62_5Hz = 0x07,
    Qmi8658accodr_31_25Hz = 0x08,
    Qmi8658accodr_LowPower_128Hz = 0x0c,
    Qmi8658accodr_LowPower_21Hz = 0x0d,
    Qmi8658accodr_LowPower_11Hz = 0x0e,
    Qmi8658accodr_LowPower_3Hz = 0x0f
};

/* 陀螺仪量程选择 */
enum qmi8658_gyrrange
{
    Qmi8658gyrrange_16dps = 0 << 4,
    Qmi8658gyrrange_32dps = 1 << 4,
    Qmi8658gyrrange_64dps = 2 << 4,
    Qmi8658gyrrange_128dps = 3 << 4,
    Qmi8658gyrrange_256dps = 4 << 4,
    Qmi8658gyrrange_512dps = 5 << 4,
    Qmi8658gyrrange_1024dps = 6 << 4,
    Qmi8658gyrrange_2048dps = 7 << 4
};

/* 陀螺仪输出速率选择 */
enum qmi8658_gyrodr
{
    Qmi8658gyrodr_8000Hz = 0x00,
    Qmi8658gyrodr_4000Hz = 0x01,
    Qmi8658gyrodr_2000Hz = 0x02,
    Qmi8658gyrodr_1000Hz = 0x03,
    Qmi8658gyrodr_500Hz = 0x04,
    Qmi8658gyrodr_250Hz = 0x05,
    Qmi8658gyrodr_125Hz = 0x06,
    Qmi8658gyrodr_62_5Hz = 0x07,
    Qmi8658gyrodr_31_25Hz = 0x08
};

typedef struct
{
    unsigned char ensensors;        /* 传感器使能标志 */
    enum qmi8658_accrange accrange; /* 设置加速度计的量程 */
    enum qmi8658_accodr accodr;     /* 设置加速度计的ODR输出速率 */     
    enum qmi8658_gyrrange gyrrange; /* 设置陀螺仪的量程 */
    enum qmi8658_gyrodr gyrodr;     /* 设置陀螺仪的ODR输出速率 */     
    unsigned char syncsample;       /* 同步采样标志 */
} qmi8658_config;

typedef struct
{
    qmi8658_config cfg;             /* 记录传感器参数 */
    unsigned short ssvt_a;          /* 加速度计单位换算参数 */
    unsigned short ssvt_g;          /* 陀螺仪单位换算参数 */
    float imu[6];                   /* 记录上一次的加速度计和陀螺仪的三轴参数 */
} qmi8658_state;

/****************************************************************************************************************/
/* 函数声明 */
uint8_t qmi8658_init(void);                                         /* 初始化QMI8658 */
float qmi8658_get_temperature(void);                                /* 获取传感器温度 */
uint8_t get_euler_angles(float *pitch, float *roll, float *yaw);    /* 获取欧拉角数据 */
void qmi8658_reset(void);                                           /* 复位传感器 */
void qmi8658_read_xyz(float *acc, float *gyro);                     /* 获取加速度计和陀螺仪的三轴数据 */

#endif
