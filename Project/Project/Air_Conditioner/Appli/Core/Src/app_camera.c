/**
 ****************************************************************************************************
 * @file        app_camera.c
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2025-01-13
 * @brief       app_camera.c文件
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

#include "app_camera.h"
#include "app_config.h"
#include "app_utils.h"
#include "app.h"
#include "app_frame_rate.h"
#include "stm32n6xx_hal.h"
#include "cmw_camera.h"
#include "tx_api.h"

#define APP_CAMERA_STOP_TIMEOUT_MS (100U)

#if (NN_WIDTH != NN_HEIGHT) || \
    (LCD_CAMERA_VIEW_WIDTH != LCD_CAMERA_VIEW_HEIGHT)
#error "The camera viewport must match the square model input aspect ratio."
#endif
#if ((LCD_CAMERA_VIEW_X + LCD_CAMERA_VIEW_WIDTH) > LCD_CAMERA_WIDTH) || \
    ((LCD_CAMERA_VIEW_Y + LCD_CAMERA_VIEW_HEIGHT) > LCD_CAMERA_HEIGHT) || \
    (((LCD_CAMERA_VIEW_Y * LCD_CAMERA_WIDTH + LCD_CAMERA_VIEW_X) * 2U) % 32U != 0U) || \
    ((LCD_CAMERA_WIDTH * 2U) % 32U != 0U)
#error "The camera viewport must fit the RGB565 buffer with aligned DMA rows."
#endif

static int32_t app_camera_display_pipe_init(uint32_t sensor_width,
                                            uint32_t sensor_height);
static int32_t app_camera_nn_pipe_init(uint32_t sensor_width,
                                       uint32_t sensor_height);
static void app_camera_init_crop_config(CMW_Manual_roi_area_t *roi,
                                        uint32_t sensor_width,
                                        uint32_t sensor_height,
                                        uint32_t output_width,
                                        uint32_t output_height);

static void (*app_camera_display_pipe_vsync_user_cb)(void) = NULL;
static void (*app_camera_display_pipe_frame_user_cb)(void) = NULL;
static void (*app_camera_nn_pipe_vsync_user_cb)(void) = NULL;
static void (*app_camera_nn_pipe_frame_user_cb)(void) = NULL;
static int32_t s_camera_display_pipe_config_status =
    APP_CAMERA_STATUS_PENDING;
static int32_t s_camera_sensor_test_pattern_status =
    APP_CAMERA_STATUS_PENDING;
static volatile int32_t s_camera_isp_run_status =
    APP_CAMERA_STATUS_PENDING;
static volatile int32_t s_camera_isp_native_status =
    APP_CAMERA_STATUS_PENDING;
static volatile int32_t s_camera_sensor_exposure_us =
    APP_CAMERA_STATUS_PENDING;
static volatile int32_t s_camera_sensor_gain_mdb =
    APP_CAMERA_STATUS_PENDING;
static volatile int32_t s_camera_exposure_request_us =
    APP_CAMERA_STATUS_PENDING;
static volatile uint32_t s_camera_exposure_vmax_lines;
static volatile uint32_t s_camera_exposure_shutter_lines;
static volatile int32_t s_camera_exposure_failure_stage =
    APP_CAMERA_STATUS_PENDING;
static volatile int32_t s_camera_exposure_driver_status =
    APP_CAMERA_STATUS_PENDING;
static uint32_t s_camera_display_pipe_pitch;
static volatile uint32_t s_camera_display_pipe_frame_count;
static volatile uint32_t s_camera_display_pipe_last_frame_ms;
static volatile uint8_t s_camera_display_pipe_frame_seen;
static app_frame_rate_t s_camera_display_frame_rate;
static app_camera_roi_t s_camera_display_roi;
static app_camera_roi_t s_camera_nn_roi;

int32_t app_camera_init(void (*display_pipe_vsync_cb)(void),
                        void (*display_pipe_frame_cb)(void),
                        void (*nn_pipe_vsync_cb)(void),
                        void (*nn_pipe_frame_cb)(void))
{
    CMW_CameraInit_t cmw_camera_init = {0};
    int32_t status;

    cmw_camera_init.width = 0;
    cmw_camera_init.height = 0;
    cmw_camera_init.fps = 0;
    cmw_camera_init.pixel_format = 0;
    cmw_camera_init.anti_flicker = 0;
    cmw_camera_init.mirror_flip = CAMERA_MIRROR_FLIP;
    status = CMW_CAMERA_Init(&cmw_camera_init);
    if (status != CMW_ERROR_NONE)
    {
        return status;
    }

    status = app_camera_display_pipe_init(cmw_camera_init.width,
                                          cmw_camera_init.height);
    if (status != CMW_ERROR_NONE)
    {
        return status;
    }

    status = app_camera_nn_pipe_init(cmw_camera_init.width,
                                     cmw_camera_init.height);
    if (status != CMW_ERROR_NONE)
    {
        return status;
    }

    if (display_pipe_vsync_cb != NULL)
    {
        app_camera_display_pipe_vsync_user_cb = display_pipe_vsync_cb;
    }

    if (display_pipe_frame_cb != NULL)
    {
        app_camera_display_pipe_frame_user_cb = display_pipe_frame_cb;
    }

    if (nn_pipe_vsync_cb != NULL)
    {
        app_camera_nn_pipe_vsync_user_cb = nn_pipe_vsync_cb;
    }

    if (nn_pipe_frame_cb != NULL)
    {
        app_camera_nn_pipe_frame_user_cb = nn_pipe_frame_cb;
    }

    return CMW_ERROR_NONE;
}

int32_t app_camera_display_pipe_start(uint8_t *display_pipe_destination,
                                      uint32_t capture_mode)
{
    if (display_pipe_destination == NULL ||
        ((uintptr_t)display_pipe_destination & 31U) != 0U)
    {
        return CMW_ERROR_WRONG_PARAM;
    }
    /* Keep the existing 800-pixel allocation and LTDC stride. Only the square
       viewport is DMA-written; initially black side bars remain untouched. */
    return CMW_CAMERA_Start(DCMIPP_PIPE1,
                            display_pipe_destination +
                            (LCD_CAMERA_VIEW_Y * LCD_CAMERA_WIDTH +
                             LCD_CAMERA_VIEW_X) * 2U,
                            capture_mode);
}

int32_t app_camera_deinit(void)
{
    int32_t status = CMW_CAMERA_DeInit();
    uint32_t irq_state;

    irq_state = __get_PRIMASK();
    __disable_irq();
    s_camera_display_pipe_config_status = APP_CAMERA_STATUS_PENDING;
    s_camera_sensor_test_pattern_status = APP_CAMERA_STATUS_PENDING;
    s_camera_isp_run_status = APP_CAMERA_STATUS_PENDING;
    s_camera_isp_native_status = APP_CAMERA_STATUS_PENDING;
    s_camera_display_pipe_frame_count = 0U;
    s_camera_display_pipe_frame_seen = 0U;
    s_camera_display_pipe_last_frame_ms = 0U;
    memset(&s_camera_display_frame_rate, 0, sizeof(s_camera_display_frame_rate));
    memset(&s_camera_display_roi, 0, sizeof(s_camera_display_roi));
    memset(&s_camera_nn_roi, 0, sizeof(s_camera_nn_roi));
    __set_PRIMASK(irq_state);
    return status;
}

int32_t app_camera_display_pipe_set_address(uint8_t *display_pipe_destination)
{
    DCMIPP_HandleTypeDef *hdcmipp = CMW_CAMERA_GetDCMIPPHandle();
    if (hdcmipp == NULL || hdcmipp->Instance == NULL ||
        display_pipe_destination == NULL ||
        ((uintptr_t)display_pipe_destination & 31U) != 0U) { return HAL_ERROR; }
    display_pipe_destination +=
        (LCD_CAMERA_VIEW_Y * LCD_CAMERA_WIDTH + LCD_CAMERA_VIEW_X) * 2U;
    HAL_StatusTypeDef status = HAL_DCMIPP_PIPE_SetMemoryAddress(hdcmipp,
        DCMIPP_PIPE1, DCMIPP_MEMORY_ADDRESS_0, (uint32_t)display_pipe_destination);
    __DMB();
    if (status == HAL_OK && hdcmipp->Instance->P1PPM0AR1 != (uint32_t)display_pipe_destination) {
        return HAL_ERROR;
    }
    return (int32_t)status;
}

void app_camera_set_sensor_test_pattern(void)
{
    int32_t status;
    uint32_t irq_state;

    status = CMW_CAMERA_SetTestPattern(
        APP_CAMERA_SENSOR_TEST_PATTERN_MODE);

    irq_state = __get_PRIMASK();
    __disable_irq();
    s_camera_sensor_test_pattern_status = status;
    __set_PRIMASK(irq_state);
}

void app_camera_get_display_diag(app_camera_display_diag_t *diag)
{
    DCMIPP_HandleTypeDef *hdcmipp;
    uint32_t irq_state;
    uint32_t now_ms;
    uint8_t frame_seen;

    if (diag == NULL)
    {
        return;
    }

    hdcmipp = CMW_CAMERA_GetDCMIPPHandle();
    irq_state = __get_PRIMASK();
    __disable_irq();
    diag->config_status = s_camera_display_pipe_config_status;
    diag->sensor_test_pattern_mode =
        APP_CAMERA_SENSOR_TEST_PATTERN_MODE;
    diag->sensor_test_pattern_status =
        s_camera_sensor_test_pattern_status;
    diag->isp_run_status = s_camera_isp_run_status;
    diag->isp_native_status = s_camera_isp_native_status;
    diag->sensor_exposure_us = s_camera_sensor_exposure_us;
    diag->sensor_gain_mdb = s_camera_sensor_gain_mdb;
    diag->exposure_request_us = s_camera_exposure_request_us;
    diag->exposure_vmax_lines = s_camera_exposure_vmax_lines;
    diag->exposure_shutter_lines = s_camera_exposure_shutter_lines;
    diag->exposure_failure_stage = s_camera_exposure_failure_stage;
    diag->exposure_driver_status = s_camera_exposure_driver_status;
    diag->configured_pitch = s_camera_display_pipe_pitch;
    diag->hardware_pitch = 0U;
    diag->dma_address = 0U;
    diag->frame_count = s_camera_display_pipe_frame_count;
    diag->last_frame_ms = s_camera_display_pipe_last_frame_ms;
    frame_seen = s_camera_display_pipe_frame_seen;
    now_ms = HAL_GetTick();
    diag->csi_status0 = 0U;
    diag->csi_status1 = 0U;
    diag->csi_error1 = 0U;
    diag->csi_error2 = 0U;
    diag->pipe0_data_count = 0U;
    diag->hal_error = 0U;
    diag->pipe0_state = 0U;
    diag->pipe1_state = 0U;
    diag->pipe2_state = 0U;

    if ((hdcmipp != NULL) && (hdcmipp->Instance != NULL))
    {
        diag->hardware_pitch =
            hdcmipp->Instance->P1PPM0PR &
            DCMIPP_P1PPM0PR_PITCH_Msk;
        diag->dma_address = hdcmipp->Instance->P1PPM0AR1;
        diag->csi_status0 = CSI->SR0;
        diag->csi_status1 = CSI->SR1;
        diag->csi_error1 = CSI->ERR1;
        diag->csi_error2 = CSI->ERR2;
        diag->pipe0_data_count = hdcmipp->Instance->P0DCCNTR;
        diag->hal_error = hdcmipp->ErrorCode;
        diag->pipe0_state = (uint32_t)hdcmipp->PipeState[0];
        diag->pipe1_state = (uint32_t)hdcmipp->PipeState[1];
        diag->pipe2_state = (uint32_t)hdcmipp->PipeState[2];
    }
    __set_PRIMASK(irq_state);

    /* Keep rate arithmetic out of the camera ISR and interrupt snapshot. */
    app_frame_rate_sample(&s_camera_display_frame_rate, diag->frame_count,
                          diag->last_frame_ms, frame_seen, now_ms);
    diag->frame_rate_x10 = s_camera_display_frame_rate.rate_x10;
    diag->frame_rate_valid = s_camera_display_frame_rate.valid;
}

uint32_t app_camera_get_display_frame_count(void)
{
    return s_camera_display_pipe_frame_count;
}

int32_t app_camera_nn_pipe_start(uint8_t *nn_pipe_destination,
                                 uint32_t capture_mode)
{
    return CMW_CAMERA_Start(DCMIPP_PIPE2, nn_pipe_destination, capture_mode);
}

static int32_t app_camera_pipe_stop(uint32_t pipe)
{
    DCMIPP_HandleTypeDef *hdcmipp = CMW_CAMERA_GetDCMIPPHandle();
    uint32_t active_mask;
    uint32_t interrupts;
    uint32_t start_ms;
    uint32_t waits = 0U;
    uint32_t irq_state;

    if ((hdcmipp == NULL) || (hdcmipp->Instance == NULL) ||
        (hdcmipp->PipeState[pipe] == HAL_DCMIPP_PIPE_STATE_RESET))
    {
        return CMW_ERROR_PERIPH_FAILURE;
    }

    if (pipe == DCMIPP_PIPE2)
    {
        CLEAR_BIT(hdcmipp->Instance->P2FCTCR, DCMIPP_P2FCTCR_CPTREQ);
        active_mask = DCMIPP_CMSR1_P2CPTACT;
        interrupts = DCMIPP_IT_PIPE2_FRAME | DCMIPP_IT_PIPE2_VSYNC |
                     DCMIPP_IT_PIPE2_LINE | DCMIPP_IT_PIPE2_OVR;
    }
    else
    {
        CLEAR_BIT(hdcmipp->Instance->P1FCTCR, DCMIPP_P1FCTCR_CPTREQ);
        active_mask = DCMIPP_CMSR1_P1CPTACT;
        interrupts = DCMIPP_IT_PIPE1_FRAME | DCMIPP_IT_PIPE1_VSYNC |
                     DCMIPP_IT_PIPE1_LINE | DCMIPP_IT_PIPE1_OVR;
    }

    /* HAL_DCMIPP_CSI_PIPE_Stop also stops shared CSI VC0. Stop the
       selected output only, and yield while the last DMA frame drains.
       A stalled pipe remains quarantined: do not declare its DMA free. */
    start_ms = HAL_GetTick();
    while ((hdcmipp->Instance->CMSR1 & active_mask) != 0U)
    {
        if (((HAL_GetTick() - start_ms) >= APP_CAMERA_STOP_TIMEOUT_MS) ||
            (waits++ >= APP_CAMERA_STOP_TIMEOUT_MS))
        {
            return APP_CAMERA_STATUS_TIMEOUT;
        }
        tx_thread_sleep(1U);
    }

    irq_state = __get_PRIMASK();
    __disable_irq();
    if (pipe == DCMIPP_PIPE2)
    {
        CLEAR_BIT(hdcmipp->Instance->P2FSCR, DCMIPP_P2FSCR_PIPEN);
        CLEAR_BIT(hdcmipp->Instance->P2PPCR, DCMIPP_P2PPCR_DBM);
    }
    else
    {
        CLEAR_BIT(hdcmipp->Instance->P1FSCR, DCMIPP_P1FSCR_PIPEN);
        CLEAR_BIT(hdcmipp->Instance->P1PPCR, DCMIPP_P1PPCR_DBM);
    }
    __HAL_DCMIPP_DISABLE_IT(hdcmipp, interrupts);
    __HAL_DCMIPP_CLEAR_FLAG(hdcmipp, interrupts);
    hdcmipp->PipeState[pipe] = HAL_DCMIPP_PIPE_STATE_READY;
    __set_PRIMASK(irq_state);
    return CMW_ERROR_NONE;
}

int32_t app_camera_nn_pipe_stop(void)
{
    return app_camera_pipe_stop(DCMIPP_PIPE2);
}

int32_t app_camera_display_pipe_stop(void)
{
    return app_camera_pipe_stop(DCMIPP_PIPE1);
}

int32_t app_camera_nn_pipe_restart(uint8_t *nn_pipe_destination,
                                   uint32_t capture_mode)
{
    int32_t status = app_camera_nn_pipe_stop();

    if (status != CMW_ERROR_NONE)
    {
        return status;
    }
    return app_camera_nn_pipe_start(nn_pipe_destination, capture_mode);
}

uint8_t app_camera_map_nn_box(float cx, float cy, float width, float height,
                              app_camera_rect_t *rect)
{
    const app_camera_rect_t view = {
        LCD_CAMERA_VIEW_X, LCD_CAMERA_VIEW_Y,
        LCD_CAMERA_VIEW_WIDTH, LCD_CAMERA_VIEW_HEIGHT
    };
    return app_camera_geometry_map_view_box(&s_camera_nn_roi,
                                            &s_camera_display_roi, &view,
                                            LCD_CAMERA_WIDTH, LCD_CAMERA_HEIGHT,
                                            cx, cy, width, height, rect);
}

void app_camera_nn_pipe_set_address(uint8_t *nn_pipe_destination)
{
    HAL_DCMIPP_PIPE_SetMemoryAddress(CMW_CAMERA_GetDCMIPPHandle(), DCMIPP_PIPE2, DCMIPP_MEMORY_ADDRESS_0, (uint32_t)nn_pipe_destination);
}

void app_camera_isp_update(void)
{
    int32_t run_status;
    int32_t native_status;
    int32_t exposure_us;
    int32_t gain_mdb;
    CMW_CAMERA_ExposureDiagnostics_t exposure_diag = {0};
    uint32_t irq_state;

    run_status = CMW_CAMERA_Run();
    native_status = APP_CAMERA_STATUS_PENDING;
    if (CMW_CAMERA_GetLastISPBackgroundStatus(&native_status) !=
        CMW_ERROR_NONE)
    {
        native_status = APP_CAMERA_STATUS_PENDING;
    }
    exposure_us = APP_CAMERA_STATUS_PENDING;
    gain_mdb = APP_CAMERA_STATUS_PENDING;

    if (CMW_CAMERA_GetExposure(&exposure_us) != CMW_ERROR_NONE)
    {
        exposure_us = APP_CAMERA_STATUS_PENDING;
    }
    if (CMW_CAMERA_GetGain(&gain_mdb) != CMW_ERROR_NONE)
    {
        gain_mdb = APP_CAMERA_STATUS_PENDING;
    }
    if (CMW_CAMERA_GetExposureDiagnostics(&exposure_diag) !=
        CMW_ERROR_NONE)
    {
        exposure_diag.requested_exposure_us = APP_CAMERA_STATUS_PENDING;
        exposure_diag.failure_stage = APP_CAMERA_STATUS_PENDING;
        exposure_diag.driver_status = APP_CAMERA_STATUS_PENDING;
    }

    irq_state = __get_PRIMASK();
    __disable_irq();
    s_camera_isp_run_status = run_status;
    s_camera_isp_native_status = native_status;
    s_camera_sensor_exposure_us = exposure_us;
    s_camera_sensor_gain_mdb = gain_mdb;
    s_camera_exposure_request_us =
        exposure_diag.requested_exposure_us;
    s_camera_exposure_vmax_lines = exposure_diag.vmax_lines;
    s_camera_exposure_shutter_lines =
        exposure_diag.shutter_lines;
    s_camera_exposure_failure_stage = exposure_diag.failure_stage;
    s_camera_exposure_driver_status = exposure_diag.driver_status;
    __set_PRIMASK(irq_state);
}

static int32_t app_camera_display_pipe_init(uint32_t sensor_width,
                                            uint32_t sensor_height)
{
    CMW_DCMIPP_Conf_t cmw_dcmipp_conf = {0};
    int32_t status;
    uint32_t irq_state;
    uint32_t hw_pitch = 0U;

    cmw_dcmipp_conf.output_width = LCD_CAMERA_VIEW_WIDTH;
    cmw_dcmipp_conf.output_height = LCD_CAMERA_VIEW_HEIGHT;
    cmw_dcmipp_conf.output_format = DCMIPP_PIXEL_PACKER_FORMAT_RGB565_1;
    cmw_dcmipp_conf.output_bpp = 2;
    cmw_dcmipp_conf.enable_swap = 0;
    cmw_dcmipp_conf.enable_gamma_conversion = 0;
    cmw_dcmipp_conf.mode = CMW_Aspect_ratio_manual_roi;
    app_camera_init_crop_config(&cmw_dcmipp_conf.manual_conf,
                                sensor_width, sensor_height,
                                NN_WIDTH, NN_HEIGHT);
    s_camera_display_roi.x = cmw_dcmipp_conf.manual_conf.offset_x;
    s_camera_display_roi.y = cmw_dcmipp_conf.manual_conf.offset_y;
    s_camera_display_roi.width = cmw_dcmipp_conf.manual_conf.width;
    s_camera_display_roi.height = cmw_dcmipp_conf.manual_conf.height;
    status = CMW_CAMERA_SetPipeConfig(DCMIPP_PIPE1, &cmw_dcmipp_conf,
                                      &hw_pitch);
    if (status == CMW_ERROR_NONE)
    {
        DCMIPP_HandleTypeDef *hdcmipp = CMW_CAMERA_GetDCMIPPHandle();
        hw_pitch = LCD_CAMERA_WIDTH * 2U;
        /* CMW derives pitch from output width. HAL explicitly supports a
           wider memory stride: preserve 1600 bytes for the 800-pixel LTDC. */
        if (hdcmipp == NULL || hdcmipp->Instance == NULL ||
            HAL_DCMIPP_PIPE_SetPitch(hdcmipp, DCMIPP_PIPE1, hw_pitch) != HAL_OK)
        {
            status = CMW_ERROR_COMPONENT_FAILURE;
        }
        else if (((hdcmipp->Instance->P1PPM0PR &
                   DCMIPP_P1PPM0PR_PITCH_Msk) >>
                  DCMIPP_P1PPM0PR_PITCH_Pos) != hw_pitch)
        {
            status = CMW_ERROR_COMPONENT_FAILURE;
        }
    }
    irq_state = __get_PRIMASK();
    __disable_irq();
    s_camera_display_pipe_config_status = status;
    s_camera_display_pipe_pitch = hw_pitch;
    __set_PRIMASK(irq_state);
    return status;
}

static int32_t app_camera_nn_pipe_init(uint32_t sensor_width,
                                       uint32_t sensor_height)
{
    CMW_DCMIPP_Conf_t cmw_dcmipp_conf = {0};
    uint32_t hw_pitch = 0U;

    cmw_dcmipp_conf.output_width = NN_WIDTH;
    cmw_dcmipp_conf.output_height = NN_HEIGHT;
    cmw_dcmipp_conf.output_format = NN_FORMAT;
    cmw_dcmipp_conf.output_bpp = NN_BPP;
    cmw_dcmipp_conf.enable_swap = 1;
    cmw_dcmipp_conf.enable_gamma_conversion = 0;
    cmw_dcmipp_conf.mode = CMW_Aspect_ratio_manual_roi;
    app_camera_init_crop_config(&cmw_dcmipp_conf.manual_conf,
                                sensor_width, sensor_height,
                                NN_WIDTH, NN_HEIGHT);
    s_camera_nn_roi.x = cmw_dcmipp_conf.manual_conf.offset_x;
    s_camera_nn_roi.y = cmw_dcmipp_conf.manual_conf.offset_y;
    s_camera_nn_roi.width = cmw_dcmipp_conf.manual_conf.width;
    s_camera_nn_roi.height = cmw_dcmipp_conf.manual_conf.height;
    return CMW_CAMERA_SetPipeConfig(DCMIPP_PIPE2, &cmw_dcmipp_conf,
                                    &hw_pitch);
}

static void app_camera_init_crop_config(CMW_Manual_roi_area_t *roi,
                                        uint32_t sensor_width,
                                        uint32_t sensor_height,
                                        uint32_t output_width,
                                        uint32_t output_height)
{
    float ratiox;
    float ratioy;
    float ratio;

    ratiox = (float)sensor_width / output_width;
    ratioy = (float)sensor_height / output_height;
    ratio = MIN(ratiox, ratioy);

    roi->width = (uint32_t)MIN(output_width * ratio, sensor_width);
    roi->height = (uint32_t)MIN(output_height * ratio, sensor_height);
    roi->offset_x = (sensor_width - roi->width + 1) / 2;
    roi->offset_y = (sensor_height - roi->height + 1) / 2;
}

HAL_StatusTypeDef MX_DCMIPP_ClockConfig(DCMIPP_HandleTypeDef *hdcmipp)
{
    RCC_PeriphCLKInitTypeDef rcc_periph_clk_init_struct = {0};

    rcc_periph_clk_init_struct.PeriphClockSelection = RCC_PERIPHCLK_DCMIPP | RCC_PERIPHCLK_CSI;
    rcc_periph_clk_init_struct.DcmippClockSelection = RCC_DCMIPPCLKSOURCE_IC17;
    rcc_periph_clk_init_struct.ICSelection[RCC_IC17].ClockSelection = RCC_ICCLKSOURCE_PLL2;
    rcc_periph_clk_init_struct.ICSelection[RCC_IC17].ClockDivider = 3;
    rcc_periph_clk_init_struct.ICSelection[RCC_IC18].ClockSelection = RCC_ICCLKSOURCE_PLL1;
    rcc_periph_clk_init_struct.ICSelection[RCC_IC18].ClockDivider = 40;
    if (HAL_RCCEx_PeriphCLKConfig(&rcc_periph_clk_init_struct) != HAL_OK)
    {
        return HAL_ERROR;
    }

    return HAL_OK;
}

int CMW_CAMERA_PIPE_VsyncEventCallback(uint32_t pipe)
{
    if (pipe == DCMIPP_PIPE1)
    {
        if (app_camera_display_pipe_vsync_user_cb != NULL)
        {
            app_camera_display_pipe_vsync_user_cb();
        }
    }
    else if (pipe == DCMIPP_PIPE2)
    {
        if (app_camera_nn_pipe_vsync_user_cb != NULL)
        {
            app_camera_nn_pipe_vsync_user_cb();
        }
    }

    return 0;
}

int CMW_CAMERA_PIPE_FrameEventCallback(uint32_t pipe)
{
    if (pipe == DCMIPP_PIPE1)
    {
        s_camera_display_pipe_frame_count++;
        s_camera_display_pipe_last_frame_ms = HAL_GetTick();
        s_camera_display_pipe_frame_seen = 1U;
        if (app_camera_display_pipe_frame_user_cb != NULL)
        {
            app_camera_display_pipe_frame_user_cb();
        }
    }
    else if (pipe == DCMIPP_PIPE2)
    {
        if (app_camera_nn_pipe_frame_user_cb != NULL)
        {
            app_camera_nn_pipe_frame_user_cb();
        }
    }

    return 0;
}
