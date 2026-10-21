/**
 ****************************************************************************************************
 * @file        app_config.h
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2025-01-13
 * @brief       app_config.h文件
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

#ifndef __APP_CONFIG_H
#define __APP_CONFIG_H

#include "stm32n6xx_hal.h"
#include "postprocess_conf.h"
#include "app_ui_port.h"
#include "app_model_config.h"

#define DISPLAY_DELAY                           1
#define DISPLAY_BUFFER_NB                       (DISPLAY_DELAY + 2)

#define CAMERA_MIRROR_FLIP                      CMW_MIRRORFLIP_MIRROR

#define CPU_LOAD_HISTORY_DEPTH                  8

#define BQUEUE_MAX_BUFFERS                      2

#define NN_WIDTH                                APP_MODEL_INPUT_WIDTH
#define NN_HEIGHT                               APP_MODEL_INPUT_HEIGHT
#define NN_FORMAT                               DCMIPP_PIXEL_PACKER_FORMAT_RGB888_YUV444_1
#define NN_BPP                                  APP_MODEL_INPUT_CHANNELS
#define NN_BUFFER_OUT_SIZE                      APP_MODEL_OUTPUT_BYTES
#define NN_CLASSES                              APP_MODEL_CLASSES
#define NN_CLASSES_TABLE                        {"person"}

#define POSTPROCESS_TYPE                        POSTPROCESS_OD_YOLO_V8_UF
#define AI_OBJDETECT_YOLOV8_PP_CONF_THRESHOLD    APP_MODEL_CONF_THRESHOLD
#define AI_OBJDETECT_YOLOV8_PP_IOU_THRESHOLD     APP_MODEL_IOU_THRESHOLD
#define AI_OBJDETECT_YOLOV8_PP_MAX_BOXES_LIMIT   APP_MODEL_MAX_DETECTIONS
#define AI_OBJDETECT_YOLOV2_PP_CONF_THRESHOLD   0.6f
#define AI_OBJDETECT_YOLOV2_PP_IOU_THRESHOLD    0.3f
#define AI_OBJDETECT_YOLOV2_PP_MAX_BOXES_LIMIT  10

#define APP_DEFAULT_TARGET_TEMP_C               (26U)
#define APP_INPUT_POLL_TICKS                    (5U)
#define APP_INPUT_CLIMATE_PERIOD_MS             (500U)
#define APP_DHT11_PERIOD_MS                     (2000U)

#define APP_CAMERA_STATUS_PENDING               (-1000)
#define APP_CAMERA_STATUS_SKIPPED               (-1001)
#define APP_CAMERA_STATUS_THREAD_ERROR          (-1002)
#define APP_CAMERA_STATUS_TIMEOUT               (-1003)

/* Keep the live camera background and people detector enabled in the
   integrated user/debug interface. */
#define APP_CAMERA_PIPELINE_ENABLED              (1U)

/* Use the completed DCMIPP Pipe1 frame as the LTDC background. */
#define APP_CAMERA_LCD_TEST_PATTERN               (0U)

/* Disable the IMX335 test pattern so Pipe1 shows the live scene. */
#define APP_CAMERA_SENSOR_TEST_PATTERN_MODE       (-1)

#define APP_MODE_AUTO                           (0U)
#define APP_MODE_COOL                           (1U)
#define APP_MODE_SLEEP                          (2U)

#endif
