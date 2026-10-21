#include "app_ui.h"

#include <stddef.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "app_ui_port.h"
#include "app_ui_pixels.h"
#include "app_ui_insights.h"
#include "app_ui_controls.h"
#include "app_ui_theme.h"
#include "app_ui_environment.h"
#include "app_ui_journal.h"
#include "app_ui_lifecycle.h"
#include "app_ui_watch_home.h"
#include "app_detection_smoother.h"
#include "app_model_config.h"
#include "lvgl.h"
#ifdef APP_SIMULATOR
#include "pc_port.h"
#endif

LV_FONT_DECLARE(app_font_ui_16)

#define APP_UI_FLUSH_ROWS (20U)
#define APP_UI_UPDATE_MS  (200U)
#define APP_UI_DEBUG_DETAIL_UPDATE_MS (1000U)
#define APP_UI_LCD_PIXEL_BYTES (2U)
#define APP_UI_LVGL_PIXEL_BYTES (4U)
#define APP_UI_DEBUG_PANEL_X (392)
#define APP_UI_DEBUG_PANEL_Y (88)
#define APP_UI_DEBUG_PANEL_WIDTH (400)
#define APP_UI_DEBUG_PANEL_HEIGHT (316)


static lv_display_t *s_display;
static lv_obj_t *s_user_page;
static lv_obj_t *s_debug_page;
static app_environment_alert_t s_environment_alerts;
static app_event_journal_t s_event_records;
static uint8_t s_lifecycle_ready;
static uint8_t s_presence_observed;
static uint8_t s_previous_occupied;
static lv_obj_t *s_debug_summary;
static lv_obj_t *s_debug_panel;
static lv_obj_t *s_debug_details_button;
static lv_obj_t *s_debug_values;
static lv_obj_t *s_detection_boxes[APP_UI_MAX_DETECTIONS];
static lv_obj_t *s_detection_labels[APP_UI_MAX_DETECTIONS];
static app_detection_smoother_t s_detection_smoother;
static uint8_t s_debug_details_open;
static uint8_t s_debug_details_force;
static uint8_t s_debug_details_paused;
static uint32_t s_debug_details_last_update;
static app_ui_action_cb_t s_action_cb;
static app_ui_demo_cb_t s_demo_cb;
static app_ui_snapshot_t s_snapshot;
static uint32_t s_last_update;
static uint32_t s_process_count;
static uint32_t s_overlay_frame_id;
static uint8_t s_overlay_valid;
static uint8_t s_overlay_initialized;
static char s_debug_values_text[3072];
static char s_perf_text[640];
static char s_sensor_text[200];
static uint8_t *s_active_draw_buffer;
static lv_display_t *s_pending_flush_display;
static lv_area_t s_pending_flush_area;
static uint8_t *s_pending_flush_pixels;

static void app_ui_update_user(void);
static void app_ui_update_debug(void);
static void app_ui_set_text(lv_obj_t *label, const char *text);
static const char *app_ui_reason_name(app_climate_reason_t reason);
static void app_ui_navigate(uint8_t page);
static void app_ui_close_modals(void);

static void app_ui_close_modals(void)
{
    app_ui_watch_home_cancel_input();
    app_ui_controls_close();
    app_ui_environment_close();
    app_ui_journal_close();
    app_ui_watch_home_close_details();
}

static uint8_t app_ui_source(void)
{
    return s_snapshot.simulated ? (s_snapshot.demo_active ? APP_ENV_SOURCE_DEMO :
        APP_ENV_SOURCE_SIMULATED) : APP_ENV_SOURCE_BOARD;
}

static void app_ui_record_event(app_event_type_t type, uint8_t channel)
{
    app_event_meta_t meta = {0};
    meta.type = (uint8_t)type;
    meta.source = app_ui_source();
    meta.environment_channel = channel;
    meta.run_ms = app_ui_tick_ms();
    meta.people_valid = s_snapshot.climate.people_fresh && s_snapshot.people_valid &&
        s_snapshot.people_count >= 0 && s_snapshot.people_count <= INT16_MAX;
    meta.people_count = meta.people_valid ? (int16_t)s_snapshot.people_count : 0;
    meta.temperature_quality = s_snapshot.climate.sensor_fresh && isfinite(s_snapshot.temperature) ?
        APP_EVENT_QUALITY_VALID : s_snapshot.dht.has_sample ?
        APP_EVENT_QUALITY_STALE : APP_EVENT_QUALITY_UNAVAILABLE;
    meta.humidity_quality = s_snapshot.climate.humidity_fresh && isfinite(s_snapshot.humidity) ?
        APP_EVENT_QUALITY_VALID : s_snapshot.dht.has_sample ?
        APP_EVENT_QUALITY_STALE : APP_EVENT_QUALITY_UNAVAILABLE;
    if (meta.temperature_quality == APP_EVENT_QUALITY_VALID &&
        s_snapshot.temperature >= -3276.0f && s_snapshot.temperature <= 3276.0f) {
        meta.temperature_x10 = (int16_t)lroundf(s_snapshot.temperature * 10.0f);
    } else if (meta.temperature_quality == APP_EVENT_QUALITY_VALID) {
        meta.temperature_quality = APP_EVENT_QUALITY_UNAVAILABLE;
    }
    if (meta.humidity_quality == APP_EVENT_QUALITY_VALID &&
        s_snapshot.humidity >= 0.0f && s_snapshot.humidity <= 100.0f) {
        meta.humidity_x10 = (uint16_t)lroundf(s_snapshot.humidity * 10.0f);
    } else if (meta.humidity_quality == APP_EVENT_QUALITY_VALID) {
        meta.humidity_quality = APP_EVENT_QUALITY_UNAVAILABLE;
    }
    uint32_t id = app_event_journal_append(&s_event_records, &meta);
    if (!id) { return; }
    if (!s_snapshot.simulated && (s_snapshot.camera_init_status != 0 ||
        s_snapshot.camera_start_status != 0 || s_snapshot.ai.recovery_active)) {
        (void)app_event_journal_mark_no_image(&s_event_records, id, APP_EVENT_IMAGE_CAMERA_UNAVAILABLE);
        return;
    }
    uint16_t *pixels = app_event_journal_thumb_pixels(&s_event_records, id);
    app_camera_thumbnail_info_t image = {0};
    if (pixels && app_lcd_copy_camera_thumbnail(pixels, APP_EVENT_THUMB_WIDTH,
        APP_EVENT_THUMB_HEIGHT, &image)) {
        /* Never attach a PC test scene to a board-like fixture, and never
           identify the display frame as the exact inference source frame. */
        if ((image.simulated != 0U) != (s_snapshot.simulated != 0U)) {
            (void)app_event_journal_mark_no_image(&s_event_records, id, APP_EVENT_IMAGE_CAMERA_UNAVAILABLE);
        } else {
            (void)app_event_journal_commit_thumb(&s_event_records, id,
                image.frame_id, image.captured_ms);
        }
    } else {
        (void)app_event_journal_mark_no_image(&s_event_records, id, APP_EVENT_IMAGE_COPY_FAILED);
    }
}

static void app_ui_environment_events(app_environment_events_t events)
{
    for (uint8_t channel = 0U; channel < APP_ENV_CHANNEL_COUNT; channel++) {
        uint8_t changes = APP_ENV_EVENTS_FOR_CHANNEL(events, channel);
        if (changes & APP_ENV_EVENT_RAISED) {
            app_ui_record_event(channel == APP_ENV_CHANNEL_TEMPERATURE ?
                APP_EVENT_ENV_TEMP_RAISED : APP_EVENT_ENV_HUM_RAISED, channel);
        }
        if (changes & APP_ENV_EVENT_RECOVERED) {
            app_ui_record_event(APP_EVENT_ENV_RECOVERY, channel);
        }
    }
}

static void app_ui_manual_capture(void *context)
{
    (void)context;
    app_ui_record_event(APP_EVENT_MANUAL_SNAPSHOT, APP_EVENT_CHANNEL_NONE);
}

static void app_ui_observe_lifecycle(void)
{
    if (!s_lifecycle_ready) { return; }
    uint8_t source = app_ui_source();
    if (s_event_records.source != source) {
        app_event_journal_reset(&s_event_records, source);
        s_presence_observed = 0U;
    }
    app_environment_input_t input = {0};
    input.source = source;
    input.temperature_valid = s_snapshot.climate.sensor_fresh &&
        isfinite(s_snapshot.temperature) && s_snapshot.temperature >= -3276.0f &&
        s_snapshot.temperature <= 3276.0f;
    input.humidity_valid = s_snapshot.climate.humidity_fresh &&
        isfinite(s_snapshot.humidity) && s_snapshot.humidity >= 0.0f && s_snapshot.humidity <= 100.0f;
    if (input.temperature_valid) { input.temperature_x10 = (int16_t)lroundf(s_snapshot.temperature * 10.0f); }
    if (input.humidity_valid) { input.humidity_x10 = (uint16_t)lroundf(s_snapshot.humidity * 10.0f); }
    app_ui_environment_events(app_environment_update(&s_environment_alerts, &input, app_ui_tick_ms()));
    /* Confirmed state changes describe observations, not identified people
       entering/leaving an entire room. Faults never generate an ABSENT event. */
    if (!s_snapshot.climate.people_fresh || !s_snapshot.people_valid ||
        s_snapshot.climate.people_confirming || s_snapshot.climate.presence == APP_PRESENCE_UNKNOWN) {
        /* A brief occlusion or unavailable sample does not constitute a new
           arrival. Retain the last confirmed state until a real transition. */
        return;
    }
    uint8_t occupied = s_snapshot.climate.presence != APP_PRESENCE_ABSENT;
    if (!s_presence_observed || occupied != s_previous_occupied) {
        app_ui_record_event(occupied ? APP_EVENT_FIRST_PRESENT : APP_EVENT_ABSENT, APP_EVENT_CHANNEL_NONE);
        s_presence_observed = 1U;
        s_previous_occupied = occupied;
    }
}

const app_environment_alert_t *app_ui_environment_alerts(void) { return &s_environment_alerts; }
const app_event_journal_t *app_ui_event_records(void) { return &s_event_records; }

void app_ui_reset_lifecycle(void)
{
    if (!s_lifecycle_ready) { return; }
    app_ui_close_modals();
    app_environment_init(&s_environment_alerts, NULL, app_ui_source());
    app_event_journal_reset(&s_event_records, app_ui_source());
    s_presence_observed = 0U;
}

static void app_ui_watch_command(uint8_t command)
{
    app_ui_close_modals();
    switch (command) {
    case APP_UI_WATCH_OPEN_ENVIRONMENT:
        app_ui_environment_open(s_user_page);
        break;
    case APP_UI_WATCH_OPEN_JOURNAL:
        app_ui_journal_open(s_user_page, &s_event_records);
        break;
    case APP_UI_WATCH_OPEN_MORE:
        app_ui_controls_show(APP_UI_CONTROLS_MORE, &s_snapshot);
        break;
    case APP_UI_WATCH_OPEN_DETAILS:
        app_ui_watch_home_show_details();
        break;
    default:
        break;
    }
}

static void app_ui_demo_event(uint8_t command)
{
    if (s_demo_cb != NULL && s_snapshot.simulated) {
        s_demo_cb(command);
        /* A paused model clock must not delay its visible control feedback. */
        app_ui_insights_update(&s_snapshot, app_ui_reason_name(s_snapshot.climate.reason));
    }
}

static void app_ui_format_perf(void)
{
    static const char *const names[APP_PERF_COUNT] = {
        "InWait", "OutWait", "NN", "PP", "ISP", "UI", "DP", "Frame", "TouchGap"
    };
    int written = snprintf(s_perf_text, sizeof(s_perf_text),
        "PERF last64 | avg/p95/max us\nClock %s Pixels %s\n"
        "Wall incl. preemption\nFrame/TouchGap: tick 1ms\n",
        s_snapshot.simulated ? "PC --" :
            (s_snapshot.perf.cycle_clock_valid ? "DWT" : "TICK"),
        app_ui_pixels_uses_helium() ? "MVE" : "Scalar");
    size_t used = written > 0 ? (size_t)written : 0U;
    for (unsigned int i = 0U; i < APP_PERF_COUNT && used < sizeof(s_perf_text); i++) {
        const app_perf_stats_t *stage = &s_snapshot.perf.stages[i];
        if (stage->valid && !s_snapshot.simulated) {
            written = snprintf(s_perf_text + used, sizeof(s_perf_text) - used,
                "%s %lu/%lu/%lu us\n", names[i],
                (unsigned long)(stage->mean_x10_us / 10U),
                (unsigned long)stage->p95_us, (unsigned long)stage->max_us);
        } else {
            written = snprintf(s_perf_text + used, sizeof(s_perf_text) - used,
                "%s --\n", names[i]);
        }
        if (written <= 0 || (size_t)written >= sizeof(s_perf_text) - used) { break; }
        used += (size_t)written;
    }
}

static void app_ui_update_detections(void)
{
    const app_vision_result_t *vision =
        app_detection_smoother_update(&s_detection_smoother, &s_snapshot.vision,
                                      s_snapshot.climate.people_fresh);
    uint8_t valid = s_snapshot.climate.people_fresh && vision->valid &&
                    vision->count > 0U && vision->count <= APP_UI_MAX_DETECTIONS;
    for (unsigned int i = 0U; i < APP_UI_MAX_DETECTIONS; i++) {
        const app_ui_detection_t *box = &vision->boxes[i];
        if (!valid || i >= vision->count || !box->width || !box->height ||
            box->x >= LCD_FG_WIDTH || box->y >= LCD_FG_HEIGHT ||
            !isfinite(box->confidence) || box->confidence < 0.0f || box->confidence > 1.0f) {
            lv_obj_add_flag(s_detection_boxes[i], LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        uint32_t width = box->width;
        uint32_t height = box->height;
        if (width > (uint32_t)LCD_FG_WIDTH - box->x) { width = (uint32_t)LCD_FG_WIDTH - box->x; }
        if (height > (uint32_t)LCD_FG_HEIGHT - box->y) { height = (uint32_t)LCD_FG_HEIGHT - box->y; }
        lv_obj_set_pos(s_detection_boxes[i], box->x, box->y);
        lv_obj_set_size(s_detection_boxes[i], (int32_t)width, (int32_t)height);
        char confidence[16];
        (void)snprintf(confidence, sizeof(confidence), "%.0f%%", (double)box->confidence * 100.0);
        app_ui_set_text(s_detection_labels[i], confidence);
        lv_obj_remove_flag(s_detection_boxes[i], LV_OBJ_FLAG_HIDDEN);
    }
}

static void app_ui_set_text(lv_obj_t *label, const char *text)
{
    app_ui_theme_text(label, text);
}


static const char *app_ui_presence_name(app_presence_t presence)
{
    switch (presence)
    {
    case APP_PRESENCE_SLEEP: return "暗光有人";
    case APP_PRESENCE_PRESENT: return "有人";
    case APP_PRESENCE_ABSENT: return "无人";
    default: return "未知";
    }
}

static const char *app_ui_ai_stage_name(uint32_t stage)
{
    switch (stage)
    {
    case 0U: return "PENDING";
    case 1U: return "WAITCAM";
    case 2U: return "PIPE";
    case 3U: return "INPUT";
    case 4U: return "OUTPUT";
    case 5U: return "BIND";
    case 6U: return "NPU";
    case 7U: return "READY";
    case 8U: return "ERROR";
    case 9U: return "OFF";
    default: return "?";
    }
}

static const char *app_ui_reason_name(app_climate_reason_t reason)
{
    switch (reason)
    {
    case APP_REASON_POWER_OFF: return "电源已关闭";
    case APP_REASON_AI_UNKNOWN: return "人员检测未知，等待更新";
    case APP_REASON_CONFIRMING: return "人员变化确认中";
    case APP_REASON_SENSOR_INVALID: return "温度数据失效，暂停制冷请求";
    case APP_REASON_TEMP_SATISFIED: return "已达到目标温度";
    case APP_REASON_COOLING_WAIT: return "制冷保护等待中";
    case APP_REASON_COOLING: return "已生成制冷请求";
    case APP_REASON_ABSENCE_OFF: return "无人超时，自动关机";
    default: return "等待状态更新";
    }
}

static void app_ui_clear_overlay(void)
{
    uint8_t *buffer;

    buffer = s_active_draw_buffer ? s_active_draw_buffer : app_lcd_get_fg_draw_buffer();
    if (buffer == NULL) { return; }
    memset(buffer, 0,
           LCD_FG_WIDTH * LCD_FG_HEIGHT * APP_UI_LCD_PIXEL_BYTES);
    app_lcd_mark_fg_dirty(0, 0, LCD_FG_WIDTH, LCD_FG_HEIGHT);
}

static uint32_t app_ui_tick(void)
{
#ifdef APP_SIMULATOR
    return pc_port_render_tick_ms();
#else
    return app_ui_tick_ms();
#endif
}

static void app_ui_flush(lv_display_t *display, const lv_area_t *area,
                         uint8_t *px_map)
{
    uint32_t width;
    uint32_t height;
    uint16_t *target;

    if (s_active_draw_buffer == NULL) {
        /* LVGL owns px_map until flush_ready. Retain the block and complete
           it once the display port has a writable buffer; never drop it. */
        s_pending_flush_display = display;
        s_pending_flush_area = *area;
        s_pending_flush_pixels = px_map;
        return;
    }

    width = (uint32_t)(area->x2 - area->x1 + 1);
    height = (uint32_t)(area->y2 - area->y1 + 1);
    target = (uint16_t *)s_active_draw_buffer +
             ((uint32_t)area->y1 * LCD_FG_WIDTH + (uint32_t)area->x1);
    /* LVGL reshapes partial buffers to the rendered area width. The converter
       preserves alpha and uses Helium on M55, with a portable PC fallback. */
    app_ui_pixels_convert(target, LCD_FG_WIDTH, px_map, width, height);

    app_lcd_mark_fg_dirty(area->x1, area->y1, width, height);

    if (lv_display_flush_is_last(display))
    {
        app_lcd_present_fg_buffer(s_active_draw_buffer);
    }
    lv_display_flush_ready(display);
}

static void app_ui_touch_read(lv_indev_t *indev, lv_indev_data_t *data)
{
    static lv_point_t last_point;

    (void)indev;
    if (s_snapshot.touch.pressed != 0U)
    {
        last_point.x = s_snapshot.touch.x;
        last_point.y = s_snapshot.touch.y;
        data->state = LV_INDEV_STATE_PRESSED;
    }
    else
    {
        data->state = LV_INDEV_STATE_RELEASED;
    }
    data->point = last_point;
}

static void app_ui_watch_action(app_climate_action_t action)
{
    if (s_action_cb == NULL) { return; }
    if (action == APP_ACTION_FAN_NEXT) {
        app_ui_close_modals();
        app_ui_controls_show(APP_UI_CONTROLS_FAN, &s_snapshot);
        return;
    }
    if (action == APP_ACTION_SWING_LR) {
        app_ui_close_modals();
        app_ui_controls_show(APP_UI_CONTROLS_SWING, &s_snapshot);
        return;
    }
    s_action_cb(action);
}

static lv_obj_t *app_ui_label(lv_obj_t *parent, const char *text,
                              int32_t x, int32_t y, int32_t width)
{
    return app_ui_theme_label(parent, text, x, y, width);
}

static lv_obj_t *app_ui_card(lv_obj_t *parent, int32_t x, int32_t y,
                             int32_t width, int32_t height)
{
    return app_ui_theme_card(parent, x, y, width, height);
}

static lv_obj_t *app_ui_visual_button(lv_obj_t *parent, const char *text,
                                      int32_t x, int32_t y,
                                      int32_t width, int32_t height)
{
    return app_ui_theme_button(parent, text, x, y, width, height);
}

static void app_ui_mode_event(lv_event_t *event)
{
    if (lv_event_get_code(event) == LV_EVENT_CLICKED)
    {
        app_ui_close_modals();
        app_ui_clear_overlay();
        lv_screen_load(s_user_page);
        app_ui_update_user();
        app_ui_watch_home_enter();
        lv_obj_invalidate(s_user_page);
    }
}

static void app_ui_navigate(uint8_t page)
{
    app_ui_close_modals();
    app_ui_clear_overlay();
    if (page == APP_UI_PAGE_HOME) {
        lv_screen_load(s_user_page);
        app_ui_update_user();
        app_ui_watch_home_enter();
    } else if (page == APP_UI_PAGE_VISION) {
        s_debug_details_open = 0U;
        app_ui_theme_checked(s_debug_details_button, false);
        s_debug_details_force = 0U;
        s_debug_details_paused = 0U;
        lv_obj_add_flag(s_debug_panel, LV_OBJ_FLAG_HIDDEN);
        lv_screen_load(s_debug_page);
        s_overlay_initialized = 0U;
        app_ui_update_debug();
    } else {
        app_ui_insights_show(page);
        app_ui_insights_update(&s_snapshot, app_ui_reason_name(s_snapshot.climate.reason));
    }
    lv_obj_invalidate(lv_screen_active());
}

static void app_ui_debug_details_event(lv_event_t *event)
{
    if (lv_event_get_code(event) == LV_EVENT_CLICKED)
    {
        s_debug_details_open = !s_debug_details_open;
        app_ui_theme_checked(s_debug_details_button, s_debug_details_open != 0U);
        if (s_debug_details_open != 0U)
        {
            lv_obj_remove_flag(s_debug_panel, LV_OBJ_FLAG_HIDDEN);
            lv_obj_scroll_to_y(s_debug_panel, 0, LV_ANIM_OFF);
            s_debug_details_force = 1U;
            app_ui_update_debug();
        }
        else
        {
            lv_obj_add_flag(s_debug_panel, LV_OBJ_FLAG_HIDDEN);
            s_debug_details_force = 0U;
            s_debug_details_paused = 0U;
        }
    }
}

static void app_ui_make_user_page(void)
{
    s_user_page = app_ui_watch_home_create(&s_snapshot, app_ui_watch_action,
                                          app_ui_navigate, app_ui_watch_command);
}

static void app_ui_make_debug_page(void)
{
    lv_obj_t *panel;
    lv_obj_t *summary;
    lv_obj_t *title;
    lv_obj_t *button;

    s_debug_page = app_ui_theme_screen(true);
    for (unsigned int i = 0U; i < APP_UI_MAX_DETECTIONS; i++) {
        lv_obj_t *box = lv_obj_create(s_debug_page);
        lv_obj_remove_style_all(box);
        lv_obj_set_style_bg_opa(box, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(box, 2, 0);
        lv_obj_set_style_border_color(box, lv_color_hex(APP_UI_COLOR_ACCENT), 0);
        lv_obj_set_style_pad_all(box, 2, 0);
        lv_obj_remove_flag(box, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(box, LV_OBJ_FLAG_HIDDEN);
        s_detection_boxes[i] = box;
        s_detection_labels[i] = lv_label_create(box);
        lv_obj_set_style_text_font(s_detection_labels[i], &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(s_detection_labels[i], lv_color_hex(0xFFFFFFU), 0);
        lv_obj_remove_flag(s_detection_labels[i], LV_OBJ_FLAG_CLICKABLE);
    }
    summary = app_ui_card(s_debug_page, 8, 8, 784, 72);
    lv_obj_set_style_bg_opa(summary, LV_OPA_60, 0);
    lv_obj_set_style_border_width(summary, 0, 0);
    lv_obj_set_style_radius(summary, 10, 0);
    s_debug_summary = app_ui_label(summary,
                                   "Camera WAIT | FPS -- | AI -- Hz (PENDING)\n"
                                   "Touch OFF IC0 Err0 Retry0 K0",
                                   14, 12, 754);
    lv_obj_set_style_text_font(s_debug_summary, &lv_font_montserrat_20, 0);
    lv_label_set_long_mode(s_debug_summary, LV_LABEL_LONG_CLIP);

    panel = app_ui_card(s_debug_page, APP_UI_DEBUG_PANEL_X,
                        APP_UI_DEBUG_PANEL_Y, APP_UI_DEBUG_PANEL_WIDTH,
                        APP_UI_DEBUG_PANEL_HEIGHT);
    s_debug_panel = panel;
    s_debug_details_open = 0U;
    lv_obj_add_flag(panel, LV_OBJ_FLAG_HIDDEN);
    /* Long hardware diagnostics can scroll; the return button stays outside. */
    lv_obj_add_flag(panel, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_scroll_dir(panel, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(panel, LV_SCROLLBAR_MODE_AUTO);
    title = app_ui_label(panel, "摄像头预览 / 调试诊断\n向下滚动查看", 16, 12, 366);
    lv_obj_set_style_text_color(title, lv_color_hex(APP_UI_COLOR_ACCENT), 0);
    s_debug_values = app_ui_label(panel, "等待状态更新", 16, 64, 366);
    button = app_ui_visual_button(s_debug_page, "诊断", 512, 422, 96, 48);
    s_debug_details_button = button;
    lv_obj_add_event_cb(button, app_ui_debug_details_event, LV_EVENT_CLICKED, NULL);
    button = app_ui_visual_button(s_debug_page, "返回主页", 624, 422, 160, 48);
    lv_obj_add_event_cb(button, app_ui_mode_event, LV_EVENT_CLICKED, NULL);
}

static void app_ui_update_user(void)
{
    char reminder[128];
    uint8_t attention = app_ui_environment_summary(reminder, sizeof(reminder));
    app_ui_watch_home_update(app_ui_reason_name(s_snapshot.climate.reason),
                             reminder, attention);
    app_ui_controls_update(&s_snapshot);
    app_ui_environment_update();
    app_ui_journal_update();
}

static void app_ui_update_debug(void)
{
    uint32_t inference_rate_x10 = 0U;
    const app_ui_snapshot_t *snapshot = &s_snapshot;
    const char *camera;
    const char *camera_short;
    uint32_t now = app_ui_tick_ms();
    uint32_t frame_age = now - snapshot->camera.last_frame_ms;
    char frame_rate[16];
    char inference_rate[24];
    char summary[256];
    uint8_t mask = app_climate_output_mask(&snapshot->climate);

    if (!snapshot->simulated && snapshot->camera.frame_rate_valid)
    {
        (void)snprintf(frame_rate, sizeof(frame_rate), "%lu.%lu",
                       (unsigned long)(snapshot->camera.frame_rate_x10 / 10U),
                       (unsigned long)(snapshot->camera.frame_rate_x10 % 10U));
    }
    else
    {
        (void)snprintf(frame_rate, sizeof(frame_rate), "--");
    }
    if (!snapshot->simulated && snapshot->ai.completed_rate_valid && snapshot->climate.people_fresh)
    {
        inference_rate_x10 = snapshot->ai.completed_rate_x10;
        (void)snprintf(inference_rate, sizeof(inference_rate), "%lu.%lu",
                       (unsigned long)(inference_rate_x10 / 10U),
                       (unsigned long)(inference_rate_x10 % 10U));
    }
    else
    {
        (void)snprintf(inference_rate, sizeof(inference_rate), "--");
    }
    if (snapshot->ai.error == APP_AI_ERROR_MODEL_WEIGHTS)
    {
        camera = "模型权重不匹配";
        camera_short = "MODEL";
    }
    else if (snapshot->ai.stage == APP_AI_STAGE_ERROR &&
             snapshot->ai.error <= -1200 && snapshot->ai.error >= -1209)
    {
        camera = "模型接口不匹配";
        camera_short = "MODEL";
    }
    else if (snapshot->simulated && snapshot->camera_start_status == -1001)
    {
        camera = "未接硬件";
        camera_short = "PC";
    }
    else if (snapshot->camera_init_status != 0 &&
             snapshot->camera_init_status != -1000)
    {
        camera = "初始化异常";
        camera_short = "ERR";
    }
    else if (snapshot->camera_start_status == 0)
    {
        camera = "运行";
        camera_short = snapshot->camera.frame_rate_valid && frame_age > 1500U ?
                       "STALL" : "RUN";
    }
    else if (snapshot->camera_start_status == -1000)
    {
        camera = "等待初始化";
        camera_short = "WAIT";
    }
    else
    {
        camera = "初始化异常";
        camera_short = "ERR";
    }
    (void)snprintf(summary, sizeof(summary),
                   "Camera %s | FPS %s | AI %s Hz (%s)\n"
                   "Touch %s IC%u Err%lu Retry%lu K%u",
                   camera_short, frame_rate, inference_rate,
                   app_ui_ai_stage_name(snapshot->ai.stage),
                   snapshot->touch.ready ? "READY" : "OFF",
                   (unsigned int)snapshot->touch.controller,
                   (unsigned long)snapshot->touch.error_count,
                   (unsigned long)snapshot->touch.recovery_count,
                   (unsigned int)snapshot->touch.error_kind);
    app_ui_set_text(s_debug_summary, summary);

    /* Keep the large diagnostic string and its scrollable label dormant while
       previewing. Updating a hidden panel adds work without helping the user. */
    if (s_debug_details_open == 0U)
    {
        return;
    }
    if (snapshot->touch.pressed != 0U || lv_obj_is_scrolling(s_debug_panel))
    {
        /* Preserve label geometry and the reader's scroll position throughout
           a gesture. The small independent status strip continues updating. */
        s_debug_details_paused = 1U;
        return;
    }
    if (s_debug_details_paused != 0U)
    {
        s_debug_details_force = 1U;
        s_debug_details_paused = 0U;
    }
    if (s_debug_details_force == 0U &&
        (uint32_t)(now - s_debug_details_last_update) < APP_UI_DEBUG_DETAIL_UPDATE_MS)
    {
        return;
    }
    s_debug_details_force = 0U;
    s_debug_details_last_update = now;
    app_ui_format_perf();
    if (!snapshot->simulated && snapshot->temperature_source == APP_TEMPERATURE_DHT11) {
        if (snapshot->dht.has_sample) {
            (void)snprintf(s_sensor_text, sizeof(s_sensor_text),
                "DHT11 PG3 A%lu OK%lu Fail%lu\nE%lu Streak%lu Retry%lu ms\nLast good %lu s / ReadOK%u\n",
                (unsigned long)snapshot->dht.attempts,
                (unsigned long)snapshot->dht.successes,
                (unsigned long)snapshot->dht.failures,
                (unsigned long)snapshot->dht.last_status,
                (unsigned long)snapshot->dht.consecutive_failures,
                (unsigned long)snapshot->dht.retry_interval_ms,
                (unsigned long)((now - snapshot->dht.last_success_ms) / 1000U),
                (unsigned int)snapshot->dht.last_read_ok);
        } else {
            (void)snprintf(s_sensor_text, sizeof(s_sensor_text),
                "DHT11 PG3 A%lu OK%lu Fail%lu\nE%lu Streak%lu Retry%lu ms\nLast good NONE / ReadOK%u\n",
                (unsigned long)snapshot->dht.attempts,
                (unsigned long)snapshot->dht.successes,
                (unsigned long)snapshot->dht.failures,
                (unsigned long)snapshot->dht.last_status,
                (unsigned long)snapshot->dht.consecutive_failures,
                (unsigned long)snapshot->dht.retry_interval_ms,
                (unsigned int)snapshot->dht.last_read_ok);
        }
    } else {
        s_sensor_text[0] = '\0';
    }
    (void)snprintf(s_debug_values_text, sizeof(s_debug_values_text),
                   "Model %s\n"
                   "%s%s"
                   "Touch %s IC%u E%lu R%lu\n"
                   "Kind%u HAL%lu Err%08lX\n"
                   "State%lu ISR%08lX\n"
                   "CR2%08lX Elapsed%lu ms\n"
                   "BusRetry%lu Age%lu ms\n"
                   "Init%ld Start%ld CFG%ld\n"
                   "AI %s E%ld N%lu/%lu P%lu\n"
                   "PP%ld Pipe2%lu Run%lu ms\n"
                   "Input OK%lu Drop%lu\n"
                   "Vision F%lu Boxes%u V%u CaptureAge%lu ms\n"
                   "ResultAge%lu ms Retry%lu/%s E%ld\n"
                   "UART Q%lu Drop%lu Err%lu S%lu\n"
                   "UI %lu/%lu ms ISP%lu/%lu\n"
                   "ISP time %lu/%lu ms\n"
                   "Camera %s / FPS %s\n推理更新率 %s Hz / %lu ms\n"
                   "软件请求 R1:%s R2:%s R3:%s R4:%s\n"
                   "People %ld / AI %s\nALS %u / %s\n"
                   "温湿度 %s / 数据年龄 %lu s\n"
                   "策略 %s\n"
                   "Init %ld TPG %ld/S%ld\n"
                   "ISP %ld/%ld Exp%ldus Gain%ldmdB\n"
                   "Set%ld V%lu SH%lu Step%ld Err%ld\n"
                   "CAM CFG %ld P%lu/%lu\n"
                   "FB %08lX DMA %08lX\n"
                   "LCD BG %08lX P%lu C%08lX\n"
                   "BG F%lu Show%lu Drop%lu Pend%u/%u\n"
                   "BG Err%lu DMA%lu S%ld\n"
                   "LCD FG %08lX P%lu C%08lX\n"
                   "Pixels %04X/%04X\n"
                   "Frames %lu Buff%u/%u\n"
                   "Frame age %lu ms\n"
                   "CSI %08lX/%08lX E%08lX/%08lX\n"
                   "Pipe %lu/%lu/%lu Raw%08lX H%08lX\n"
                   "Last sample: Temperature %.1f C Humidity %.0f %%\n"
                   "State %s Target %u C\n"
                   "Touch %s %s X%u Y%u\n"
                   "Diag FT%u GT%u FA%u GA%u\n"
                   "Bus %u/%u R%u I%u\n"
                   "Scan %lu Finger %u\n"
                   "Touch IC%u Error%lu Retry%lu Age%lu ms",
                   APP_MODEL_DESCRIPTION,
                   s_perf_text,
                   s_sensor_text,
                   snapshot->touch.ready ? "READY" : "NO DEV",
                   (unsigned int)snapshot->touch.controller,
                   (unsigned long)snapshot->touch.error_count,
                   (unsigned long)snapshot->touch.recovery_count,
                   (unsigned int)snapshot->touch.error_kind,
                   (unsigned long)snapshot->touch.last_hal_status,
                   (unsigned long)snapshot->touch.last_hal_error,
                   (unsigned long)snapshot->touch.last_hal_state,
                   (unsigned long)snapshot->touch.last_isr,
                   (unsigned long)snapshot->touch.last_cr2,
                   (unsigned long)snapshot->touch.last_elapsed_ms,
                   (unsigned long)snapshot->touch.bus_recovery_count,
                   (unsigned long)(now - snapshot->touch.last_success_ms),
                   (long)snapshot->camera_init_status,
                   (long)snapshot->camera_start_status,
                   (long)snapshot->camera.config_status,
                   app_ui_ai_stage_name(snapshot->ai.stage),
                   (long)snapshot->ai.error,
                   (unsigned long)snapshot->ai.nn_started,
                   (unsigned long)snapshot->ai.nn_completed,
                   (unsigned long)snapshot->ai.pp_completed,
                   (long)snapshot->ai.pp_status,
                   (unsigned long)snapshot->ai.pipe2_frames,
                   (unsigned long)snapshot->ai.inference_ms,
                   (unsigned long)snapshot->ai.input_published,
                   (unsigned long)snapshot->ai.input_dropped,
                   (unsigned long)snapshot->vision.frame_id,
                   (unsigned int)(snapshot->vision.valid ? snapshot->vision.count : 0U),
                   (unsigned int)snapshot->vision.valid,
                   (unsigned long)(snapshot->vision.valid ? now - snapshot->vision.captured_ms : 0U),
                   (unsigned long)(snapshot->vision.valid ? now - snapshot->vision.completed_ms : 0U),
                   (unsigned long)snapshot->ai.recovery_count,
                   snapshot->ai.recovery_active ? "ACTIVE" : "IDLE",
                   (long)snapshot->ai.last_recovery_error,
                   (unsigned long)snapshot->uart.queued_bytes,
                   (unsigned long)snapshot->uart.dropped_bytes,
                   (unsigned long)snapshot->uart.tx_errors,
                   (unsigned long)snapshot->uart.last_status,
                   (unsigned long)snapshot->ui_last_ms,
                   (unsigned long)snapshot->ui_max_ms,
                   (unsigned long)snapshot->ai.isp_vsyncs,
                   (unsigned long)snapshot->ai.isp_runs,
                   (unsigned long)snapshot->ai.isp_last_ms,
                   (unsigned long)snapshot->ai.isp_max_ms,
                   camera, frame_rate, inference_rate,
                   (unsigned long)snapshot->nn_period_ms,
                   (mask & 0x01U) ? "ON" : "OFF", (mask & 0x02U) ? "ON" : "OFF",
                   (mask & 0x04U) ? "ON" : "OFF", (mask & 0x08U) ? "ON" : "OFF",
                   (long)snapshot->people_count,
                   snapshot->climate.people_fresh ? "新鲜" : "UNKNOWN",
                   (unsigned int)snapshot->als,
                   snapshot->climate.als_fresh ? "新鲜" : "未知",
                   snapshot->climate.sensor_fresh ? "有效" : "失效",
                   (unsigned long)((app_ui_tick_ms() - snapshot->sensor_updated_ms) / 1000U),
                   app_ui_reason_name(snapshot->climate.reason),
                   (long)snapshot->camera_init_status,
                   (long)snapshot->camera.sensor_test_pattern_mode,
                   (long)snapshot->camera.sensor_test_pattern_status,
                   (long)snapshot->camera.isp_run_status,
                   (long)snapshot->camera.isp_native_status,
                   (long)snapshot->camera.sensor_exposure_us,
                   (long)snapshot->camera.sensor_gain_mdb,
                   (long)snapshot->camera.exposure_request_us,
                   (unsigned long)snapshot->camera.exposure_vmax_lines,
                   (unsigned long)snapshot->camera.exposure_shutter_lines,
                   (long)snapshot->camera.exposure_failure_stage,
                   (long)snapshot->camera.exposure_driver_status,
                   (long)snapshot->camera.config_status,
                   (unsigned long)snapshot->camera.configured_pitch,
                   (unsigned long)snapshot->camera.hardware_pitch,
                   (unsigned long)snapshot->lcd.address,
                   (unsigned long)snapshot->camera.dma_address,
                   (unsigned long)snapshot->lcd.address,
                   (unsigned long)snapshot->lcd.pitch_bytes,
                   (unsigned long)snapshot->lcd.control,
                   (unsigned long)snapshot->lcd.background_frame_id,
                   (unsigned long)snapshot->lcd.background_presented,
                   (unsigned long)snapshot->lcd.background_dropped,
                   (unsigned int)snapshot->lcd.pending_index,
                   (unsigned int)snapshot->lcd.background_reload_pending,
                   (unsigned long)snapshot->lcd.background_reload_errors,
                   (unsigned long)snapshot->lcd.background_target_errors,
                   (long)snapshot->lcd.background_last_status,
                   (unsigned long)snapshot->lcd.foreground_address,
                   (unsigned long)snapshot->lcd.foreground_pitch_bytes,
                   (unsigned long)snapshot->lcd.foreground_control,
                   (unsigned int)snapshot->lcd.pixel_min,
                   (unsigned int)snapshot->lcd.pixel_max,
                   (unsigned long)snapshot->camera.frame_count,
                   (unsigned int)snapshot->lcd.display_index,
                   (unsigned int)snapshot->lcd.fill_index,
                   (unsigned long)frame_age,
                   (unsigned long)snapshot->camera.csi_status0,
                   (unsigned long)snapshot->camera.csi_status1,
                   (unsigned long)snapshot->camera.csi_error1,
                   (unsigned long)snapshot->camera.csi_error2,
                   (unsigned long)snapshot->camera.pipe0_state,
                   (unsigned long)snapshot->camera.pipe1_state,
                   (unsigned long)snapshot->camera.pipe2_state,
                   (unsigned long)snapshot->camera.pipe0_data_count,
                   (unsigned long)snapshot->camera.hal_error,
                   (double)snapshot->temperature, (double)snapshot->humidity,
                   app_ui_presence_name(snapshot->climate.presence),
                   (unsigned int)snapshot->climate.target_temperature,
                   snapshot->touch.ready ? "READY" : "NO DEV",
                   snapshot->touch.pressed ? "DOWN" : "IDLE",
                   (unsigned int)snapshot->touch.x, (unsigned int)snapshot->touch.y,
                   (unsigned int)snapshot->touch.ft_probe,
                   (unsigned int)snapshot->touch.gt_probe,
                   (unsigned int)snapshot->touch.ft_ack,
                   (unsigned int)snapshot->touch.gt_ack,
                   (unsigned int)snapshot->touch.scl_level,
                   (unsigned int)snapshot->touch.sda_level,
                   (unsigned int)snapshot->touch.reset_level,
                   (unsigned int)snapshot->touch.interrupt_level,
                   (unsigned long)snapshot->touch.scan_count,
                   (unsigned int)snapshot->touch.finger_count,
                   (unsigned int)snapshot->touch.controller,
                   (unsigned long)snapshot->touch.error_count,
                   (unsigned long)snapshot->touch.recovery_count,
                   (unsigned long)(now - snapshot->touch.last_success_ms));
    app_ui_set_text(s_debug_values, s_debug_values_text);
}

void app_ui_init(app_ui_action_cb_t action_cb)
{
    static uint8_t draw_buffer[2][LCD_FG_WIDTH * APP_UI_FLUSH_ROWS *
                                  APP_UI_LVGL_PIXEL_BYTES]
        __attribute__((aligned(32)));
    lv_indev_t *indev;

    s_action_cb = action_cb;
    s_process_count = 0U;
    s_active_draw_buffer = NULL;
    s_pending_flush_display = NULL;
    app_detection_smoother_init(&s_detection_smoother);
    lv_init();
    lv_tick_set_cb(app_ui_tick);
    s_display = lv_display_create(LCD_FG_WIDTH, LCD_FG_HEIGHT);
    lv_display_set_color_format(s_display, LV_COLOR_FORMAT_ARGB8888);
    lv_display_set_flush_cb(s_display, app_ui_flush);
    lv_display_set_buffers(s_display, draw_buffer[0], draw_buffer[1],
                           sizeof(draw_buffer[0]),
                           LV_DISPLAY_RENDER_MODE_PARTIAL);
    indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, app_ui_touch_read);
    app_environment_init(&s_environment_alerts, NULL, APP_ENV_SOURCE_BOARD);
    app_event_journal_init(&s_event_records, APP_EVENT_SOURCE_BOARD);
    s_presence_observed = 0U;
    s_lifecycle_ready = 1U;
    app_ui_environment_init(&s_environment_alerts, app_ui_environment_events);
    app_ui_journal_init(app_ui_manual_capture, NULL);
    app_ui_make_user_page();
    app_ui_controls_init(s_user_page, s_action_cb);
    app_ui_make_debug_page();
    app_ui_insights_init(app_ui_navigate, app_ui_demo_event);
    lv_screen_load(s_user_page);
    /* Freshness and requested outputs arrive through the controller snapshot. */
    s_last_update = app_ui_tick();
}

void app_ui_set_snapshot(const app_ui_snapshot_t *snapshot)
{
    if (snapshot != NULL)
    {
        s_snapshot = *snapshot;
        app_ui_watch_home_observe_target(snapshot->climate.target_temperature);
        (void)app_detection_smoother_update(&s_detection_smoother, &snapshot->vision,
                                           snapshot->climate.people_fresh);
        app_ui_insights_observe(snapshot, app_ui_tick_ms());
        app_ui_observe_lifecycle();
    }
}

void app_ui_set_demo_callback(app_ui_demo_cb_t demo_cb)
{
    s_demo_cb = demo_cb;
}

void app_ui_process(void)
{
    uint32_t now;

    s_active_draw_buffer = app_lcd_get_fg_draw_buffer();
    if (s_active_draw_buffer == NULL) { return; }
    if (s_pending_flush_display != NULL) {
        lv_display_t *pending_display = s_pending_flush_display;
        s_pending_flush_display = NULL;
        app_ui_flush(pending_display, &s_pending_flush_area, s_pending_flush_pixels);
        /* A completed last block was submitted above. Acquire the next
           writable buffer before allowing another LVGL refresh. */
        s_active_draw_buffer = app_lcd_get_fg_draw_buffer();
        if (s_active_draw_buffer == NULL) { return; }
    }

    s_process_count++;
    now = app_ui_tick();
    if (lv_screen_active() == s_debug_page) {
        const app_vision_result_t *drawn = &s_detection_smoother.output;
        uint8_t valid = s_snapshot.climate.people_fresh && drawn->valid &&
                        drawn->count > 0U && drawn->count <= APP_UI_MAX_DETECTIONS;
        /* New results animate at the display cadence; large labels retain
           their slower cadence and remain frozen during diagnostic scrolling. */
        if (!s_overlay_initialized || valid != s_overlay_valid ||
            s_snapshot.vision.frame_id != s_overlay_frame_id) {
            app_ui_update_detections();
            s_overlay_frame_id = s_snapshot.vision.frame_id;
            s_overlay_valid = valid;
            s_overlay_initialized = 1U;
        }
    }
    /* Keep bring-up diagnostics independent of the HAL tick. */
    if ((s_process_count == 1U) ||
        ((now - s_last_update) >= APP_UI_UPDATE_MS))
    {
        if (lv_screen_active() == s_user_page)
        {
            app_ui_update_user();
        }
        else if (lv_screen_active() == s_debug_page)
        {
            app_ui_update_debug();
        }
        else if (app_ui_insights_active()) {
            app_ui_insights_update(&s_snapshot, app_ui_reason_name(s_snapshot.climate.reason));
        }
        s_last_update = now;
    }
    lv_timer_handler();
    s_active_draw_buffer = NULL;
}
