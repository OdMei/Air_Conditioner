/**
 ****************************************************************************************************
 * @file        imu.h
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2025-01-13
 * @brief       姿态解算 代码
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

#ifndef __IMU_H
#define __IMU_H

#define IMU_DELTA_T     0.015f          /* 采样时间 */
#define DEG2RAD         0.017453293f    /* 度转弧度 π/180 */
#define RAD2DEG         57.29578f       /* 弧度转度 180/π */

void imu_get_eulerian_angles(float acc[3], float gyro[3], float *rpy , float dt); /* 获取欧拉角 */

#endif



