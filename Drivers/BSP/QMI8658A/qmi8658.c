/**
 ****************************************************************************************************
 * @file        qmi8658.c
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

#include "qmi8658.h"
#include "math.h"

extern I2C_HandleTypeDef hi2c4; /* I2C句柄 */

/* 全局变量缓存区 */
uint8_t g_imu_init = 0;
qmi8658_state g_imu;

/**
 * @brief   写一个字节到QMI8568的寄存器
 * @param   reg: 寄存器地址
 * @param   data: 寄存器数据
 * @retval  写入结果
 * @arg     0: 成功
 * @arg     1: 失败
 */
uint8_t qmi8658_write_one_byte(uint8_t reg, uint8_t data)
{

    if (HAL_I2C_Mem_Write(&hi2c4, QMI8658_ADDR, reg, I2C_MEMADD_SIZE_8BIT, &data, 1, 1000) != HAL_OK)
    {
    	return 1;
    }
    return 0;
}

/**
 * @brief   在QMI8658指定寄存器地址读出一个数据
 * @param   reg:        寄存器地址
 * @retval  读到的数据
 */
uint8_t qmi8658_read_one_byte(uint8_t reg)
{
	uint8_t temp = 0;
    if (HAL_I2C_Mem_Read(&hi2c4, QMI8658_ADDR, reg, I2C_MEMADD_SIZE_8BIT, &temp, 1, 1000)!= HAL_OK)
	{
		return 1;
	}
    return temp;
}

/**
 * @brief   从QMI8568读取N字节数据
 * @param   reg:        寄存器地址
 * @param   date:       数据存储buf
 * @param   len:        数据长度
 * @retval  读出结果
 * @retval  0, 操作成功
 *          其他, 操作失败
 */
int qmi8568_read_nbytes(uint8_t reg, uint8_t *date, uint8_t len)
{
	if (HAL_I2C_Mem_Read(&hi2c4, QMI8658_ADDR, reg, I2C_MEMADD_SIZE_8BIT, date, len, 1000)!= HAL_OK)
	{
		return 1;
	}
    return 0;
}

/**
 * @brief   QMI8568写入N字节数据
 * @param   reg:        寄存器地址
 * @param   data:       写入数据
 * @param   len:        数据长度
 * @retval  写入结果
 * @arg     0:          成功
 * @arg     1:          失败
 */
uint8_t qmi8658_write_nbytes(uint8_t reg, uint8_t* data, uint8_t len)
{

	if (HAL_I2C_Mem_Write(&hi2c4, QMI8658_ADDR, reg, I2C_MEMADD_SIZE_8BIT, data, len, 1000) != HAL_OK)
	{
		return 1;
	}
    return 0;
}

/**
 * @brief   检查QMI8658A的ID
 * @param   无
 * @retval  检查结果
 * @arg     0: 正确
 * @arg     1: 错误
 */
uint8_t qmi8658_check_whoami(void)
{
    uint8_t qmi8658_chip_id = 0;
    uint8_t qmi8658_revision_id = 0;
    uint8_t i = 0;
    
    while((qmi8658_chip_id != 0x05) && (i < 5))                             /* 多次尝试检查设备标识符 */
    {
        qmi8658_chip_id = qmi8658_read_one_byte(Register_WhoAmI);
        if(qmi8658_chip_id == 0x05)
        {
            qmi8658_revision_id = qmi8658_read_one_byte(Register_Revision); /* 读取设备ID */
            break;
        }
        i++;
    }
    if((qmi8658_chip_id == 0x05) && (qmi8658_revision_id == 0x7c))          /* 读取到准确的标识符和设备ID */
    {
        printf("qmi8658的设备标识符：%#x, 设备ID：%#x\r\n", qmi8658_chip_id, qmi8658_revision_id);
        return 0;
    }
    else
    {
        return 1;
    }
}

/**
 * @brief   陀螺仪校准
 * @param   无
 * @retval  检查结果
 * @arg     0: 校准成功
 * @arg     1: 校准失败
 */
uint8_t qmi8658_calibration(void)
{
    uint8_t sta = 0;
    qmi8658_write_one_byte(Register_Ctrl7, 0x00);   /* 关闭陀螺仪、加速度计 */
    qmi8658_write_one_byte(Register_Ctrl9, 0xA2);
    HAL_Delay(2000);
    sta = qmi8658_read_one_byte(Register_COD_Status);
    if(sta == 0x00)
    {
        return 0;
    }else return 1;
}

/**
 * @brief   传感器软件复位
 * @param   无
 * @retval  无
 */
void qmi8658_reset(void)
{
    qmi8658_write_one_byte(Register_Reset, 0xB0); /* 复位QMI8658 */
    HAL_Delay(150);
}

/**
 * @brief   读取初始状态寄存器
 * @param   无
 * @retval  读取结果
 */
uint8_t qmi8658_read_statusint(void)
{
    return qmi8658_read_one_byte(Register_StatusInt);
}

/**
 * @brief   读取状态寄存器0
 * @param   无
 * @retval  读取结果
 */
uint8_t qmi8658_read_status0(void)
{
    return qmi8658_read_one_byte(Register_Status0);
}

/**
 * @brief   读取状态寄存器1
 * @param   无
 * @retval  读取结果
 */
uint8_t qmi8658_read_status1(void)
{
    return qmi8658_read_one_byte(Register_Status1);
}

/*****************************************************************************************************************/
/**
 * @brief   配置加速度计参数
 * @param   range       ：量程
 * @param   odr         ：odr输出速率
 * @param   lpfEnable   ：低通滤波器 ：Qmi8658Lpf_Enable 打开，Qmi8658Lpf_Disable 关闭
 * @param   stEnable    ：陀螺仪自检 ：Qmi8658St_Enable 自检，Qmi8658St_Disable 不自检
 * @retval  无
 */
void qmi8658_config_acc(enum qmi8658_accrange range, enum qmi8658_accodr odr, enum qmi8658_LpfConfig lpfEnable, enum qmi8658_StConfig stEnable)
{
    unsigned char ctl_dada;

    switch (range)
    {
        case Qmi8658accrange_2g:
            g_imu.ssvt_a = (1 << 14);
            break;
        case Qmi8658accrange_4g:
            g_imu.ssvt_a = (1 << 13);
            break;
        case Qmi8658accrange_8g:
            g_imu.ssvt_a = (1 << 12);
            break;
        case Qmi8658accrange_16g:
            g_imu.ssvt_a = (1 << 11);
            break;
        default:
            range = Qmi8658accrange_8g;
            g_imu.ssvt_a = (1 << 12);
            break;
    }
    if (stEnable == Qmi8658St_Enable)
    {
        ctl_dada = (unsigned char)range | (unsigned char)odr | 0x80;
    } 
    else
    {
        ctl_dada = (unsigned char)range | (unsigned char)odr;
    }
    qmi8658_write_one_byte(Register_Ctrl2, ctl_dada);
    /* set LPF & HPF */
    qmi8568_read_nbytes(Register_Ctrl5, &ctl_dada, 1);
    ctl_dada &= 0xf0;
    if (lpfEnable == Qmi8658Lpf_Enable)
    {
        ctl_dada |= A_LSP_MODE_3;
        ctl_dada |= 0x01;
    }
    else
    {
        ctl_dada &= ~0x01;
    }
    qmi8658_write_one_byte(Register_Ctrl5, ctl_dada);
}

/**
 * @brief   配置陀螺仪参数
 * @param   range       ：量程
 * @param   odr         ：odr输出速率
 * @param   lpfEnable   ：低通滤波器 ：Qmi8658Lpf_Enable 打开，Qmi8658Lpf_Disable 关闭
 * @param   stEnable    ：陀螺仪自检 ：Qmi8658St_Enable 自检，Qmi8658St_Disable 不自检
 * @retval  无
 */
void qmi8658_config_gyro(enum qmi8658_gyrrange range, enum qmi8658_gyrodr odr, enum qmi8658_LpfConfig lpfEnable, enum qmi8658_StConfig stEnable)
{
    /* Set the CTRL3 register to configure dynamic range and ODR */
    unsigned char ctl_dada;

    /* Store the scale factor for use when processing raw data */
    switch (range)
    {
        case Qmi8658gyrrange_16dps:
            g_imu.ssvt_g = 2048;
            break;
        case Qmi8658gyrrange_32dps:
            g_imu.ssvt_g = 1024;
            break;
        case Qmi8658gyrrange_64dps:
            g_imu.ssvt_g = 512;
            break;
        case Qmi8658gyrrange_128dps:
            g_imu.ssvt_g = 256;
            break;
        case Qmi8658gyrrange_256dps:
            g_imu.ssvt_g = 128;
            break;
        case Qmi8658gyrrange_512dps:
            g_imu.ssvt_g = 64;
            break;
        case Qmi8658gyrrange_1024dps:
            g_imu.ssvt_g = 32;
            break;
        case Qmi8658gyrrange_2048dps:
            g_imu.ssvt_g = 16;
            break;
        default:
            range = Qmi8658gyrrange_512dps;
            g_imu.ssvt_g = 64;
            break;
    }

    if (stEnable == Qmi8658St_Enable)
    {
        ctl_dada = (unsigned char)range | (unsigned char)odr | 0x80;
    }   
    else
    {
        ctl_dada = (unsigned char)range | (unsigned char)odr;
    }
    qmi8658_write_one_byte(Register_Ctrl3, ctl_dada);

    /* Conversion from degrees/s to rad/s if necessary */
    /* set LPF & HPF */
    qmi8568_read_nbytes(Register_Ctrl5, &ctl_dada, 1);
    ctl_dada &= 0x0f;
    if (lpfEnable == Qmi8658Lpf_Enable)
    {
        ctl_dada |= G_LSP_MODE_3;
        ctl_dada |= 0x10;
    }
    else
    {
        ctl_dada &= ~0x10;
    }
    qmi8658_write_one_byte(Register_Ctrl5, ctl_dada);
}

/**
 * @brief   发送命令给Register_Ctrl9寄存器
 * @param   cmd  ：命令：参数Ctrl9Command枚举
 * @retval  命令发送结果
 * @arg     0: 成功
 * @arg     1: 失败
 */
int qmi8658_send_ctl9cmd(enum Ctrl9Command cmd)
{
    unsigned char status1 = 0x00;
    unsigned short count = 0;
    unsigned char status_reg = Register_StatusInt;
    unsigned char cmd_done = 0x80;
    unsigned char retry = 0;
    int ret1 = 0;
    int ret2 = 0;

#if defined(QMI8658_SYNC_SAMPLE_MODE)   /* 同步采样模式 */
    if (g_imu.cfg.syncsample == 1)
    {
        status_reg = Register_Status1;
        cmd_done = 0x01;
    }
#endif
    while(retry++ < 3)
    {
        qmi8658_write_one_byte(Register_Ctrl9, (unsigned char)cmd);     /* write commond to ctrl9*/

        qmi8568_read_nbytes(status_reg, &status1, 1);
        while (((status1 & cmd_done) != cmd_done) && (count++ < 100))   /* read statusINT until bit7 is 1*/
        {
        	HAL_Delay(1);
            qmi8568_read_nbytes(status_reg, &status1, 1);
        }
        if(count < 100)
        {
            ret1 = 1;
        }
        else
        {
            ret1 = 0;
        }

        qmi8658_write_one_byte(Register_Ctrl9, Ctrl9_Cmd_Ack);          /* write commond  0x00 to ctrl */
        count = 0;
        qmi8568_read_nbytes(status_reg, &status1, 1);
        while (((status1 & cmd_done) == cmd_done) && (count++ < 100))   /* read statusINT until bit7 is 0 */
        {
        	HAL_Delay(1);
            qmi8568_read_nbytes(status_reg, &status1, 1);
        }
        if(count < 100)
        {
            ret2 = 1;
        }
        else
        {
            ret2 = 0;
        }

        if((ret1==0) || (ret2==0))
        {
            continue;
        }
        else
        {
            break;
        }
    }

    if(ret1 && ret2)
    {
        return 0;
    }
    else
    {
        printf("qmi8658_send_ctl9cmd fail cmd=%d\n", cmd);
        return 1;
    }
}

/**
 * @brief   获取传感器温度
 * @param   无
 * @retval  温度值,单位为℃
 */
float qmi8658_get_temperature(void)
{
    short temp = 0;
    float temp_f = 0;
    uint8_t buf[2];

    qmi8568_read_nbytes(Register_Tempearture_L, buf, 2);    /* 读取温度数据 */    
    temp = ((short)buf[1] << 8) | buf[0];
    temp_f = (float)temp / 256.0f;
    return temp_f;
}


/**
 * @brief   读取补偿后QMI8658陀螺仪和加速度的数据（无判断） 
 * @param   acc  ：存储加速度计xyz轴数据
 * @param   gyro ：存储陀螺仪xyz轴数据
 * @retval  无
 */
void qmi8658_read_sensor_data(float *acc, float *gyro)
{
    unsigned char buf_reg[12];
    short raw_acc_xyz[3];
    short raw_gyro_xyz[3];
    unsigned char axis = 0;
    static int cali_count = 0;
    static float offset_acc[3] = {0.0, 0.0, 0.0};
    static float offset_gyro[3] = {0.0, 0.0, 0.0};
    static float accel_calibration_sum[3] = {0.0f, 0.0f, 0.0f};
    static float gyro_calibration_sum[3] = {0.0f, 0.0f, 0.0f};

    float acc_raw[3];
    float gyro_raw[3];

    /* 读取加速度计和陀螺仪数据 */
    qmi8568_read_nbytes(Register_Ax_L, buf_reg, 12);
    
    raw_acc_xyz[0] = (short)((unsigned short)(buf_reg[1] << 8) | (buf_reg[0]));
    raw_acc_xyz[1] = (short)((unsigned short)(buf_reg[3] << 8) | (buf_reg[2]));
    raw_acc_xyz[2] = (short)((unsigned short)(buf_reg[5] << 8) | (buf_reg[4]));

    raw_gyro_xyz[0] = (short)((unsigned short)(buf_reg[7] << 8) | (buf_reg[6]));
    raw_gyro_xyz[1] = (short)((unsigned short)(buf_reg[9] << 8) | (buf_reg[8]));
    raw_gyro_xyz[2] = (short)((unsigned short)(buf_reg[11] << 8) | (buf_reg[10]));

    /* 加速度单位：m/s2 */
    acc_raw[0] = (float)(raw_acc_xyz[0] * ONE_G) / g_imu.ssvt_a;
    acc_raw[1] = (float)(raw_acc_xyz[1] * ONE_G) / g_imu.ssvt_a;
    acc_raw[2] = (float)(raw_acc_xyz[2] * ONE_G) / g_imu.ssvt_a;

    /* 陀螺仪单位：rad/s */
    gyro_raw[0] = (float)(raw_gyro_xyz[0] * M_PI) / (g_imu.ssvt_g * 180);   /* *pi/180 */
    gyro_raw[1] = (float)(raw_gyro_xyz[1] * M_PI) / (g_imu.ssvt_g * 180);
    gyro_raw[2] = (float)(raw_gyro_xyz[2] * M_PI) / (g_imu.ssvt_g * 180);

    if (g_imu_init != 1)            /* flash中已经存储校准标记？ */
    {
        if (cali_count == 0)        /* 没有存储，重新计算 */
        {
            memset((void *)accel_calibration_sum, 0, sizeof(accel_calibration_sum));
            memset((void *)gyro_calibration_sum, 0, sizeof(gyro_calibration_sum));
            cali_count++;
        }
        else if (cali_count < MAX_CALI_COUNT)
        {
            for (axis = 0; axis < 3; axis++)
            {
                if (axis == 2)
                {
                    accel_calibration_sum[axis] += (acc_raw[axis] - ONE_G);
                }
                else
                {
                    accel_calibration_sum[axis] += acc_raw[axis];
                }
                gyro_calibration_sum[axis] += gyro_raw[axis];
            }
            cali_count++;
        }
        else if (cali_count == MAX_CALI_COUNT)
        {
            for (axis = 0; axis < 3; axis++)
            {
                offset_gyro[axis] = (0.0f - (gyro_calibration_sum[axis] / (MAX_CALI_COUNT - 1)));
                offset_acc[axis] = (0.0f - (accel_calibration_sum[axis] / (MAX_CALI_COUNT - 1)));
            }
            g_imu_init = 1;

            cali_count++;
        }
    }
    else    /* 有存储，直接计算 */
    {
        for (axis = 0; axis < 3; axis++)
        {
            acc[axis] = acc_raw[axis] + offset_acc[axis];
            gyro[axis] = gyro_raw[axis] + offset_gyro[axis];
        }
    }
}

/**
 * @brief   判断数据更新后，在读取补偿后QMI8658陀螺仪和加速度的数据(推荐使用)
 * @param   acc  : 加速度计 X,Y,Z缓存区;
 * @param   gyro : 陀螺仪 X,Y,Z缓存区;
 * @retval  无
 */
void qmi8658_read_xyz(float *acc, float *gyro)
{
    unsigned char status = 0;
    unsigned char data_ready = 0;
    int retry = 0;

    while (retry++ < 3)
    {
#if defined(QMI8658_SYNC_SAMPLE_MODE)
        qmi8568_read_nbytes(Register_StatusInt, &status, 1);

        if (status & 0x01)
        {
            delay_us(12);       /* delay 12us <=500Hz， 12us 1000Hz, 4us 2000Hz 2us > 2000Hz */
        }
        if ((status & 0x01) || (status & 0x03))
        {
            data_ready = 1;
            break;
        }
#else
        /* 检查加速度计和陀螺仪数据是否可用 */
        qmi8568_read_nbytes(Register_Status0, &status, 1);
        if (status & 0x03)
        {
            data_ready = 1;
            break;
        }
#endif
    }
    if (data_ready)
    {
        qmi8658_read_sensor_data(acc, gyro);

        g_imu.imu[0] = acc[0];
        g_imu.imu[1] = acc[1];
        g_imu.imu[2] = acc[2];
        g_imu.imu[3] = gyro[0];
        g_imu.imu[4] = gyro[1];
        g_imu.imu[5] = gyro[2];
    }
    else
    {
        acc[0] = g_imu.imu[0];
        acc[1] = g_imu.imu[1];
        acc[2] = g_imu.imu[2];
        gyro[0] = g_imu.imu[3];
        gyro[1] = g_imu.imu[4];
        gyro[2] = g_imu.imu[5];
//      printf("数据还未准备好\r\n");    /* 调试使用 */
    }
}

/**
 * @brief   开启传感器内部高速时钟 
 * @param   enable ：
 *          1 ： 开启传感器高速内部时钟
 *          0 ： 关闭传感器高速内部时钟
 * @retval  无
 */
void qmi8658_enable_ahb_clock(int enable)
{
    if (enable)
    {
        qmi8658_write_one_byte(Register_Ctrl7, 0x00);
        qmi8658_write_one_byte(Register_Cal1_L, 0x00);
        qmi8658_send_ctl9cmd(Ctrl9_Cmd_AHBClockGating);
    }
    else
    {
        qmi8658_write_one_byte(Register_Ctrl7, 0x00);
        qmi8658_write_one_byte(Register_Cal1_L, 0x01);
        qmi8658_send_ctl9cmd(Ctrl9_Cmd_AHBClockGating);
    }
}

/**
 * @brief   使能陀螺仪、加速度计 
 * @param   enableFlags ：
 *          QMI8658_DISABLE_ALL  : 都不使能
 *          QMI8658_ACC_ENABLE   : 使能加速度计
 *          QMI8658_GYR_ENABLE   : 使能陀螺仪
 *          QMI8658_ACCGYR_ENABLE: 使能陀螺仪、加速度计
 * @retval  无
 */
void qmi8658_enablesensors(unsigned char enableFlags)
{
#if defined(QMI8658_SYNC_SAMPLE_MODE)
    g_imu.cfg.syncsample = 1;
    qmi8658_enable_ahb_clock(0);
    qmi8658_write_one_byte(Register_Ctrl7, enableFlags | 0x80);
#else
    qmi8658_write_one_byte(Register_Ctrl7, enableFlags);    
#endif
    g_imu.cfg.ensensors = enableFlags & 0x03;

    HAL_Delay(2);
}

/**
 * @brief   配置QMI8658陀螺仪和加速度计的量程、输出频率参数等
 * @param   low_power ： 0： 正常模式 1：低功耗模式
 * @retval  无
 */
void qmi8658_config_reg(unsigned char low_power)
{
    qmi8658_enablesensors(QMI8658_DISABLE_ALL);
    if (low_power)
    {
        g_imu.cfg.ensensors = QMI8658_ACC_ENABLE;           
        g_imu.cfg.accrange = Qmi8658accrange_8g;
        g_imu.cfg.accodr = Qmi8658accodr_LowPower_21Hz;
        g_imu.cfg.gyrrange = Qmi8658gyrrange_1024dps;
        g_imu.cfg.gyrodr = Qmi8658gyrodr_250Hz;
    }
    else
    {
        g_imu.cfg.ensensors = QMI8658_ACCGYR_ENABLE;    /* 使能陀螺仪、加速度计 */
        g_imu.cfg.accrange = Qmi8658accrange_16g;       /* ±16g */
        g_imu.cfg.accodr = Qmi8658accodr_500Hz;         /* 500Hz采样 */
        g_imu.cfg.gyrrange = Qmi8658gyrrange_128dps;    /* ±128dps */
        g_imu.cfg.gyrodr = Qmi8658gyrodr_500Hz;         /* 500Hz采样 */
    }

    if (g_imu.cfg.ensensors & QMI8658_ACC_ENABLE)
    {
        qmi8658_config_acc(g_imu.cfg.accrange, g_imu.cfg.accodr, Qmi8658Lpf_Enable, Qmi8658St_Enable); /* 设置参数并开启加速度计自检和低通滤波器 */
    }
    if (g_imu.cfg.ensensors & QMI8658_GYR_ENABLE)
    {
        qmi8658_config_gyro(g_imu.cfg.gyrrange, g_imu.cfg.gyrodr, Qmi8658Lpf_Enable, Qmi8658St_Enable);/* 设置参数并开启陀螺仪自检和低通滤波器 */
    }
}

/**
 * @brief   初始化QMI8658
 * @param   无
 * @retval  初始化结果
 * @arg     0: 成功
 * @arg     1: 失败
 */
uint8_t qmi8658_init(void)
{
    qmi8658_reset();                                /* 复位传感器 */
    
    if(qmi8658_check_whoami())                      /* 检查设备ID是否正确 */
    {
        return 1;
    }
    if(qmi8658_calibration())
    {
        return 1;
    }
    printf("校准成功！！\r\n");

    qmi8658_write_one_byte(Register_Ctrl1, 0x60);   /* I2C驱动 */
    qmi8658_write_one_byte(Register_Ctrl7, 0x00);   /* 关闭陀螺仪、加速度计 */
    
    qmi8658_config_reg(0);                          /* 配置陀螺仪和加速度计的量程和数据输出速率等参数 */
    qmi8658_enablesensors(g_imu.cfg.ensensors);     /* 使能陀螺仪、加速度计 */

    return 0;
}
