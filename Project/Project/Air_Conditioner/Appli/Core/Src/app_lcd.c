/**
 ****************************************************************************************************
 * @file        app_lcd.c
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2025-01-13
 * @brief       app_lcd.c文件
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

#include "app_lcd.h"
#include "app_config.h"
#include "app_bg_buffers.h"
#include "bsp_lcd.h"
#include "tx_api.h"
#include <string.h>

#define APP_LCD_TEST_PATTERN_COLOR_COUNT (8U)
#define APP_LCD_RGB565_BYTES_PER_PIXEL (2U)
#define APP_LCD_DIAG_SAMPLE_STEPS (3U)
#define APP_LCD_DIAG_SAMPLE_DIVISOR (4U)
#define APP_LCD_FOREGROUND_RELOAD_TIMEOUT_MS (50U)

static volatile uint16_t app_lcd_bg_pixel_min = UINT16_MAX;
static volatile uint16_t app_lcd_bg_pixel_max;

#if (APP_CAMERA_LCD_TEST_PATTERN != 0U) && (DISPLAY_BUFFER_NB < 3U)
#error "The LCD test pattern requires a third background buffer."
#endif

static uint8_t app_lcd_bg_buffer[DISPLAY_BUFFER_NB][LCD_CAMERA_WIDTH * LCD_CAMERA_HEIGHT * 2] __attribute__((aligned(32))) __attribute__((section(".EXTRAM")));
static app_bg_buffers_t app_lcd_bg_state;
static uint32_t app_lcd_bg_completed_sequence;
static uint32_t app_lcd_bg_sampled_sequence;
static int32_t app_lcd_bg_last_status;
typedef struct
{
    uint32_t frame_id;
    uint32_t captured_ms;
    uint32_t generation;
    uint8_t valid;
} app_lcd_bg_capture_meta_t;
static app_lcd_bg_capture_meta_t app_lcd_bg_meta[DISPLAY_BUFFER_NB];
static uint32_t app_lcd_bg_generation;
static uint8_t app_lcd_fg_buffer[2][LCD_FG_WIDTH * LCD_FG_HEIGHT * 2] __attribute__((aligned(32))) __attribute__((section(".EXTRAM")));
static uint8_t app_lcd_fg_buffer_load_idx;
typedef struct
{
    uint32_t x0;
    uint32_t y0;
    uint32_t x1;
    uint32_t y1;
    uint8_t valid;
} app_lcd_dirty_t;
static app_lcd_dirty_t app_lcd_fg_dirty;
static app_lcd_dirty_t app_lcd_fg_pending_dirty;
static uint8_t app_lcd_fg_reload_pending;
static const uint16_t app_lcd_test_pattern_colors[
    APP_LCD_TEST_PATTERN_COLOR_COUNT] = {
        0xFFFFU, 0xFFE0U, 0x07FFU, 0x07E0U,
        0xF81FU, 0xF800U, 0x001FU, 0x0000U
    };

uint32_t app_ui_tick_ms(void)
{
    return HAL_GetTick();
}

void app_lcd_init(void)
{
    bsp_lcd_layer_config_t bsp_lcd_layer_config = {0};

    (void)app_bg_buffers_init(&app_lcd_bg_state, DISPLAY_BUFFER_NB, 1U, 0U);
    app_bg_buffers_stop(&app_lcd_bg_state);
    app_lcd_bg_completed_sequence = 0U;
    app_lcd_bg_sampled_sequence = 0U;
    app_lcd_bg_last_status = HAL_OK;
    app_lcd_bg_generation = 1U;
    memset(app_lcd_bg_meta, 0, sizeof(app_lcd_bg_meta));

    app_lcd_fg_buffer_load_idx = 0U;
    app_lcd_fg_reload_pending = 0U;
    memset(&app_lcd_fg_dirty, 0, sizeof(app_lcd_fg_dirty));
    memset(&app_lcd_fg_pending_dirty, 0, sizeof(app_lcd_fg_pending_dirty));

    memset(app_lcd_bg_buffer, 0, sizeof(app_lcd_bg_buffer));
    SCB_CleanInvalidateDCache_by_Addr(app_lcd_bg_buffer, sizeof(app_lcd_bg_buffer));
    memset(app_lcd_fg_buffer, 0, sizeof(app_lcd_fg_buffer));
    SCB_CleanInvalidateDCache_by_Addr(app_lcd_fg_buffer, sizeof(app_lcd_fg_buffer));

    bsp_lcd_init();
    bsp_lcd_set_brightness(100);

    bsp_lcd_layer_config.x0 = 0U;
    bsp_lcd_layer_config.y0 = 0U;
    bsp_lcd_layer_config.x1 = LCD_CAMERA_WIDTH;
    bsp_lcd_layer_config.y1 = LCD_CAMERA_HEIGHT;
    bsp_lcd_layer_config.pixel_format = LCD_PIXEL_FORMAT_RGB565;
    bsp_lcd_layer_config.address = (uint32_t)app_lcd_bg_buffer[app_lcd_bg_state.front];
    bsp_lcd_config_layer(0, &bsp_lcd_layer_config);

    bsp_lcd_layer_config.x0 = (BSP_LCD_WIDTH - LCD_FG_WIDTH) / 2;
    bsp_lcd_layer_config.y0 = (BSP_LCD_HEIGHT - LCD_FG_HEIGHT) / 2;
    bsp_lcd_layer_config.x1 = bsp_lcd_layer_config.x0 + LCD_FG_WIDTH;
    bsp_lcd_layer_config.y1 = bsp_lcd_layer_config.y0 + LCD_FG_HEIGHT;
    bsp_lcd_layer_config.pixel_format = LCD_PIXEL_FORMAT_ARGB4444;
    bsp_lcd_layer_config.address = (uint32_t)app_lcd_fg_buffer[1];
    bsp_lcd_config_layer(1, &bsp_lcd_layer_config);

    UTIL_LCD_SetFuncDriver(&bsp_lcd_driver);
    UTIL_LCD_SetLayer(1);
    UTIL_LCD_Clear(0x00000000U);
    UTIL_LCD_SetFont(&Font20);
    UTIL_LCD_SetTextColor(UTIL_LCD_COLOR_WHITE);
}

/**
 * @brief  Display fixed RGB565 color bars on the camera background layer.
 * @return HAL_OK when the test buffer is selected, otherwise an HAL error.
 * @note   Call after app_lcd_init and before camera buffer rotation starts.
 */
HAL_StatusTypeDef app_lcd_show_camera_test_pattern(void)
{
    uint8_t *buffer;
    LTDC_HandleTypeDef *ltdc;
    uint32_t row;
    uint32_t column;
    uint32_t color_index;
    uint32_t byte_offset;
    uint16_t color;
    HAL_StatusTypeDef status;

    buffer = app_lcd_bg_buffer[DISPLAY_BUFFER_NB - 1U];
    for (row = 0U; row < LCD_CAMERA_HEIGHT; row++)
    {
        for (column = 0U; column < LCD_CAMERA_WIDTH; column++)
        {
            color_index = (column * APP_LCD_TEST_PATTERN_COLOR_COUNT) /
                          LCD_CAMERA_WIDTH;
            color = app_lcd_test_pattern_colors[color_index];
            byte_offset = ((row * LCD_CAMERA_WIDTH) + column) *
                          APP_LCD_RGB565_BYTES_PER_PIXEL;
            buffer[byte_offset] = (uint8_t)(color & 0x00FFU);
            buffer[byte_offset + 1U] = (uint8_t)(color >> 8U);
        }
    }

    SCB_CleanDCache_by_Addr(
        buffer, LCD_CAMERA_WIDTH * LCD_CAMERA_HEIGHT *
                APP_LCD_RGB565_BYTES_PER_PIXEL);
    ltdc = bsp_lcd_get_ltdc_handle();
    if (ltdc == NULL)
    {
        return HAL_ERROR;
    }

    status = HAL_LTDC_SetAddress_NoReload(ltdc, (uint32_t)buffer, 0U);
    if (status != HAL_OK)
    {
        return status;
    }

    status = HAL_LTDC_ReloadLayer(
        ltdc, LTDC_RELOAD_VERTICAL_BLANKING, 0U);
    if (status == HAL_OK)
    {
        uint32_t started = HAL_GetTick();
        uint32_t waits = 0U;
        /* Startup only, with camera DMA stopped. The pattern's buffer must
           actually become scanout before changing the ownership model. */
        while ((LTDC_Layer1->RCR & LTDC_LxRCR_VBR) != 0U) {
            if ((uint32_t)(HAL_GetTick() - started) >= 50U || waits++ >= 50U) {
                return HAL_ERROR;
            }
            tx_thread_sleep(1U);
        }
        (void)app_bg_buffers_init(&app_lcd_bg_state, DISPLAY_BUFFER_NB,
            DISPLAY_BUFFER_NB - 1U, 0U);
        app_bg_buffers_stop(&app_lcd_bg_state);
    }

    return status;
}

uint8_t *app_lcd_get_bg_buffer(void)
{
    uint8_t index = app_lcd_bg_state.phase == APP_BG_PREPARED_CAPTURE_TARGET ?
        app_lcd_bg_state.next_fill : app_lcd_bg_state.fill;
    return app_lcd_bg_buffer[index];
}

static void app_lcd_observe_bg_reload(void)
{
    /* Serialize this with camera completion. A global LTDC IRQ is insufficient:
       only Layer1.RCR.VBR proves this background hand-off completed. */
    (void)app_bg_buffers_observe_reload(&app_lcd_bg_state,
        (LTDC_Layer1->RCR & LTDC_LxRCR_VBR) != 0U);
}

void app_lcd_stop_bg_capture(void)
{
    uint32_t irq_state = __get_PRIMASK();
    __disable_irq();
    app_lcd_observe_bg_reload();
    app_bg_buffers_stop(&app_lcd_bg_state);
    /* A recovering/restarted pipeline cannot report the old scanout as a
       current picture, even if LTDC safely continues showing that buffer. */
    app_lcd_bg_generation++;
    __set_PRIMASK(irq_state);
}

uint8_t app_lcd_restart_bg_capture(void)
{
    uint32_t irq_state = __get_PRIMASK();
    __disable_irq();
    app_lcd_observe_bg_reload();
    uint8_t result = app_bg_buffers_restart(&app_lcd_bg_state);
    __set_PRIMASK(irq_state);
    return result;
}

static void app_lcd_sample_completed_frame(uint8_t buffer_index)
{
    const volatile uint16_t *pixels;
    uint16_t pixel_min = UINT16_MAX;
    uint16_t pixel_max = 0U;
    uint32_t row;
    uint32_t column;
    uint32_t x;
    uint32_t y;
    uint32_t pixel_index;
    uint16_t pixel;

    if (buffer_index >= DISPLAY_BUFFER_NB)
    {
        return;
    }

    pixels = (const volatile uint16_t *)app_lcd_bg_buffer[buffer_index];
    for (row = 1U; row <= APP_LCD_DIAG_SAMPLE_STEPS; row++)
    {
        y = (LCD_CAMERA_HEIGHT * row) / APP_LCD_DIAG_SAMPLE_DIVISOR;
        for (column = 1U; column <= APP_LCD_DIAG_SAMPLE_STEPS; column++)
        {
            x = (LCD_CAMERA_WIDTH * column) /
                APP_LCD_DIAG_SAMPLE_DIVISOR;
            pixel_index = (y * LCD_CAMERA_WIDTH) + x;
            SCB_InvalidateDCache_by_Addr(
                (volatile void *)&pixels[pixel_index],
                (int32_t)sizeof(pixel));
            pixel = pixels[pixel_index];
            if (pixel < pixel_min)
            {
                pixel_min = pixel;
            }
            if (pixel > pixel_max)
            {
                pixel_max = pixel;
            }
        }
    }

    app_lcd_bg_pixel_min = pixel_min;
    app_lcd_bg_pixel_max = pixel_max;
}

uint8_t app_lcd_switch_bg_buffer(void)
{
    app_bg_capture_plan_t plan;
    uint32_t irq_state = __get_PRIMASK();
    __disable_irq();
    app_lcd_observe_bg_reload();
    uint8_t completed_index = app_lcd_bg_state.fill;
    uint8_t was_running = app_lcd_bg_state.running;
    uint8_t result = app_bg_buffers_frame_complete(&app_lcd_bg_state,
        ++app_lcd_bg_completed_sequence, &plan);
    if (was_running != 0U)
    {
        app_lcd_bg_meta[completed_index].frame_id =
            app_lcd_bg_completed_sequence;
        app_lcd_bg_meta[completed_index].captured_ms = HAL_GetTick();
        app_lcd_bg_meta[completed_index].generation = app_lcd_bg_generation;
        app_lcd_bg_meta[completed_index].valid = 1U;
    }
    __set_PRIMASK(irq_state);
    /* No copying, cache maintenance, waiting, or display publication here.
       If pending still owns two slots, capture safely reuses its own fill. */
    return result;
}

void app_lcd_bg_capture_target_result(uint8_t success)
{
    uint32_t irq_state = __get_PRIMASK();
    __disable_irq();
    (void)app_bg_buffers_capture_target_result(&app_lcd_bg_state,
        app_lcd_bg_state.next_fill, success != 0U);
    if (!success) { app_lcd_bg_last_status = HAL_ERROR; }
    __set_PRIMASK(irq_state);
}

void app_lcd_present_bg_buffer(void)
{
    uint8_t candidate;
    uint32_t frame_id;
    uint32_t irq_state = __get_PRIMASK();
    __disable_irq();
    app_lcd_observe_bg_reload();
    if ((LTDC_Layer1->RCR & LTDC_LxRCR_VBR) != 0U ||
        !app_bg_buffers_candidate(&app_lcd_bg_state, &candidate, &frame_id)) {
        __set_PRIMASK(irq_state);
        return;
    }
    LTDC_HandleTypeDef *ltdc = bsp_lcd_get_ltdc_handle();
    HAL_StatusTypeDef status = ltdc ? HAL_LTDC_SetAddress_NoReload(ltdc,
        (uint32_t)app_lcd_bg_buffer[candidate], 0U) : HAL_ERROR;
    if (status == HAL_OK) {
        status = HAL_LTDC_ReloadLayer(ltdc, LTDC_RELOAD_VERTICAL_BLANKING, 0U);
    }
    app_lcd_bg_last_status = status;
    /* Failed/ambiguous requests retain the immutable candidate. Only an
       accepted retry and the layer-specific hardware acknowledgement release it. */
    (void)app_bg_buffers_submission_result(&app_lcd_bg_state, status == HAL_OK);
    __set_PRIMASK(irq_state);
}

void app_lcd_draw_area_update(void)
{
    /* Legacy callers must not modify shadow addresses before a completed
       foreground hand-off. app_lcd_present_fg_buffer owns the reload. */
}

void app_lcd_draw_area_commit(void)
{
    app_lcd_present_fg_buffer(
        app_lcd_fg_buffer[app_lcd_fg_buffer_load_idx]);
}

uint8_t *app_lcd_get_fg_draw_buffer(void)
{
    uint32_t start_ms;
    uint32_t waits = 0U;
    uint32_t row;
    uint32_t offset;
    uint32_t bytes;
    uint8_t previous_draw_index;

    if (app_lcd_fg_reload_pending != 0U)
    {
        start_ms = HAL_GetTick();
        /* RCR.VBR is cleared only by hardware after this layer's reload.
           The shared Reload IRQ also fires for camera layer 0, so a global
           callback cannot prove ownership of foreground layer 1. */
        while ((LTDC_Layer2->RCR & LTDC_LxRCR_VBR) != 0U)
        {
            if (((HAL_GetTick() - start_ms) >=
                 APP_LCD_FOREGROUND_RELOAD_TIMEOUT_MS) ||
                (waits++ >= APP_LCD_FOREGROUND_RELOAD_TIMEOUT_MS))
            {
                /* Keep both buffers quarantined while scanout ownership
                   is ambiguous. The UI can retry its invalidated regions. */
                return NULL;
            }
            tx_thread_sleep(1U);
        }

        previous_draw_index = app_lcd_fg_buffer_load_idx;
        app_lcd_fg_buffer_load_idx = 1U - previous_draw_index;
        bytes = (app_lcd_fg_pending_dirty.x1 -
                 app_lcd_fg_pending_dirty.x0) * 2U;
        for (row = app_lcd_fg_pending_dirty.y0;
             row < app_lcd_fg_pending_dirty.y1; row++)
        {
            offset = (row * LCD_FG_WIDTH +
                      app_lcd_fg_pending_dirty.x0) * 2U;
            /* Partial LVGL rendering needs the new scanout as its base.
               Synchronize only modified pixels, after the old front is
               released by hardware; never copy a screen with IRQs off. */
            memcpy(app_lcd_fg_buffer[app_lcd_fg_buffer_load_idx] + offset,
                   app_lcd_fg_buffer[previous_draw_index] + offset, bytes);
            /* This baseline can be outside the next LVGL dirty region.
               Make it visible to LTDC now, rather than relying on the next
               partial submission to clean those unrelated cache lines. */
            SCB_CleanDCache_by_Addr(
                app_lcd_fg_buffer[app_lcd_fg_buffer_load_idx] + offset,
                (int32_t)bytes);
        }
        app_lcd_fg_reload_pending = 0U;
    }
    return app_lcd_fg_buffer[app_lcd_fg_buffer_load_idx];
}

void app_lcd_mark_fg_dirty(int32_t x, int32_t y,
                          uint32_t width, uint32_t height)
{
    int64_t right = (int64_t)x + width;
    int64_t bottom = (int64_t)y + height;
    uint32_t left;
    uint32_t top;
    uint32_t clipped_right;
    uint32_t clipped_bottom;

    if ((width == 0U) || (height == 0U) ||
        (x >= LCD_FG_WIDTH) || (y >= LCD_FG_HEIGHT) ||
        (right <= 0) || (bottom <= 0))
    {
        return;
    }
    left = x > 0 ? (uint32_t)x : 0U;
    top = y > 0 ? (uint32_t)y : 0U;
    clipped_right = right < LCD_FG_WIDTH ? (uint32_t)right : LCD_FG_WIDTH;
    clipped_bottom = bottom < LCD_FG_HEIGHT ? (uint32_t)bottom : LCD_FG_HEIGHT;
    if (app_lcd_fg_dirty.valid == 0U)
    {
        app_lcd_fg_dirty.x0 = left;
        app_lcd_fg_dirty.y0 = top;
        app_lcd_fg_dirty.x1 = clipped_right;
        app_lcd_fg_dirty.y1 = clipped_bottom;
        app_lcd_fg_dirty.valid = 1U;
    }
    else
    {
        if (left < app_lcd_fg_dirty.x0) app_lcd_fg_dirty.x0 = left;
        if (top < app_lcd_fg_dirty.y0) app_lcd_fg_dirty.y0 = top;
        if (clipped_right > app_lcd_fg_dirty.x1) app_lcd_fg_dirty.x1 = clipped_right;
        if (clipped_bottom > app_lcd_fg_dirty.y1) app_lcd_fg_dirty.y1 = clipped_bottom;
    }
}

uint8_t *app_lcd_get_fg_buffer(uint8_t index)
{
    return app_lcd_fg_buffer[index & 1U];
}

void app_lcd_present_fg_buffer(uint8_t *buffer)
{
    uint32_t irq_state;
    uint32_t row;
    uint32_t offset;
    uint32_t bytes;
    HAL_StatusTypeDef status;

    if ((app_lcd_fg_reload_pending != 0U) ||
        (buffer != app_lcd_fg_buffer[app_lcd_fg_buffer_load_idx]))
    {
        return;
    }

    /* Compatibility with legacy full-buffer writers. LVGL explicitly
       marks its accumulated partial regions before committing. */
    if (app_lcd_fg_dirty.valid == 0U)
    {
        app_lcd_mark_fg_dirty(0, 0, LCD_FG_WIDTH, LCD_FG_HEIGHT);
    }
    bytes = (app_lcd_fg_dirty.x1 - app_lcd_fg_dirty.x0) * 2U;
    for (row = app_lcd_fg_dirty.y0; row < app_lcd_fg_dirty.y1; row++)
    {
        offset = (row * LCD_FG_WIDTH + app_lcd_fg_dirty.x0) * 2U;
        SCB_CleanDCache_by_Addr(buffer + offset, (int32_t)bytes);
    }

    irq_state = __get_PRIMASK();
    __disable_irq();
    status = HAL_LTDC_SetAddress_NoReload(bsp_lcd_get_ltdc_handle(),
                                         (uint32_t)buffer, 1);
    if (status == HAL_OK)
    {
        status = HAL_LTDC_ReloadLayer(bsp_lcd_get_ltdc_handle(),
                                     LTDC_RELOAD_VERTICAL_BLANKING, 1);
    }
    if (status == HAL_OK)
    {
        app_lcd_fg_pending_dirty = app_lcd_fg_dirty;
        app_lcd_fg_dirty.valid = 0U;
        app_lcd_fg_reload_pending = 1U;
    }
    __set_PRIMASK(irq_state);
}

void app_lcd_get_display_diag(app_lcd_display_diag_t *diag)
{
    uint32_t irq_state;
    uint32_t completed_sequence;
    uint8_t completed_index;

    if (diag == NULL)
    {
        return;
    }

    irq_state = __get_PRIMASK();
    __disable_irq();
    app_lcd_observe_bg_reload();
    completed_sequence = app_lcd_bg_state.front_frame_id;
    uint8_t probe = completed_sequence != app_lcd_bg_sampled_sequence &&
        app_bg_buffers_pin_front(&app_lcd_bg_state, &completed_index, &completed_sequence);
    __set_PRIMASK(irq_state);
    if (probe)
    {
        /* Pin the confirmed frame while reading nine pixels with IRQs enabled.
           Camera IRQs can still run, but cannot reclaim this frame for DMA. */
        app_lcd_sample_completed_frame(completed_index);
        irq_state = __get_PRIMASK();
        __disable_irq();
        (void)app_bg_buffers_release_probe(&app_lcd_bg_state, completed_index);
        app_lcd_bg_sampled_sequence = completed_sequence;
        __set_PRIMASK(irq_state);
    }

    irq_state = __get_PRIMASK();
    __disable_irq();
    diag->pitch_bytes =
        (LTDC_Layer1->CFBLR & LTDC_LxCFBLR_CFBP_Msk) >>
        LTDC_LxCFBLR_CFBP_Pos;
    diag->address = LTDC_Layer1->CFBAR;
    diag->control = LTDC_Layer1->CR;
    diag->foreground_pitch_bytes =
        (LTDC_Layer2->CFBLR & LTDC_LxCFBLR_CFBP_Msk) >>
        LTDC_LxCFBLR_CFBP_Pos;
    diag->foreground_address = LTDC_Layer2->CFBAR;
    diag->foreground_control = LTDC_Layer2->CR;
    app_lcd_observe_bg_reload();
    diag->display_index = app_lcd_bg_state.front;
    diag->fill_index = app_lcd_bg_state.fill;
    diag->pending_index = app_lcd_bg_state.pending;
    diag->background_reload_pending = app_lcd_bg_state.pending != APP_BG_BUFFER_NONE;
    diag->background_frame_id = app_lcd_bg_state.front_frame_id;
    diag->background_presented = app_lcd_bg_state.presentations;
    diag->background_dropped = app_lcd_bg_state.dropped;
    diag->background_reload_errors = app_lcd_bg_state.reload_failures;
    diag->background_target_errors = app_lcd_bg_state.capture_target_failures;
    diag->background_last_status = app_lcd_bg_last_status;
    diag->pixel_min = app_lcd_bg_pixel_min;
    diag->pixel_max = app_lcd_bg_pixel_max;
    __set_PRIMASK(irq_state);
}

uint8_t app_lcd_copy_camera_thumbnail(uint16_t *dst, uint16_t width,
                                     uint16_t height,
                                     app_camera_thumbnail_info_t *info)
{
    app_lcd_bg_capture_meta_t meta;
    uint8_t index;
    uint32_t frame_id;
    uint32_t irq_state;
    uint8_t accepted;
    const volatile uint16_t *pixels;

    if (info != NULL)
    {
        memset(info, 0, sizeof(*info));
    }
    if (dst == NULL || info == NULL || width == 0U || width != height ||
        width > APP_CAMERA_THUMBNAIL_MAX_SIDE)
    {
        return 0U;
    }

    irq_state = __get_PRIMASK();
    __disable_irq();
    app_lcd_observe_bg_reload();
    index = app_lcd_bg_state.front;
    meta = app_lcd_bg_meta[index];
    accepted = app_lcd_bg_state.running != 0U && meta.valid != 0U &&
        meta.generation == app_lcd_bg_generation &&
        meta.frame_id == app_lcd_bg_state.front_frame_id &&
        (uint32_t)(HAL_GetTick() - meta.captured_ms) <=
            APP_CAMERA_THUMBNAIL_MAX_AGE_MS &&
        app_bg_buffers_pin_front(&app_lcd_bg_state, &index, &frame_id);
    __set_PRIMASK(irq_state);
    if (accepted == 0U)
    {
        return 0U;
    }

    pixels = (const volatile uint16_t *)app_lcd_bg_buffer[index];
    /* The pinned frame may cease being front during this bounded copy. Its
       buffer cannot be retargeted to DMA until release. Selected rows are
       cache-line-aligned (x=160, pitch=1600), so black bars are not touched.
       Camera IRQs and touch input remain enabled throughout pixel reads. */
    for (uint32_t y = 0U; y < height; y++)
    {
        uint32_t source_y = LCD_CAMERA_VIEW_Y +
            ((y * 2U + 1U) * LCD_CAMERA_VIEW_HEIGHT) / (height * 2U);
        const volatile uint16_t *row = pixels +
            source_y * LCD_CAMERA_WIDTH + LCD_CAMERA_VIEW_X;
        SCB_InvalidateDCache_by_Addr((volatile void *)row,
                                     LCD_CAMERA_VIEW_WIDTH * 2U);
        for (uint32_t x = 0U; x < width; x++)
        {
            uint32_t source_x =
                ((x * 2U + 1U) * LCD_CAMERA_VIEW_WIDTH) / (width * 2U);
            dst[y * width + x] = row[source_x];
        }
    }

    irq_state = __get_PRIMASK();
    __disable_irq();
    accepted = app_lcd_bg_state.running != 0U &&
        meta.generation == app_lcd_bg_generation &&
        (uint32_t)(HAL_GetTick() - meta.captured_ms) <=
            APP_CAMERA_THUMBNAIL_MAX_AGE_MS;
    accepted = app_bg_buffers_release_probe(&app_lcd_bg_state, index) && accepted;
    __set_PRIMASK(irq_state);
    if (accepted == 0U)
    {
        return 0U;
    }
    info->frame_id = frame_id;
    info->captured_ms = meta.captured_ms;
    info->source_x = LCD_CAMERA_VIEW_X;
    info->source_y = LCD_CAMERA_VIEW_Y;
    info->source_width = LCD_CAMERA_VIEW_WIDTH;
    info->source_height = LCD_CAMERA_VIEW_HEIGHT;
    info->simulated = 0U;
    return 1U;
}
