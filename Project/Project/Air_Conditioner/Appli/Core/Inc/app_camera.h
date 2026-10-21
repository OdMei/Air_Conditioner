/**
 ****************************************************************************************************
 * @file        app_camera.h
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2025-01-13
 * @brief       app_camera.h文件
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

#ifndef __APP_CAMERA_H
#define __APP_CAMERA_H

#include "stm32n6xx_hal.h"

#include "app_ui_port.h"
#include "app_camera_geometry.h"

int32_t app_camera_init(void (*display_pipe_vsync_cb)(void),
                        void (*display_pipe_frame_cb)(void),
                        void (*nn_pipe_vsync_cb)(void),
                        void (*nn_pipe_frame_cb)(void));
/* The application must pause NN/PP/ISP and gate both pipe callbacks first. */
int32_t app_camera_deinit(void);
/* Destination is the full 800x480 RGB565 allocation base. The camera port
   adds the centered square viewport offset and preserves its 1600-byte pitch. */
int32_t app_camera_display_pipe_start(uint8_t *display_pipe_destination,
                                      uint32_t capture_mode);
/**
 * @brief Attempt the configured sensor test pattern after capture starts.
 * @note Thread context only. The status is exposed by the display diagnostic.
 */
void app_camera_set_sensor_test_pattern(void);
/* Returns HAL_OK only when the aligned target register was updated. */
int32_t app_camera_display_pipe_set_address(uint8_t *display_pipe_destination);
int32_t app_camera_nn_pipe_start(uint8_t *nn_pipe_destination,
                                 uint32_t capture_mode);
/* Stops only the selected DCMIPP output, leaving shared CSI VC0 running.
   Success guarantees that its DMA target is no longer owned by the pipe.
   The caller must gate frame callbacks before stopping/resetting queues. */
int32_t app_camera_nn_pipe_stop(void);
int32_t app_camera_display_pipe_stop(void);
int32_t app_camera_nn_pipe_restart(uint8_t *nn_pipe_destination,
                                   uint32_t capture_mode);
uint8_t app_camera_map_nn_box(float cx, float cy, float width, float height,
                              app_camera_rect_t *rect);
void app_camera_nn_pipe_set_address(uint8_t *nn_pipe_destination);
void app_camera_isp_update(void);
/**
 * @brief Read camera, CSI receiver, and DCMIPP pipeline diagnostics.
 * @param diag Output structure; NULL is ignored.
 * @note Call from thread context. Frame count is updated by the DCMIPP ISR.
 */
void app_camera_get_display_diag(app_camera_display_diag_t *diag);
/* Supervisor reads progress without modifying the display FPS sampler. */
uint32_t app_camera_get_display_frame_count(void);

#endif
