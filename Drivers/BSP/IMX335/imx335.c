/**
 ****************************************************************************************************
 * @file        imx335.c
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2025-01-13
 * @brief       IMX335驱动代码
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

#include "imx335.h"
#include "STM32_IMX335/imx335.h"
#include "isp_api.h"
#include "isp_param_conf.h"

extern I2C_HandleTypeDef hi2c2; /* I2C句柄 */

DCMIPP_HandleTypeDef hdcmipp;
static __IO uint32_t imx335_capture_frame_count = 0;
static IMX335_Object_t imx335_object = {0};
static ISP_HandleTypeDef imx335_hisp = {0};
static int32_t imx335_isp_gain;
static int32_t imx335_isp_exposure;

static uint8_t imx335_dcmipp_init(void);
static int32_t imx335_io_init(void);
static int32_t imx335_io_deinit(void);
static int32_t imx335_io_writereg(uint16_t dev_addr, uint16_t reg, uint8_t *data, uint16_t length);
static int32_t imx335_io_readreg(uint16_t dev_addr, uint16_t reg, uint8_t *data, uint16_t length);
static int32_t imx335_io_gettick(void);
static ISP_StatusTypeDef imx335_get_sensor_info_helper(uint32_t instance, ISP_SensorInfoTypeDef *sensor_info);
static ISP_StatusTypeDef imx335_set_sensor_gain_helper(uint32_t instance, int32_t gain);
static ISP_StatusTypeDef imx335_get_sensor_gain_helper(uint32_t instance, int32_t *gain);
static ISP_StatusTypeDef imx335_set_sensor_exposure_helper(uint32_t instance, int32_t exposure);
static ISP_StatusTypeDef imx335_get_sensor_exposure_helper(uint32_t instance, int32_t *exposure);

/**
 * @brief   初始化IMX335
 * @param   无
 * @retval  初始化结果
 * @arg     0: 初始化成功
 * @arg     1: 初始化失败
 */
uint8_t imx335_init(void)
{
    IMX335_IO_t imx335_io_struct = {0};
    uint32_t id;
    ISP_AppliHelpersTypeDef isp_appli_helpers_struct = {0};
    ISP_StatAreaTypeDef isp_stat_area_struct = {0};

    if (imx335_dcmipp_init() != 0)
    {
        return 1;
    }

    imx335_io_struct.Init = imx335_io_init;
    imx335_io_struct.DeInit = imx335_io_deinit;
    imx335_io_struct.Address = IMX335_ADDR;
    imx335_io_struct.WriteReg = imx335_io_writereg;
    imx335_io_struct.ReadReg = imx335_io_readreg;
    imx335_io_struct.GetTick = imx335_io_gettick;
    if (IMX335_RegisterBusIO(&imx335_object, &imx335_io_struct) != IMX335_OK)
    {
        return 1;
    }
    else if (IMX335_ReadID(&imx335_object, &id) != IMX335_OK)
    {
        return 1;
    }
    else
    {
        if (id != (uint32_t)IMX335_ID)
        {
            return 1;
        }
        else
        {
            if (IMX335_Init(&imx335_object, IMX335_R2592_1944, IMX335_RAW_RGGB10) != IMX335_OK)
            {
                return 1;
            }
            else if (IMX335_SetFrequency(&imx335_object, IMX335_INCK_37MHZ) != IMX335_OK)
            {
                return 1;
            }
        }
    }

    isp_appli_helpers_struct.GetSensorInfo = imx335_get_sensor_info_helper;
    isp_appli_helpers_struct.SetSensorGain = imx335_set_sensor_gain_helper;
    isp_appli_helpers_struct.GetSensorGain = imx335_get_sensor_gain_helper;
    isp_appli_helpers_struct.SetSensorExposure = imx335_set_sensor_exposure_helper;
    isp_appli_helpers_struct.GetSensorExposure = imx335_get_sensor_exposure_helper;
    isp_stat_area_struct.X0 = 0;
    isp_stat_area_struct.Y0 = 0;
    isp_stat_area_struct.XSize = 2592;
    isp_stat_area_struct.YSize = 1944;
    ISP_Init(&imx335_hisp, &hdcmipp, 0, &isp_appli_helpers_struct, &isp_stat_area_struct, ISP_IQParamCacheInit[0]);

    if (ISP_Start(&imx335_hisp) != ISP_OK)
    {
        return 1;
    }

    return 0;
}

/**
 * @brief   IMX335采集图像
 * @param   无
 * @retval  采集结果
 * @arg     0: 采集成功
 * @arg     1: 采集失败
 */
uint8_t imx335_get_capture(uint32_t address)
{
    imx335_capture_frame_count = 0;
    if (HAL_DCMIPP_CSI_PIPE_Start(&hdcmipp, DCMIPP_PIPE1, DCMIPP_VIRTUAL_CHANNEL0, address, DCMIPP_MODE_SNAPSHOT) != HAL_OK)
    {
        return 1;
    }

    return 0;
}

/**
 * @brief   启动IMX335采集图像
 * @param   无
 * @retval  启动结果
 * @arg     0: 启动成功
 * @arg     1: 启动失败
 */
uint8_t imx335_start_capture(uint32_t address)
{
    imx335_capture_frame_count = 0;
    if (HAL_DCMIPP_CSI_PIPE_Start(&hdcmipp, DCMIPP_PIPE1, DCMIPP_VIRTUAL_CHANNEL0, address, DCMIPP_MODE_CONTINUOUS) != HAL_OK)
    {
        return 1;
    }

    return 0;
}

/**
 * @brief   停止IMX335采集图像
 * @param   无
 * @retval  无
 */
void imx335_stop_capture(void)
{
    HAL_DCMIPP_CSI_PIPE_Stop(&hdcmipp, DCMIPP_PIPE1, DCMIPP_VIRTUAL_CHANNEL0);
}

/**
 * @brief   获取IMX335采集帧数
 * @param   无
 * @retval  帧数
 */
uint32_t imx335_get_capture_frame_count(void)
{
    return imx335_capture_frame_count;
}

/**
 * @brief   IMX335 ISP后台处理函数
 * @param   无
 * @retval  处理结果
 * @arg     0: 处理成功
 * @arg     1: 处理失败
 */
uint8_t imx335_isp_background_process(void)
{
    if (ISP_BackgroundProcess(&imx335_hisp) != ISP_OK)
    {
        return 1;
    }

    return 0;
}

/**
 * @brief   IMX335 DCMIPP pipe 帧事件回调函数
 * @param   pHdcmipp: DCMIPP句柄指针
 * @param   pipe: DCMIPP pipe号
 * @retval  无
 */
void imx335_dcmipp_pipe_frame_cb(DCMIPP_HandleTypeDef *hdcmipp, uint32_t pipe)
{
    UNUSED(hdcmipp);
    UNUSED(pipe);

    imx335_capture_frame_count++;
}

/**
 * @brief   IMX335 DCMIPP pipe Vsync事件回调函数
 * @param   pHdcmipp: DCMIPP句柄指针
 * @param   pipe: DCMIPP pipe号
 * @retval  无
 */
void imx335_dcmipp_pipe_vsync_cb(DCMIPP_HandleTypeDef *hdcmipp, uint32_t pipe)
{
    UNUSED(hdcmipp);

    switch (pipe)
    {
        case DCMIPP_PIPE0:
        {
            ISP_IncDumpFrameId(&imx335_hisp);
            break;
        }
        case DCMIPP_PIPE1:
        {
            ISP_IncMainFrameId(&imx335_hisp);
            ISP_GatherStatistics(&imx335_hisp);
            break;
        }
        case DCMIPP_PIPE2:
        {
            ISP_IncAncillaryFrameId(&imx335_hisp);
            break;
        }
    }
}

/**
 * @brief   初始化IMX335 DCMIPP
 * @param   无
 * @retval  初始化结果
 * @arg     0: 初始化成功
 * @arg     1: 初始化失败
 */
static uint8_t imx335_dcmipp_init(void)
{
    DCMIPP_CSI_ConfTypeDef dcmipp_csi_conf_struct = {0};
    DCMIPP_CSI_PIPE_ConfTypeDef dcmipp_csi_pipe_conf_struct = {0};
    DCMIPP_PipeConfTypeDef dcmipp_pipe_conf_struct = {0};
    DCMIPP_DownsizeTypeDef dcmipp_down_size_struct = {0};

    IMX335_PWDN(1);
    IMX335_RST(0);
    HAL_Delay(200);

    IMX335_RST(1);
    HAL_Delay(3);

    hdcmipp.Instance = DCMIPP;
    if (HAL_DCMIPP_Init(&hdcmipp) != HAL_OK)
    {
        return 1;
    }

    dcmipp_csi_conf_struct.NumberOfLanes = DCMIPP_CSI_TWO_DATA_LANES;
    dcmipp_csi_conf_struct.DataLaneMapping = DCMIPP_CSI_PHYSICAL_DATA_LANES;
    dcmipp_csi_conf_struct.PHYBitrate = DCMIPP_CSI_PHY_BT_1600;
    if (HAL_DCMIPP_CSI_SetConfig(&hdcmipp, &dcmipp_csi_conf_struct) != HAL_OK)
    {
        return 1;
    }

    if (HAL_DCMIPP_CSI_SetVCConfig(&hdcmipp, DCMIPP_VIRTUAL_CHANNEL0, DCMIPP_CSI_DT_BPP10) != HAL_OK)
    {
        return 1;
    }

    dcmipp_csi_pipe_conf_struct.DataTypeMode = DCMIPP_DTMODE_DTIDA;
    dcmipp_csi_pipe_conf_struct.DataTypeIDA = DCMIPP_DT_RAW10;
    dcmipp_csi_pipe_conf_struct.DataTypeIDB = DCMIPP_DT_RAW10;
    if (HAL_DCMIPP_CSI_PIPE_SetConfig(&hdcmipp, DCMIPP_PIPE1, &dcmipp_csi_pipe_conf_struct) != HAL_OK)
    {
        return 1;
    }

    dcmipp_pipe_conf_struct.FrameRate  = DCMIPP_FRAME_RATE_ALL;
    dcmipp_pipe_conf_struct.PixelPipePitch = 1600;
    dcmipp_pipe_conf_struct.PixelPackerFormat = DCMIPP_PIXEL_PACKER_FORMAT_RGB565_1;
    if (HAL_DCMIPP_PIPE_SetConfig(&hdcmipp, DCMIPP_PIPE1, &dcmipp_pipe_conf_struct) != HAL_OK)
    {
        return 1;
    }

    dcmipp_down_size_struct.VSize = 480;
    dcmipp_down_size_struct.HSize = 800;
    dcmipp_down_size_struct.VRatio = 33161;
    dcmipp_down_size_struct.HRatio = 25656;
    dcmipp_down_size_struct.VDivFactor = 253;
    dcmipp_down_size_struct.HDivFactor = 316;
    if (HAL_DCMIPP_PIPE_SetDownsizeConfig(&hdcmipp, DCMIPP_PIPE1, &dcmipp_down_size_struct) != HAL_OK)
    {
        return 1;
    }

    if (HAL_DCMIPP_PIPE_EnableDownsize(&hdcmipp, DCMIPP_PIPE1) != HAL_OK)
    {
        return 1;
    }

    return 0;
}

/**
 * @brief   IMX335 IO初始化
 * @param   无
 * @retval  执行结果
 */
static int32_t imx335_io_init(void)
{
    return 0;
}

/**
 * @brief   IMX335 IO反初始化
 * @param   无
 * @retval  执行结果
 */
static int32_t imx335_io_deinit(void)
{
    return 0;
}

/**
 * @brief   IMX335 IO写寄存器
 * @param   dev_addr: 设备地址
 * @param   reg: 寄存器地址
 * @param   data: 数据
 * @param   length: 数据长度
 * @retval  执行结果
 */
static int32_t imx335_io_writereg(uint16_t dev_addr, uint16_t reg, uint8_t *data, uint16_t length)
{
    if (HAL_I2C_Mem_Write(&hi2c2, dev_addr, reg, I2C_MEMADD_SIZE_16BIT, data, length, 1000) != HAL_OK)
    {
        return 1;
    }

    return 0;
}

/**
 * @brief   IMX335 IO读寄存器
 * @param   dev_addr: 设备地址
 * @param   reg: 寄存器地址
 * @param   data: 数据
 * @param   length: 数据长度
 * @retval  执行结果
 */
static int32_t imx335_io_readreg(uint16_t dev_addr, uint16_t reg, uint8_t *data, uint16_t length)
{
    if (HAL_I2C_Mem_Read(&hi2c2, dev_addr, reg, I2C_MEMADD_SIZE_16BIT, data, length, 1000) != HAL_OK)
    {
        return 1;
    }

    return 0;
}

/**
 * @brief   IMX335 IO获取Tick
 * @param   无
 * @retval  Tick
 */
static int32_t imx335_io_gettick(void)
{
    return (int32_t)HAL_GetTick();
}

/**
 * @brief   IMX335获取传感器信息
 * @param   instance: 对象
 * @param   sensor_info: 传感器信息
 * @retval  ISP状态
 */
static ISP_StatusTypeDef imx335_get_sensor_info_helper(uint32_t instance, ISP_SensorInfoTypeDef *sensor_info)
{
    UNUSED(instance);

    return (ISP_StatusTypeDef)IMX335_GetSensorInfo(&imx335_object, (IMX335_SensorInfo_t *)sensor_info);
}

/**
 * @brief   IMX335设置传感器增益
 * @param   instance: 对象
 * @param   gain: 传感器增益
 * @retval  ISP状态
 */
static ISP_StatusTypeDef imx335_set_sensor_gain_helper(uint32_t instance, int32_t gain)
{
    UNUSED(instance);

    imx335_isp_gain = gain;
    return (ISP_StatusTypeDef)IMX335_SetGain(&imx335_object, gain);
}

/**
 * @brief   IMX335获取传感器增益
 * @param   instance: 对象
 * @param   gain: 传感器增益
 * @retval  ISP状态
 */
static ISP_StatusTypeDef imx335_get_sensor_gain_helper(uint32_t instance, int32_t *gain)
{
    UNUSED(instance);

    *gain = imx335_isp_gain;
    return ISP_OK;
}

/**
 * @brief   IMX335设置传感器曝光
 * @param   instance: 对象
 * @param   exposure: 传感器曝光
 * @retval  ISP状态
 */
static ISP_StatusTypeDef imx335_set_sensor_exposure_helper(uint32_t instance, int32_t exposure)
{
    UNUSED(instance);

    imx335_isp_exposure = exposure;
    return (ISP_StatusTypeDef)IMX335_SetExposure(&imx335_object, exposure);
}

/**
 * @brief   IMX335获取传感器曝光
 * @param   instance: 对象
 * @param   exposure: 传感器曝光
 * @retval  ISP状态
 */
static ISP_StatusTypeDef imx335_get_sensor_exposure_helper(uint32_t instance, int32_t *exposure)
{
    UNUSED(instance);

    *exposure = imx335_isp_exposure;
    return ISP_OK;
}
