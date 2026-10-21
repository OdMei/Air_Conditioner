#ifndef APP_UI_PORT_H
#define APP_UI_PORT_H

#include <stdint.h>

/* Shared display geometry and diagnostic values; no MCU headers required. */
#define LCD_CAMERA_WIDTH  800
#define LCD_CAMERA_HEIGHT 480
#define LCD_CAMERA_VIEW_X 160
#define LCD_CAMERA_VIEW_Y 0
#define LCD_CAMERA_VIEW_WIDTH 480
#define LCD_CAMERA_VIEW_HEIGHT 480
#define LCD_FG_WIDTH      800
#define LCD_FG_HEIGHT     480

#define APP_CAMERA_THUMBNAIL_MAX_SIDE 128U
#define APP_CAMERA_THUMBNAIL_MAX_AGE_MS 500U

typedef struct
{
    uint32_t frame_id;
    uint32_t captured_ms;
    uint16_t source_x;
    uint16_t source_y;
    uint16_t source_width;
    uint16_t source_height;
    uint8_t simulated;
} app_camera_thumbnail_info_t;

typedef struct
{
    int32_t config_status;
    int32_t sensor_test_pattern_mode;
    int32_t sensor_test_pattern_status;
    int32_t isp_run_status;
    int32_t isp_native_status;
    int32_t sensor_exposure_us;
    int32_t sensor_gain_mdb;
    int32_t exposure_request_us;
    uint32_t exposure_vmax_lines;
    uint32_t exposure_shutter_lines;
    int32_t exposure_failure_stage;
    int32_t exposure_driver_status;
    uint32_t configured_pitch;
    uint32_t hardware_pitch;
    uint32_t dma_address;
    uint32_t frame_count;
    uint32_t csi_status0;
    uint32_t csi_status1;
    uint32_t csi_error1;
    uint32_t csi_error2;
    uint32_t pipe0_data_count;
    uint32_t hal_error;
    uint32_t pipe0_state;
    uint32_t pipe1_state;
    uint32_t pipe2_state;
    /* Measured DCMIPP PIPE1 completion rate, in tenths of a frame/second. */
    uint32_t frame_rate_x10;
    uint32_t last_frame_ms;
    uint8_t frame_rate_valid;
} app_camera_display_diag_t;

typedef struct
{
    uint32_t pitch_bytes;
    uint32_t address;
    uint32_t control;
    uint32_t foreground_pitch_bytes;
    uint32_t foreground_address;
    uint32_t foreground_control;
    uint8_t display_index;
    uint8_t fill_index;
    uint8_t pending_index;
    uint8_t background_reload_pending;
    uint32_t background_frame_id;
    uint32_t background_presented;
    uint32_t background_dropped;
    uint32_t background_reload_errors;
    uint32_t background_target_errors;
    int32_t background_last_status;
    uint16_t pixel_min;
    uint16_t pixel_max;
} app_lcd_display_diag_t;

/* The LCD and simulator provide these functions. Buffers contain ARGB4444. */
uint32_t app_ui_tick_ms(void);
uint8_t *app_lcd_get_fg_draw_buffer(void);
void app_lcd_mark_fg_dirty(int32_t x, int32_t y,
                           uint32_t width, uint32_t height);
void app_lcd_present_fg_buffer(uint8_t *buffer);

/* DP-thread-only, bounded RGB565 copy of confirmed camera scanout. Width and
   height must be equal, in 1..128. Returns 0 and clears info on unavailable,
   stale, stopped/recovering capture. A successful picture is the nearby LCD
   frame, not an assertion of exact synchronization with the NPU source frame.
   The implementation pins the DMA buffer; never call from an ISR or reuse its
   diagnostic/snapshot pin from a second display consumer. */
uint8_t app_lcd_copy_camera_thumbnail(uint16_t *dst, uint16_t width,
                                     uint16_t height,
                                     app_camera_thumbnail_info_t *info);

#endif /* APP_UI_PORT_H */
