/**
 ****************************************************************************************************
 * @file        app_lcd.h
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2025-01-13
 * @brief       app_lcd.h文件
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

#ifndef __APP_LCD_H
#define __APP_LCD_H

#include "stm32n6xx_hal.h"
#include "stm32_lcd.h"
#include "stm32_lcd_ex.h"

#include "app_ui_port.h"

void app_lcd_init(void);
HAL_StatusTypeDef app_lcd_show_camera_test_pattern(void);
/* Full 800x480 allocation base: app_camera applies the capture viewport offset. */
uint8_t *app_lcd_get_bg_buffer(void);
/* Camera ISR: prepare, retarget DMA, report its status, then present.
   No buffer becomes reusable before this layer's VBlank acknowledgement. */
uint8_t app_lcd_switch_bg_buffer(void);
void app_lcd_bg_capture_target_result(uint8_t success);
void app_lcd_present_bg_buffer(void);
/* Stop is only legal after DMA has actually stopped, never on a timeout. */
void app_lcd_stop_bg_capture(void);
uint8_t app_lcd_restart_bg_capture(void);
void app_lcd_draw_area_update(void);
void app_lcd_draw_area_commit(void);
uint8_t *app_lcd_get_fg_buffer(uint8_t index);
void app_lcd_mark_fg_dirty(int32_t x, int32_t y,
                          uint32_t width, uint32_t height);
/**
 * @brief Read the live LTDC background address, pitch, and ring indices.
 * @param diag Output structure; NULL is ignored.
 * @note Call from thread context while display updates may continue.
 */
void app_lcd_get_display_diag(app_lcd_display_diag_t *diag);

#endif
