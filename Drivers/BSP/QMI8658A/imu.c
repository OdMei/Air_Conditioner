/**
 ****************************************************************************************************
 * @file        imu.c
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2025-01-13
 * @brief       姿态解算 代码
 *              核心代码参考自:https://github.com/Krasjet/quaternion
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

#include <math.h>
#include "imu.h"
#include <stdbool.h>
#include <string.h>


#define ACCZ_SAMPLE     80                      /* 350 */

float Kp    = 20;                               /* 比例增益 */
float Ki    = 0.01f;                            /* 积分增益 */
float exInt = 0.0f;
float eyInt = 0.0f;
float ezInt = 0.0f;                             /* 积分误差累计 */

static float q0 = 1.0f;                         /* 四元数 */
static float q1 = 0.0f;
static float q2 = 0.0f;
static float q3 = 0.0f;	
static float rMat[3][3];                        /* 旋转矩阵 */

static float maxError = 0.f;                    /* 最大误差 */
bool isGravityCalibrated = false;               /* 是否校校准完成 */
static float baseAcc[3] = {0.f, 0.f, 1.0f};     /* 静态加速度 */

static float imu_inv_sqrt(float x);             /* 快速开平方求倒 */

/**
 * @brief   计算静态加速度
 * @param   acc：加速度计原始数据
 * @retval  无
 */
static void imu_calbaseacc(float* acc)          
{
    static unsigned short int cnt = 0;
    static float accZMin = 1.5;
    static float accZMax = 0.5;
    static float sumAcc[3] = {0.f};

    if(cnt == 0)
    {
        accZMin = acc[2];
        accZMax = acc[2];

        for(unsigned char i=0; i<3; i++)
        sumAcc[i] = 0.0f;
    }

    for(unsigned char i = 0; i < 3; i++)
    sumAcc[i] += acc[i];

    if(acc[2] < accZMin)    accZMin = acc[2];
    if(acc[2] > accZMax)    accZMax = acc[2];

    if(++cnt >= ACCZ_SAMPLE) /* 缓冲区满 */
    {
        cnt = 0;
        maxError = accZMax - accZMin;
        if(maxError < 100.0f)
        {
            for(unsigned char i=0; i<3; i++)
            {
                baseAcc[i] = sumAcc[i] / ACCZ_SAMPLE;
            }
            isGravityCalibrated = true;
            
        }
        for(unsigned char i=0; i<3; i++)
        {
            sumAcc[i] = 0.f;
        }
    }
}

/**
 * @brief   计算旋转矩阵
 * @param   无
 * @retval  无
 */
void imu_computerotationmatrix(void)
{
    float q1q1 = q1 * q1;
    float q2q2 = q2 * q2;
    float q3q3 = q3 * q3;

    float q0q1 = q0 * q1;
    float q0q2 = q0 * q2;
    float q0q3 = q0 * q3;
    float q1q2 = q1 * q2;
    float q1q3 = q1 * q3;
    float q2q3 = q2 * q3;

    rMat[0][0] = 1.0f - 2.0f * q2q2 - 2.0f * q3q3;
    rMat[0][1] = 2.0f * (q1q2 + -q0q3);
    rMat[0][2] = 2.0f * (q1q3 - -q0q2);

    rMat[1][0] = 2.0f * (q1q2 - -q0q3);
    rMat[1][1] = 1.0f - 2.0f * q1q1 - 2.0f * q3q3;
    rMat[1][2] = 2.0f * (q2q3 + -q0q1);

    rMat[2][0] = 2.0f * (q1q3 + -q0q2);
    rMat[2][1] = 2.0f * (q2q3 - -q0q1);
    rMat[2][2] = 1.0f - 2.0f * q1q1 - 2.0f * q2q2;
}

/**
 * @brief   获取欧拉角数据
 * @note    姿态解算融合, 核心算法，互补滤波算法，没有使用Kalman滤波算法
 *          尽量保证该函数的调用频率为: IMU_DELTA_T , 否则YAW会相应的偏大/偏小
 * @param   gyro  : 3轴陀螺仪数据
 * @param   acc   : 3轴加速度数据
 * @param   rpy   : 欧拉角存放buf
 * @param   dt    : 调用频率
 * @retval  无
 */
void imu_get_eulerian_angles(float acc[3], float gyro[3], float *rpy , float dt) 
{
    float normalise;
    float ex, ey, ez;
    float halfT = 0.5f * dt;
    float accBuf[3] = {0.f};
    float tempacc[3];
    tempacc[0] = acc[0];
    tempacc[1] = acc[1];
    tempacc[2] = acc[2];
    static unsigned short int uiPostureInitWaitTimer = 200; /* 等待一小段时间，等角度初始化稳定下来 */
    
    if(uiPostureInitWaitTimer > 0)
    {
        uiPostureInitWaitTimer--;
        Kp = 20;                                            /* 初始化开机角度的时候把KP值变大，便于快速收敛到实际值 */
    }
    else
    {
        Kp = 1.0;
    }
    /* 度转弧度 */
    gyro[0] = gyro[0];   
    gyro[1] = gyro[1];  
    gyro[2] = gyro[2]; 

    /* 加速度计输出有效时,利用加速度计补偿陀螺仪*/
    if((acc[0] != 0.0f) || (acc[1] != 0.0f) || (acc[2] != 0.0f))
    {
        /* 单位化加速计测量值 */
        normalise = imu_inv_sqrt(acc[0] * acc[0] + acc[1] * acc[1] + acc[2] * acc[2]);
        acc[0] *= normalise;
        acc[1] *= normalise;
        acc[2] *= normalise;

        /* 加速计读取的方向与重力加速计方向的差值，用向量叉乘计算 */
        ex = (acc[1] * rMat[2][2] - acc[2] * rMat[2][1]);
        ey = (acc[2] * rMat[2][0] - acc[0] * rMat[2][2]);
        ez = (acc[0] * rMat[2][1] - acc[1] * rMat[2][0]);

        /* 误差累计，与积分常数相乘 */
        exInt += Ki * ex * dt ;  
        eyInt += Ki * ey * dt ;
        ezInt += Ki * ez * dt ;

        /* 用叉积误差来做PI修正陀螺零偏，即抵消陀螺读数中的偏移量 */
        gyro[0] += Kp * ex + exInt;
        gyro[1] += Kp * ey + eyInt;
        gyro[2] += Kp * ez + ezInt;
    }
    /* 一阶近似算法，四元数运动学方程的离散化形式和积分 */
    float q0Last = q0;
    float q1Last = q1;
    float q2Last = q2;
    float q3Last = q3;
    q0 += (-q1Last * gyro[0] - q2Last * gyro[1] - q3Last * gyro[2]) * halfT;
    q1 += ( q0Last * gyro[0] + q2Last * gyro[2] - q3Last * gyro[1]) * halfT;
    q2 += ( q0Last * gyro[1] - q1Last * gyro[2] + q3Last * gyro[0]) * halfT;
    q3 += ( q0Last * gyro[2] + q1Last * gyro[1] - q2Last * gyro[0]) * halfT;

    /* 单位化四元数 */
    normalise = imu_inv_sqrt(q0 * q0 + q1 * q1 + q2 * q2 + q3 * q3);
    q0 *= normalise;
    q1 *= normalise;
    q2 *= normalise;
    q3 *= normalise;

    imu_computerotationmatrix();     /* 计算旋转矩阵 */

    /*计算roll pitch yaw 欧拉角*/
    /* pitch角度计算不采用原生欧拉角计算方法-asinf(rMat[2][0]) * RAD2DEG。而采用atan2f(rMat[2][0], rMat[2][2]) * RAD2DEG
    原因是应用不同，我们只要是要知道遥控器的倾斜角度 */
    rpy[0] = asinf(rMat[2][0]) * RAD2DEG;       /* -asinf(rMat[2][0]) * RAD2DEG; */ 
    rpy[1] = atan2f(rMat[2][1], rMat[2][2]) * RAD2DEG;
    rpy[2] = atan2f(rMat[1][0], rMat[0][0]) * RAD2DEG;

    if(0)//(!isGravityCalibrated)   /* 未校准 */
    {
        accBuf[2] = tempacc[0]* rMat[2][0] + tempacc[1] * rMat[2][1] + tempacc[2] * rMat[2][2];	/*accz*/
        imu_calbaseacc(accBuf);         /* 计算静态加速度 */   
    }
}

/**
 * @brief   开方函数
 * @param   x : 待开方的值
 * @retval  开方结果
 */
float imu_inv_sqrt(float x)
{
    float halfx;
    float y;
    long i;

    halfx = 0.5f * x;
    y = x;
    memcpy(&i, &y, sizeof(i));
    i = 0x5f3759df - (i >> 1);
    memcpy(&y, &i, sizeof(y));
    y = y * (1.5f - (halfx * y * y));

    return y;
}

