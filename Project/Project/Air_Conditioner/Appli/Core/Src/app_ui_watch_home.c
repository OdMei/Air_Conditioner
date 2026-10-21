#include "app_ui_watch_home.h"
#include "app_ui_watch_assets.h"
#include "app_ui_theme.h"
#include "app_ui_temperature_input.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define WATCH_BACKGROUND (0x071A2AU)
#define WATCH_CARD       (0x0D2C40U)
#define WATCH_CARD_UNKNOWN (0xCED8DBU)
#define WATCH_TEMPERATURE_CARD (0xA3ED49U)
#define WATCH_HUMIDITY_CARD (0x59EDDEU)
#define WATCH_SENSOR_MUTED (0x435B65U)
#define WATCH_TEXT       (0xDDEAF1U)
#define WATCH_MUTED      (0x96B6C9U)
#define WATCH_ACCENT     (0x6EF2D1U)
#define WATCH_CAUTION    (0xFFD18BU)
#define WATCH_INK        (0x143C3BU)
#define WATCH_READOUT_Y  (86)
#define WATCH_READOUT_GAP (8)
#define WATCH_SENSOR_TOP (60)
#define WATCH_SENSOR_LEFT (400)
#define WATCH_SENSOR_WIDTH (148)
#define WATCH_SENSOR_HEIGHT (70)
#define WATCH_SENSOR_GAP (12)

static lv_obj_t *s_page;
static lv_obj_t *s_foreground;
static lv_obj_t *s_power;
static lv_obj_t *s_power_status;
static lv_obj_t *s_source;
static lv_obj_t *s_arc;
static lv_obj_t *s_readout;
static lv_obj_t *s_target;
static lv_obj_t *s_target_unit;
static lv_obj_t *s_mode[2];
static lv_obj_t *s_temperature_card;
static lv_obj_t *s_humidity_card;
static lv_obj_t *s_temperature_caption;
static lv_obj_t *s_humidity_caption;
static lv_obj_t *s_temperature;
static lv_obj_t *s_humidity;
static lv_obj_t *s_temperature_unit;
static lv_obj_t *s_humidity_unit;
static lv_obj_t *s_people;
static lv_obj_t *s_light;
static lv_obj_t *s_quality;
static lv_obj_t *s_strategy;
static lv_obj_t *s_reminder;
static lv_obj_t *s_environment;
static lv_obj_t *s_bottom[6];
static lv_obj_t *s_bottom_icons[6];
static lv_obj_t *s_bottom_values[6];
static lv_obj_t *s_details;
static lv_obj_t *s_details_text;
static lv_obj_t *s_demo_button;
static const app_ui_snapshot_t *s_snapshot;
static app_ui_action_cb_t s_action;
static app_ui_navigation_cb_t s_navigate;
static app_ui_watch_command_cb_t s_command;
static uint8_t s_target_initialized;
static uint8_t s_last_target;
static char s_reason[160];

static void passive(lv_obj_t *object)
{
    lv_obj_remove_flag(object, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE |
        LV_OBJ_FLAG_EVENT_BUBBLE | LV_OBJ_FLAG_GESTURE_BUBBLE);
}

static void text_color(lv_obj_t *object, uint32_t color)
{
    lv_color_t value = lv_color_hex(color);
    if (!lv_color_eq(lv_obj_get_style_text_color(object, 0), value)) {
        lv_obj_set_style_text_color(object, value, 0);
    }
}

static lv_obj_t *label(lv_obj_t *parent, const char *text,
                        int x, int y, int width, uint32_t color)
{
    lv_obj_t *object = app_ui_theme_label(parent, text, x, y, width);
    text_color(object, color);
    lv_label_set_long_mode(object, LV_LABEL_LONG_CLIP);
    return object;
}

static lv_obj_t *flat(lv_obj_t *parent, int x, int y,
                       int width, int height, uint32_t color, int radius)
{
    lv_obj_t *object = lv_obj_create(parent);
    lv_obj_remove_style_all(object);
    lv_obj_set_pos(object, x, y);
    lv_obj_set_size(object, width, height);
    lv_obj_set_style_bg_color(object, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(object, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(object, radius, 0);
    passive(object);
    return object;
}

static lv_obj_t *icon(lv_obj_t *parent, app_ui_watch_icon_t kind,
                       int x, int y, uint32_t color)
{
    lv_obj_t *object = lv_image_create(parent);
    lv_image_set_src(object, app_ui_watch_icon(kind));
    lv_obj_set_pos(object, x, y);
    lv_obj_set_style_image_recolor(object, lv_color_hex(color), 0);
    lv_obj_set_style_image_recolor_opa(object, LV_OPA_COVER, 0);
    passive(object);
    return object;
}

static lv_obj_t *button(lv_obj_t *parent, const char *text,
                         int x, int y, int width, int height)
{
    lv_obj_t *object = app_ui_theme_button(parent, text, x, y, width, height);
    lv_obj_set_style_bg_color(object, lv_color_hex(WATCH_CARD), 0);
    lv_obj_set_style_bg_color(object, lv_color_hex(0x12483FU), LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(object, lv_color_hex(0x286159U), LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(object, lv_color_hex(0x286159U),
        LV_STATE_CHECKED | LV_STATE_PRESSED);
    lv_obj_set_style_border_width(object, 1, 0);
    lv_obj_set_style_border_color(object, lv_color_hex(0x2A4354U), 0);
    lv_obj_set_style_radius(object, 14, 0);
    lv_obj_set_style_text_color(object, lv_color_hex(WATCH_TEXT), 0);
    lv_obj_set_style_text_color(object, lv_color_hex(WATCH_ACCENT), LV_STATE_CHECKED);
    lv_obj_t *caption = lv_obj_get_child(object, 0);
    lv_label_set_long_mode(caption, LV_LABEL_LONG_CLIP);
    return object;
}

static void action_event(lv_event_t *event)
{
    if (lv_event_get_code(event) == LV_EVENT_CLICKED && s_action) {
        app_climate_action_t action = (app_climate_action_t)(uintptr_t)lv_event_get_user_data(event);
        if (action == APP_ACTION_TEMP_UP || action == APP_ACTION_TEMP_DOWN) {
            app_ui_temperature_input_step(action == APP_ACTION_TEMP_UP ? 1 : -1);
        } else { s_action(action); }
    }
}

static void mode_event(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED || !s_snapshot || !s_action) { return; }
    uint8_t mode = (uint8_t)(uintptr_t)lv_event_get_user_data(event);
    /* Both choices are selectors; tapping the current mode is a no-op. */
    if (s_snapshot->climate.mode != mode) { s_action(APP_ACTION_MODE_NEXT); }
}

static void navigation_event(lv_event_t *event)
{
    if (lv_event_get_code(event) == LV_EVENT_CLICKED && s_navigate) {
        s_navigate((uint8_t)(uintptr_t)lv_event_get_user_data(event));
    }
}

static void command_event(lv_event_t *event)
{
    if (lv_event_get_code(event) == LV_EVENT_CLICKED && s_command) {
        s_command((uint8_t)(uintptr_t)lv_event_get_user_data(event));
    }
}

static void target_y_exec(void *object, int32_t y)
{
    lv_obj_set_y((lv_obj_t *)object, y);
}

static void arc_exec(void *object, int32_t value)
{
    /* Display interpolation never emits climate actions. The arc has no
     * input callback and its value is a visual angle, not a setpoint. */
    lv_arc_set_value((lv_obj_t *)object, value);
}

void app_ui_watch_home_enter(void)
{
    if (!s_temperature_card || !s_humidity_card) { return; }
    lv_obj_t *cards[2] = {s_temperature_card, s_humidity_card};
    const int32_t y[2] = {WATCH_SENSOR_TOP, WATCH_SENSOR_TOP};
    /* A short local entrance avoids a full-screen opacity layer on the
     * 160KB heap. Measurements themselves retain the published value. */
    for (unsigned int i = 0U; i < 2U; i++) {
        lv_anim_delete(cards[i], target_y_exec);
        lv_anim_t animation;
        lv_anim_init(&animation);
        lv_anim_set_var(&animation, cards[i]);
        lv_anim_set_exec_cb(&animation, target_y_exec);
        lv_anim_set_values(&animation, y[i] + 6, y[i]);
        lv_anim_set_duration(&animation, 140);
        lv_anim_set_path_cb(&animation, lv_anim_path_ease_out);
        lv_anim_start(&animation);
    }
}

static void layout_readout(void)
{
    const lv_font_t *number_font = lv_obj_get_style_text_font(s_target, 0);
    const lv_font_t *unit_font = lv_obj_get_style_text_font(s_target_unit, 0);
    const char *number = lv_label_get_text(s_target);
    const char *unit = lv_label_get_text(s_target_unit);
    int32_t number_width = lv_text_get_width(number, (uint32_t)strlen(number), number_font, 0);
    int32_t unit_width = lv_text_get_width(unit, (uint32_t)strlen(unit), unit_font, 0);
    int32_t baseline = app_font_watch_96.line_height - app_font_watch_96.base_line;
    int32_t width = number_width + WATCH_READOUT_GAP + unit_width;
    /* Center the measured number/unit pair, rather than two independent
     * label boxes. Font baselines keep the unit aligned during feedback. */
    lv_obj_set_size(s_readout, width, app_font_watch_96.line_height);
    lv_obj_set_x(s_readout, 186 - width / 2);
    lv_obj_set_width(s_target, number_width);
    lv_obj_set_pos(s_target, 0, baseline - (number_font->line_height - number_font->base_line));
    lv_obj_set_width(s_target_unit, unit_width);
    lv_obj_set_pos(s_target_unit, number_width + WATCH_READOUT_GAP,
        baseline - (unit_font->line_height - unit_font->base_line));
}

static void layout_sensor_readout(lv_obj_t *number, lv_obj_t *unit)
{
    const lv_font_t *number_font = lv_obj_get_style_text_font(number, 0);
    const lv_font_t *unit_font = lv_obj_get_style_text_font(unit, 0);
    const char *number_text = lv_label_get_text(number);
    const char *unit_text = lv_label_get_text(unit);
    int32_t number_width = lv_text_get_width(number_text,
        (uint32_t)strlen(number_text), number_font, 0);
    int32_t unit_width = lv_text_get_width(unit_text,
        (uint32_t)strlen(unit_text), unit_font, 0);
    int32_t x = (WATCH_SENSOR_WIDTH - number_width - WATCH_READOUT_GAP - unit_width) / 2;
    int32_t y = 25;
    int32_t baseline = y + number_font->line_height - number_font->base_line;
    /* Center the measured pair below its caption; units share the number's
     * baseline for decimal and unavailable readings in the compact row. */
    lv_obj_set_width(number, number_width);
    lv_obj_set_pos(number, x, y);
    lv_obj_set_width(unit, unit_width);
    lv_obj_set_pos(unit, x + number_width + WATCH_READOUT_GAP,
        baseline - (unit_font->line_height - unit_font->base_line));
}

static void update_target(uint8_t target)
{
    char text[8];
    uint8_t valid = target >= 16U && target <= 30U;
    if (!valid) {
        app_ui_theme_text(s_target, "--");
        lv_obj_set_style_text_font(s_target, &lv_font_montserrat_48, 0);
        lv_anim_delete(s_arc, arc_exec);
        lv_anim_delete(s_readout, target_y_exec);
        lv_obj_set_y(s_readout, WATCH_READOUT_Y);
        layout_readout();
        lv_arc_set_value(s_arc, 0);
        s_target_initialized = 0U;
        return;
    }
    if (lv_obj_get_style_text_font(s_target, 0) != &app_font_watch_96) {
        lv_obj_set_style_text_font(s_target, &app_font_watch_96, 0);
    }
    (void)snprintf(text, sizeof(text), "%u", (unsigned int)target);
    app_ui_theme_text(s_target, text);
    layout_readout();
    int32_t end = ((int32_t)target - 16) * 270 / 14;
    if (app_ui_temperature_input_dragging()) {
        lv_anim_delete(s_arc, arc_exec);
        lv_anim_delete(s_readout, target_y_exec);
        lv_obj_set_y(s_readout, WATCH_READOUT_Y);
        s_last_target = target;
        s_target_initialized = 1U;
        return;
    }
    if (!s_target_initialized) {
        lv_arc_set_value(s_arc, end);
    } else if (target != s_last_target) {
        lv_anim_t animation;
        lv_anim_delete(s_arc, arc_exec);
        lv_anim_init(&animation);
        lv_anim_set_var(&animation, s_arc);
        lv_anim_set_exec_cb(&animation, arc_exec);
        lv_anim_set_values(&animation, lv_arc_get_value(s_arc), end);
        lv_anim_set_duration(&animation, 240);
        lv_anim_set_path_cb(&animation, lv_anim_path_ease_out);
        lv_anim_start(&animation);
        lv_anim_delete(s_readout, target_y_exec);
        lv_anim_init(&animation);
        lv_anim_set_var(&animation, s_readout);
        lv_anim_set_exec_cb(&animation, target_y_exec);
        lv_anim_set_values(&animation, WATCH_READOUT_Y + 5, WATCH_READOUT_Y);
        lv_anim_set_duration(&animation, 180);
        lv_anim_set_path_cb(&animation, lv_anim_path_ease_out);
        lv_anim_start(&animation);
    }
    s_last_target = target;
    s_target_initialized = 1U;
}

void app_ui_watch_home_observe_target(uint8_t target)
{
    if (s_arc) { app_ui_temperature_input_observe_target(target); }
}

void app_ui_watch_home_cancel_input(void)
{
    if (s_arc) { app_ui_temperature_input_cancel(); }
}

static void preview_target(uint8_t target)
{
    update_target(target);
}

static const char *presence_name(app_presence_t presence)
{
    switch (presence) {
    case APP_PRESENCE_SLEEP: return "暗光有人";
    case APP_PRESENCE_PRESENT: return "有人";
    case APP_PRESENCE_ABSENT: return "无人";
    default: return "未知";
    }
}

static const char *quality_name(app_data_quality_t quality)
{
    switch (quality) {
    case APP_DATA_FRESH: return "有效";
    case APP_DATA_STALE: return "数据过期";
    case APP_DATA_INVALID: return "数值无效";
    default: return "数据未更新";
    }
}

static const char *quality_short_name(app_data_quality_t quality)
{
    switch (quality) {
    case APP_DATA_FRESH: return "有效";
    case APP_DATA_STALE: return "过期";
    case APP_DATA_INVALID: return "无效";
    default: return "未更新";
    }
}

static void update_details(void)
{
    if (!s_snapshot || !s_details || lv_obj_has_flag(s_details, LV_OBJ_FLAG_HIDDEN)) { return; }
    const app_climate_state_t *state = &s_snapshot->climate;
    uint32_t now = app_ui_tick_ms();
    uint32_t elapsed = state->absence_ms / 1000U;
    uint8_t mask = app_climate_output_mask(state);
    /* This formatter runs only on the LVGL owner. Persistent scratch keeps
     * the 4096-byte DP thread stack clear for snprintf and LVGL, matching
     * the existing diagnostic formatter's storage model. */
    static char text[2304];
    static char sensing[200];
    static char absence[160];
    static char temperature[64];
    static char humidity[64];
    static char als[96];
    static char people[144];
    static char sensor[176];
    static char runtime[112];
    (void)snprintf(runtime, sizeof(runtime), "%s %02lu:%02lu:%02lu",
        s_snapshot->simulated ? "PC 模拟运行" : "实板运行",
        (unsigned long)(now / 3600000U), (unsigned long)((now / 60000U) % 60U),
        (unsigned long)((now / 1000U) % 60U));
    app_ui_insights_sensing_text(s_snapshot, sensing, sizeof(sensing));
    if (!state->people_fresh) {
        (void)snprintf(absence, sizeof(absence), "无人计时 --:--（检测未知，重新确认）");
    } else {
        (void)snprintf(absence, sizeof(absence), "无人计时 %02lu:%02lu | 10分节能 / 30分关机",
            (unsigned long)(elapsed / 60U), (unsigned long)(elapsed % 60U));
    }
    if (state->people_fresh && s_snapshot->people_valid && s_snapshot->people_count >= 0) {
        (void)snprintf(people, sizeof(people), "人员 %ld 人 / %s | 检测 %lu ms前",
            (long)s_snapshot->people_count,
            state->people_confirming ? "确认中" : presence_name(state->presence),
            (unsigned long)(now - s_snapshot->people_updated_ms));
    } else {
        (void)snprintf(people, sizeof(people), "人员 未知 / 检测未更新");
    }
    if (state->als_fresh && s_snapshot->als_valid) {
        (void)snprintf(als, sizeof(als), "光照 %s | ALS原始值 %u",
            state->light == APP_LIGHT_BRIGHT ? "明亮" : "暗", (unsigned int)s_snapshot->als);
    } else {
        (void)snprintf(als, sizeof(als), "光照 未知 | ALS数据未更新");
    }
    uint8_t cached = !s_snapshot->simulated &&
        s_snapshot->temperature_source == APP_TEMPERATURE_DHT11 && s_snapshot->dht.has_sample;
    if ((state->sensor_fresh || cached) && isfinite(s_snapshot->temperature)) {
        (void)snprintf(temperature, sizeof(temperature), "%s %.1f C / %s",
            state->sensor_fresh ? "实测温度" : "上次温度",
            (double)s_snapshot->temperature, quality_name(state->temperature_quality));
    } else {
        (void)snprintf(temperature, sizeof(temperature), "温度 -- C / %s",
            quality_name(state->temperature_quality));
    }
    if ((state->humidity_fresh || cached) && isfinite(s_snapshot->humidity) &&
        s_snapshot->humidity >= 0.0f && s_snapshot->humidity <= 100.0f) {
        (void)snprintf(humidity, sizeof(humidity), "%s %.0f %% / %s",
            state->humidity_fresh ? "实测湿度" : "上次湿度",
            (double)s_snapshot->humidity, quality_name(state->humidity_quality));
    } else {
        (void)snprintf(humidity, sizeof(humidity), "湿度 -- %% / %s",
            quality_name(state->humidity_quality));
    }
    if (s_snapshot->simulated) {
        (void)snprintf(sensor, sizeof(sensor), "PC 模拟 x%lu | 运行 %02lu:%02lu:%02lu",
            (unsigned long)(s_snapshot->simulation_speed ? s_snapshot->simulation_speed : 1U),
            (unsigned long)(now / 3600000U), (unsigned long)((now / 60000U) % 60U),
            (unsigned long)((now / 1000U) % 60U));
    } else if (s_snapshot->temperature_source == APP_TEMPERATURE_DHT11) {
        if (!s_snapshot->dht.has_sample) {
            if (s_snapshot->dht.attempts) {
                (void)snprintf(sensor, sizeof(sensor), "首次采样失败 E%lu / 暂无有效读数",
                    (unsigned long)s_snapshot->dht.last_status);
            } else {
                (void)snprintf(sensor, sizeof(sensor), "等待首次采样 / 暂无有效读数");
            }
        } else {
            (void)snprintf(sensor, sizeof(sensor), "DHT11 %s E%lu | 最后有效 %lu s前%s",
                s_snapshot->dht.last_read_ok ? "采样完成" : "采样失败",
                (unsigned long)s_snapshot->dht.last_status,
                (unsigned long)((now - s_snapshot->dht.last_success_ms) / 1000U),
                state->sensor_fresh && state->humidity_fresh ? "" : " / 上次读数，仅供参考");
        }
    } else {
        (void)snprintf(sensor, sizeof(sensor), "实板运行 / 温湿度来源未更新");
    }
    (void)snprintf(text, sizeof(text),
        "%s | 模式 %s\n%s\n策略原因：%s\n%s\n%s\n%s\n%s\n\n"
        "执行目标 %u C | 风档 %u/%u（选择/执行）\n"
        "手动睡眠 %s / 手动节能 %s\n自动睡眠 %s / 自动节能 %s\n"
        "当前执行：%s | %s%s\n保护等待 %lu s（软件制冷策略）\n"
        "左右摆风 %s / 上下摆风 %s / 灯光 %s\n\n"
        "%s\n%s\n%s\n\n"
        "软件请求（未接负载）\n制冷 %s  风机 %s  摆风 %s  灯光 %s",
        state->power_on ? "电源开启" : "电源关闭",
        state->mode == APP_CLIMATE_MODE_MANUAL_COOL ? "手动制冷" : "智能自动",
        runtime, s_reason, people, als, sensing, absence,
        (unsigned int)state->effective_target_temperature,
        (unsigned int)(state->turbo ? 5U : state->fan_speed),
        (unsigned int)((mask & 0x02U) ? state->effective_fan_speed : 0U),
        state->manual_sleep ? "开启" : "关闭",
        state->manual_energy_saving ? "开启" : "关闭",
        state->mode == APP_CLIMATE_MODE_AUTO && state->auto_sleep ? "开启" : "关闭",
        state->mode == APP_CLIMATE_MODE_AUTO && state->auto_energy_saving ? "开启" : "关闭",
        state->effective_sleep ? (state->manual_sleep ? "手动睡眠" : "自动睡眠") : "睡眠关闭",
        state->effective_eco ? (state->manual_energy_saving ? "手动节能" : "自动节能") : "节能关闭",
        state->turbo && state->effective_fan_speed < 5U ? " | 强劲受限" : "",
        (unsigned long)(state->cooling_wait_ms / 1000U + (state->cooling_wait_ms % 1000U != 0U)),
        state->swing_lr ? "开启" : "关闭", state->swing_ud ? "开启" : "关闭",
        state->lamp_on ? "开启" : "关闭", temperature, humidity, sensor,
        mask & 0x01U ? "ON" : "OFF", mask & 0x02U ? "ON" : "OFF",
        mask & 0x04U ? "ON" : "OFF", mask & 0x08U ? "ON" : "OFF");
    app_ui_theme_text(s_details_text, text);
    lv_obj_set_state(s_demo_button, LV_STATE_DISABLED, !s_snapshot->simulated);
}

static void details_close_event(lv_event_t *event)
{
    if (lv_event_get_code(event) == LV_EVENT_CLICKED) { app_ui_watch_home_close_details(); }
}

static void demo_event(lv_event_t *event)
{
    if (lv_event_get_code(event) == LV_EVENT_CLICKED && s_snapshot &&
        s_snapshot->simulated && s_navigate) { s_navigate(APP_UI_PAGE_DEMO); }
}

void app_ui_watch_home_close_details(void)
{
    if (s_details) { lv_obj_add_flag(s_details, LV_OBJ_FLAG_HIDDEN); }
}

void app_ui_watch_home_show_details(void)
{
    if (!s_page) { return; }
    if (!s_details) {
        s_details = flat(s_page, 0, 0, 800, 480, 0x000000U, 0);
        lv_obj_set_style_bg_opa(s_details, LV_OPA_60, 0);
        lv_obj_add_flag(s_details, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_t *panel = app_ui_theme_card(s_details, 80, 40, 640, 400);
        lv_obj_add_flag(panel, LV_OBJ_FLAG_CLICKABLE);
        label(panel, "策略与状态详情", 20, 16, 600, WATCH_ACCENT);
        lv_obj_t *scroll = flat(panel, 20, 54, 600, 268, APP_UI_COLOR_CARD, 0);
        lv_obj_add_flag(scroll, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_scroll_dir(scroll, LV_DIR_VER);
        lv_obj_set_scrollbar_mode(scroll, LV_SCROLLBAR_MODE_AUTO);
        s_details_text = app_ui_theme_label(scroll, "", 0, 0, 572);
        lv_obj_set_style_pad_bottom(scroll, 12, 0);
        s_demo_button = app_ui_theme_button(panel, "PC演示", 20, 340, 140, 44);
        lv_obj_add_event_cb(s_demo_button, demo_event, LV_EVENT_CLICKED, NULL);
        lv_obj_t *more = app_ui_theme_button(panel, "更多设置", 176, 340, 140, 44);
        lv_obj_add_event_cb(more, command_event, LV_EVENT_CLICKED,
            (void *)(uintptr_t)APP_UI_WATCH_OPEN_MORE);
        lv_obj_t *done = app_ui_theme_button(panel, "完成", 476, 340, 144, 44);
        lv_obj_add_event_cb(done, details_close_event, LV_EVENT_CLICKED, NULL);
    }
    lv_obj_remove_flag(s_details, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_to_index(s_details, -1);
    lv_obj_scroll_to_y(lv_obj_get_parent(s_details_text), 0, LV_ANIM_OFF);
    update_details();
}

lv_obj_t *app_ui_watch_home_create(const app_ui_snapshot_t *snapshot,
                                   app_ui_action_cb_t action,
                                   app_ui_navigation_cb_t navigate,
                                   app_ui_watch_command_cb_t command)
{
    static const char *const navigation_names[5] = {"总控", "数据", "记录", "环境", "视觉"};
    static const app_ui_watch_icon_t navigation_icons[5] = {
        APP_UI_WATCH_ICON_HOME, APP_UI_WATCH_ICON_DATA, APP_UI_WATCH_ICON_HISTORY,
        APP_UI_WATCH_ICON_ALERT, APP_UI_WATCH_ICON_VISION
    };
    static const char *const control_names[6] = {"风速", "摆风", "睡眠", "节能", "强劲", "灯光"};
    static const app_ui_watch_icon_t control_icons[6] = {
        APP_UI_WATCH_ICON_FAN, APP_UI_WATCH_ICON_SWING, APP_UI_WATCH_ICON_MOON,
        APP_UI_WATCH_ICON_ECO, APP_UI_WATCH_ICON_TURBO, APP_UI_WATCH_ICON_LIGHT
    };
    static const app_climate_action_t control_actions[6] = {
        APP_ACTION_FAN_NEXT, APP_ACTION_SWING_LR, APP_ACTION_SLEEP_TOGGLE,
        APP_ACTION_ECO_TOGGLE, APP_ACTION_TURBO, APP_ACTION_LAMP
    };
    s_snapshot = snapshot;
    s_action = action;
    s_navigate = navigate;
    s_command = command;
    s_target_initialized = 0U;
    s_page = app_ui_theme_screen(false);
    lv_obj_set_style_bg_color(s_page, lv_color_hex(WATCH_BACKGROUND), 0);
    lv_obj_t *background = lv_image_create(s_page);
    lv_image_set_src(background, &app_ui_watch_background);
    lv_image_set_pivot(background, 0, 0);
    lv_image_set_scale(background, APP_UI_WATCH_BACKGROUND_SCALE);
    lv_image_set_inner_align(background, LV_IMAGE_ALIGN_TOP_LEFT);
    lv_obj_set_pos(background, 0, 0);
    passive(background);
    s_foreground = flat(s_page, 0, 0, 800, 480, WATCH_BACKGROUND, 0);
    lv_obj_set_style_bg_opa(s_foreground, LV_OPA_TRANSP, 0);
    label(s_foreground, "智能空调", 16, 18, 140, WATCH_TEXT);
    s_source = label(s_foreground, "软件请求 · 未接负载", 160, 18, 330, WATCH_MUTED);
    s_power = button(s_foreground, "电源", 508, 10, 128, 42);
    lv_obj_set_ext_click_area(s_power, 2);
    lv_obj_t *power_caption = lv_obj_get_child(s_power, 0);
    lv_obj_set_width(power_caption, 36);
    lv_obj_align(power_caption, LV_ALIGN_LEFT_MID, 43, 0);
    icon(s_power, APP_UI_WATCH_ICON_POWER, 7, 7, WATCH_ACCENT);
    s_power_status = label(s_power, "关闭", 87, 9, 38, WATCH_MUTED);
    lv_obj_add_event_cb(s_power, action_event, LV_EVENT_CLICKED, (void *)(uintptr_t)APP_ACTION_POWER);
    lv_obj_t *more = button(s_foreground, "更多", 648, 10, 60, 42);
    lv_obj_set_ext_click_area(more, 2);
    lv_obj_add_event_cb(more, command_event, LV_EVENT_CLICKED, (void *)(uintptr_t)APP_UI_WATCH_OPEN_MORE);

    lv_obj_t *nav = flat(s_foreground, 724, 12, 60, 456, WATCH_CARD, 18);
    for (unsigned int i = 0U; i < 5U; i++) {
        lv_obj_t *item = button(nav, navigation_names[i], 4, 32 + (int)i * 80, 52, 68);
        lv_obj_set_style_bg_opa(item, LV_OPA_TRANSP, 0);
        lv_obj_set_style_bg_opa(item, LV_OPA_COVER, LV_STATE_CHECKED);
        lv_obj_set_style_border_width(item, 0, 0);
        lv_obj_t *caption = lv_obj_get_child(item, 0);
        lv_obj_align(caption, LV_ALIGN_BOTTOM_MID, 0, -6);
        icon(item, navigation_icons[i], 12, 8, i == 0U ? WATCH_ACCENT : WATCH_MUTED);
        if (i == 2U || i == 3U) {
            lv_obj_add_event_cb(item, command_event, LV_EVENT_CLICKED,
                (void *)(uintptr_t)(i == 2U ? APP_UI_WATCH_OPEN_JOURNAL : APP_UI_WATCH_OPEN_ENVIRONMENT));
        } else {
            uint8_t page = i == 0U ? APP_UI_PAGE_HOME : i == 1U ? APP_UI_PAGE_DATA : APP_UI_PAGE_VISION;
            lv_obj_add_event_cb(item, navigation_event, LV_EVENT_CLICKED, (void *)(uintptr_t)page);
        }
        if (i == 0U) { app_ui_theme_checked(item, true); }
        if (i == 3U) { s_environment = item; }
    }

    lv_obj_t *temperature_control = flat(s_foreground, 16, 60, 372, 326, WATCH_CARD, 18);
    lv_obj_set_style_bg_opa(temperature_control, LV_OPA_60, 0);
    s_arc = lv_arc_create(temperature_control);
    lv_obj_remove_style_all(s_arc);
    lv_obj_set_pos(s_arc, 60, 12);
    lv_obj_set_size(s_arc, 252, 252);
    lv_arc_set_rotation(s_arc, 135);
    lv_arc_set_bg_angles(s_arc, 0, 270);
    lv_arc_set_range(s_arc, 0, 270);
    lv_arc_set_value(s_arc, 0);
    lv_obj_set_style_pad_all(s_arc, 2, 0);
    lv_obj_set_style_arc_color(s_arc, lv_color_hex(0x2A4354U), LV_PART_MAIN);
    lv_obj_set_style_arc_color(s_arc, lv_color_hex(WATCH_ACCENT), LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(s_arc, 5, LV_PART_MAIN);
    lv_obj_set_style_arc_width(s_arc, 5, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(s_arc, true, LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(s_arc, true, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(s_arc, LV_OPA_COVER, LV_PART_KNOB);
    lv_obj_set_style_bg_color(s_arc, lv_color_hex(WATCH_ACCENT), LV_PART_KNOB);
    lv_obj_set_style_radius(s_arc, LV_RADIUS_CIRCLE, LV_PART_KNOB);
    lv_obj_set_style_pad_all(s_arc, 5, LV_PART_KNOB);
    lv_obj_set_style_border_width(s_arc, 2, LV_PART_KNOB);
    lv_obj_set_style_border_color(s_arc, lv_color_hex(WATCH_CARD), LV_PART_KNOB);
    lv_obj_remove_flag(s_arc, LV_OBJ_FLAG_SCROLLABLE);
    icon(temperature_control, APP_UI_WATCH_ICON_SNOW, 172, 32, WATCH_ACCENT);
    lv_obj_t *caption = label(temperature_control, "设定温度", 90, 54, 192, WATCH_MUTED);
    lv_obj_set_style_text_align(caption, LV_TEXT_ALIGN_CENTER, 0);
    s_readout = flat(temperature_control, 108, WATCH_READOUT_Y, 156, 96, WATCH_CARD, 0);
    lv_obj_set_style_bg_opa(s_readout, LV_OPA_TRANSP, 0);
    s_target = label(s_readout, "--", 0, 0, 106, WATCH_TEXT);
    lv_obj_set_style_text_font(s_target, &app_font_watch_96, 0);
    s_target_unit = label(s_readout, "°C", 114, 55, 42, WATCH_MUTED);
    lv_obj_set_style_text_font(s_target_unit, &lv_font_montserrat_32, 0);
    caption = label(temperature_control, "16", 105, 195, 32, WATCH_MUTED);
    lv_obj_set_style_text_font(caption, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_align(caption, LV_TEXT_ALIGN_CENTER, 0);
    caption = label(temperature_control, "30", 235, 195, 32, WATCH_MUTED);
    lv_obj_set_style_text_font(caption, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_align(caption, LV_TEXT_ALIGN_CENTER, 0);
    caption = label(temperature_control, "16-30°C · 拖动调温", 102, 228, 168, WATCH_MUTED);
    lv_obj_set_style_text_align(caption, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_t *minus = button(temperature_control, "-", 20, 198, 64, 64);
    lv_obj_t *plus = button(temperature_control, "+", 288, 198, 64, 64);
    lv_obj_set_style_radius(minus, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_radius(plus, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(minus, lv_color_hex(0x223C4BU), 0);
    lv_obj_set_style_bg_color(plus, lv_color_hex(0x223C4BU), 0);
    lv_obj_set_style_text_font(lv_obj_get_child(minus, 0), &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_font(lv_obj_get_child(plus, 0), &lv_font_montserrat_48, 0);
    lv_obj_add_event_cb(minus, action_event, LV_EVENT_CLICKED, (void *)(uintptr_t)APP_ACTION_TEMP_DOWN);
    lv_obj_add_event_cb(plus, action_event, LV_EVENT_CLICKED, (void *)(uintptr_t)APP_ACTION_TEMP_UP);
    s_mode[0] = button(temperature_control, "智能自动", 16, 274, 164, 44);
    s_mode[1] = button(temperature_control, "手动制冷", 192, 274, 164, 44);
    for (unsigned int i = 0U; i < 2U; i++) {
        lv_obj_add_event_cb(s_mode[i], mode_event, LV_EVENT_CLICKED, (void *)(uintptr_t)i);
    }

    s_temperature_card = flat(s_foreground, WATCH_SENSOR_LEFT, WATCH_SENSOR_TOP,
        WATCH_SENSOR_WIDTH, WATCH_SENSOR_HEIGHT, WATCH_TEMPERATURE_CARD, 18);
    s_humidity_card = flat(s_foreground,
        WATCH_SENSOR_LEFT + WATCH_SENSOR_WIDTH + WATCH_SENSOR_GAP, WATCH_SENSOR_TOP,
        WATCH_SENSOR_WIDTH, WATCH_SENSOR_HEIGHT, WATCH_HUMIDITY_CARD, 18);
    s_temperature_caption = label(s_temperature_card, "实测温度", 16, 1, 116, WATCH_INK);
    s_humidity_caption = label(s_humidity_card, "实测湿度", 16, 1, 116, WATCH_INK);
    lv_obj_set_style_text_align(s_temperature_caption, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_align(s_humidity_caption, LV_TEXT_ALIGN_CENTER, 0);
    s_temperature = label(s_temperature_card, "--", 16, 25, 116, WATCH_INK);
    s_humidity = label(s_humidity_card, "--", 16, 25, 116, WATCH_INK);
    lv_obj_set_style_text_font(s_temperature, &lv_font_montserrat_40, 0);
    lv_obj_set_style_text_font(s_humidity, &lv_font_montserrat_40, 0);
    s_temperature_unit = label(s_temperature_card, "°C", 112, 43, 26, WATCH_INK);
    s_humidity_unit = label(s_humidity_card, "%", 112, 43, 26, WATCH_INK);
    lv_obj_set_style_text_font(s_temperature_unit, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_font(s_humidity_unit, &lv_font_montserrat_20, 0);
    layout_sensor_readout(s_temperature, s_temperature_unit);
    layout_sensor_readout(s_humidity, s_humidity_unit);

    lv_obj_t *environment = flat(s_foreground, 400, 142, 308, 162, WATCH_CARD, 18);
    lv_obj_set_style_bg_opa(environment, LV_OPA_80, 0);
    s_people = label(environment, "人员 未知 / 检测未更新", 16, 14, 174, WATCH_TEXT);
    s_light = label(environment, "光照 未知", 196, 14, 100, WATCH_MUTED);
    s_quality = label(environment, "等待首次采样", 16, 46, 276, WATCH_MUTED);
    s_strategy = label(environment, "策略原因：等待状态更新", 16, 78, 204, WATCH_MUTED);
    s_reminder = label(environment, "", 16, 112, 204, WATCH_CAUTION);
    lv_obj_t *details = button(environment, "详情", 232, 104, 60, 44);
    lv_obj_add_event_cb(details, command_event, LV_EVENT_CLICKED, (void *)(uintptr_t)APP_UI_WATCH_OPEN_DETAILS);
    for (unsigned int i = 0U; i < 6U; i++) {
        s_bottom[i] = button(s_foreground, control_names[i], 16 + (int)i * 117, 398, 107, 66);
        lv_obj_add_event_cb(s_bottom[i], action_event, LV_EVENT_CLICKED,
            (void *)(uintptr_t)control_actions[i]);
        lv_obj_t *text = lv_obj_get_child(s_bottom[i], 0);
        lv_obj_set_width(text, 55);
        lv_obj_align(text, LV_ALIGN_TOP_LEFT, 50, 10);
        s_bottom_icons[i] = icon(s_bottom[i], control_icons[i], 12, 10, WATCH_MUTED);
        s_bottom_values[i] = label(s_bottom[i], "", 12, 38, 83, WATCH_MUTED);
    }
    app_ui_temperature_input_init(s_arc, s_action, preview_target);
    app_ui_temperature_input_observe_target(s_snapshot->climate.target_temperature);
    return s_page;
}

void app_ui_watch_home_update(const char *reason, const char *reminder,
                              uint8_t attention)
{
    if (!s_snapshot || !s_page) { return; }
    const app_climate_state_t *state = &s_snapshot->climate;
    char text[256];
    app_ui_theme_checked(s_power, state->power_on != 0U);
    app_ui_theme_text(s_power_status, state->power_on ? "开启" : "关闭");
    text_color(s_power_status, state->power_on ? WATCH_ACCENT : WATCH_MUTED);
    app_ui_theme_text(s_source, s_snapshot->simulated ?
        "PC模拟 · 软件请求 · 未接负载" : "软件请求 · 未接负载");
    update_target(app_ui_temperature_input_target(state->target_temperature));
    app_ui_theme_checked(s_mode[0], state->mode == APP_CLIMATE_MODE_AUTO);
    app_ui_theme_checked(s_mode[1], state->mode == APP_CLIMATE_MODE_MANUAL_COOL);
    uint8_t cached = !s_snapshot->simulated &&
        s_snapshot->temperature_source == APP_TEMPERATURE_DHT11 && s_snapshot->dht.has_sample;
    if ((state->sensor_fresh || cached) && isfinite(s_snapshot->temperature)) {
        (void)snprintf(text, sizeof(text), "%.1f", (double)s_snapshot->temperature);
    } else { (void)snprintf(text, sizeof(text), "--"); }
    app_ui_theme_text(s_temperature, text);
    if ((state->humidity_fresh || cached) && isfinite(s_snapshot->humidity) &&
        s_snapshot->humidity >= 0.0f && s_snapshot->humidity <= 100.0f) {
        (void)snprintf(text, sizeof(text), "%.0f", (double)s_snapshot->humidity);
    } else { (void)snprintf(text, sizeof(text), "--"); }
    app_ui_theme_text(s_humidity, text);
    layout_sensor_readout(s_temperature, s_temperature_unit);
    layout_sensor_readout(s_humidity, s_humidity_unit);
    text_color(s_temperature, state->sensor_fresh ? WATCH_INK : WATCH_SENSOR_MUTED);
    text_color(s_humidity, state->humidity_fresh ? WATCH_INK : WATCH_SENSOR_MUTED);
    app_ui_theme_text(s_temperature_caption, cached && !state->sensor_fresh ? "上次温度" : "实测温度");
    app_ui_theme_text(s_humidity_caption, cached && !state->humidity_fresh ? "上次湿度" : "实测湿度");
    lv_color_t temperature_color = lv_color_hex(state->sensor_fresh ? WATCH_TEMPERATURE_CARD : WATCH_CARD_UNKNOWN);
    lv_color_t humidity_color = lv_color_hex(state->humidity_fresh ? WATCH_HUMIDITY_CARD : WATCH_CARD_UNKNOWN);
    if (!lv_color_eq(lv_obj_get_style_bg_color(s_temperature_card, 0), temperature_color)) {
        lv_obj_set_style_bg_color(s_temperature_card, temperature_color, 0);
    }
    if (!lv_color_eq(lv_obj_get_style_bg_color(s_humidity_card, 0), humidity_color)) {
        lv_obj_set_style_bg_color(s_humidity_card, humidity_color, 0);
    }
    if (state->people_fresh && s_snapshot->people_valid && s_snapshot->people_count >= 0) {
        (void)snprintf(text, sizeof(text), "人员 %ld 人 / %s", (long)s_snapshot->people_count,
            state->people_confirming ? "确认中" : presence_name(state->presence));
    } else { (void)snprintf(text, sizeof(text), "人员 未知 / 检测未更新"); }
    app_ui_theme_text(s_people, text);
    text_color(s_people, state->people_fresh ? WATCH_TEXT : WATCH_CAUTION);
    (void)snprintf(text, sizeof(text), "光照 %s", !state->als_fresh ? "未知" :
        state->light == APP_LIGHT_BRIGHT ? "明亮" : "暗");
    app_ui_theme_text(s_light, text);
    if (!s_snapshot->simulated && s_snapshot->temperature_source == APP_TEMPERATURE_DHT11) {
        if (!s_snapshot->dht.has_sample) {
            if (s_snapshot->dht.attempts) {
                (void)snprintf(text, sizeof(text), "首次采样失败 E%lu", (unsigned long)s_snapshot->dht.last_status);
            } else { (void)snprintf(text, sizeof(text), "等待首次采样"); }
        } else if (state->sensor_fresh && state->humidity_fresh) {
            (void)snprintf(text, sizeof(text), "DHT11 正常 / %lu s前",
                (unsigned long)((app_ui_tick_ms() - s_snapshot->dht.last_success_ms) / 1000U));
        } else if (!s_snapshot->dht.last_read_ok) {
            (void)snprintf(text, sizeof(text), "采样失败 E%lu / 上次 %lu s前",
                (unsigned long)s_snapshot->dht.last_status,
                (unsigned long)((app_ui_tick_ms() - s_snapshot->dht.last_success_ms) / 1000U));
        } else {
            (void)snprintf(text, sizeof(text), "%s / 上次 %lu s前",
                state->temperature_quality == APP_DATA_INVALID || state->humidity_quality == APP_DATA_INVALID ?
                "数值无效" : "数据过期",
                (unsigned long)((app_ui_tick_ms() - s_snapshot->dht.last_success_ms) / 1000U));
        }
    } else {
        (void)snprintf(text, sizeof(text), "温度%s / 湿度%s",
            quality_short_name(state->temperature_quality), quality_short_name(state->humidity_quality));
    }
    app_ui_theme_text(s_quality, text);
    text_color(s_quality, state->sensor_fresh && state->humidity_fresh ? WATCH_MUTED : WATCH_CAUTION);
    (void)snprintf(s_reason, sizeof(s_reason), "%s", reason ? reason : "等待状态更新");
    (void)snprintf(text, sizeof(text), "策略原因：%s", s_reason);
    app_ui_theme_text(s_strategy, text);
    text_color(s_strategy, state->reason == APP_REASON_COOLING ? WATCH_ACCENT : WATCH_MUTED);
    app_ui_theme_text(s_reminder, reminder);
    app_ui_theme_checked(s_environment, attention != 0U);
    uint8_t mask = app_climate_output_mask(state);
    uint8_t selected[6] = {
        (mask & 0x02U) != 0U, state->swing_lr || state->swing_ud,
        state->manual_sleep, state->manual_energy_saving, state->turbo, state->lamp_on
    };
    for (unsigned int i = 0U; i < 6U; i++) {
        app_ui_theme_checked(s_bottom[i], selected[i] != 0U);
        lv_color_t color = lv_color_hex(selected[i] ? WATCH_ACCENT : WATCH_MUTED);
        if (!lv_color_eq(lv_obj_get_style_image_recolor(s_bottom_icons[i], 0), color)) {
            lv_obj_set_style_image_recolor(s_bottom_icons[i], color, 0);
        }
    }
    (void)snprintf(text, sizeof(text), "%u / %u档", (unsigned int)(state->turbo ? 5U : state->fan_speed),
        (unsigned int)((mask & 0x02U) ? state->effective_fan_speed : 0U));
    app_ui_theme_text(s_bottom_values[0], text);
    app_ui_theme_text(s_bottom_values[1], state->swing_lr && state->swing_ud ? "双向" :
        state->swing_lr ? "左右" : state->swing_ud ? "上下" : "关闭");
    app_ui_theme_text(s_bottom_values[2], state->manual_sleep ? "手动开启" :
        state->effective_sleep ? "自动开启" : "关闭");
    app_ui_theme_text(s_bottom_values[3], state->manual_energy_saving ? "手动开启" :
        state->effective_eco ? "自动开启" : "关闭");
    app_ui_theme_text(s_bottom_values[4], state->turbo ?
        (state->effective_fan_speed < 5U && state->power_on ? "已限档" : "已选择") : "关闭");
    app_ui_theme_text(s_bottom_values[5], state->lamp_on ? "已选择" : "关闭");
    update_details();
}
