#include "app_ui_insights.h"
#include "app_ui_theme.h"
#include "app_history.h"
#include "app_climate_logic.h"
#include "lvgl.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static app_history_t s_history;
static app_ui_navigation_cb_t s_navigate;
static app_ui_demo_cb_t s_demo;
static lv_obj_t *s_page;
static lv_obj_t *s_data_body;
static lv_obj_t *s_demo_body;
static lv_obj_t *s_title;
static lv_obj_t *s_source_label;
static lv_obj_t *s_navigation[4];
static lv_obj_t *s_summary;
static lv_obj_t *s_summary_card;
static lv_obj_t *s_event_label;
static lv_obj_t *s_event_card;
static lv_obj_t *s_stats_label;
static lv_obj_t *s_chart[4];
static lv_obj_t *s_chart_card[4];
static lv_obj_t *s_chart_title[4];
static lv_chart_cursor_t *s_cursor[4];
static lv_chart_series_t *s_series[4];
static lv_chart_series_t *s_target_series;
static int32_t s_values[5][APP_HISTORY_SAMPLE_CAPACITY];
static uint32_t s_chart_revision;
static uint8_t s_chart_current;
static uint8_t s_control_view;
static lv_obj_t *s_view_button;
static lv_obj_t *s_view_label;
static lv_obj_t *s_phase_label;
static lv_obj_t *s_demo_reason;
static lv_obj_t *s_demo_bar;
static lv_obj_t *s_start_button;
static lv_obj_t *s_start_label;
static lv_obj_t *s_stop_button;
static uint8_t s_page_kind;
static char s_events_text[768];
static lv_obj_t *s_detail_back;
static lv_obj_t *s_detail_window;
static lv_obj_t *s_detail_time;
static lv_obj_t *s_detail_value;
static lv_obj_t *s_detail_hint;
static lv_obj_t *s_detail_axis_top;
static lv_obj_t *s_detail_axis_bottom;
static uint8_t s_detail_active;
static uint8_t s_detail_index;
static uint8_t s_detail_follow_latest;
static uint8_t s_detail_have_selected;
static uint8_t s_detail_reset_note;
static uint64_t s_detail_selected_ms;
static uint32_t s_detail_session;

static void configure_chart_view(void);
static void update_history(void);
static void close_detail(void);
static void chart_detail_event(lv_event_t *event);
static void update_detail(const app_history_stats_t *stats);

static lv_obj_t *label(lv_obj_t *parent, const char *text, int x, int y, int width)
{
    return app_ui_theme_label(parent, text, x, y, width);
}

static void text_changed(lv_obj_t *object, const char *text)
{
    app_ui_theme_text(object, text);
}

static lv_obj_t *card(lv_obj_t *parent, int x, int y, int width, int height)
{
    return app_ui_theme_card(parent, x, y, width, height);
}

static lv_obj_t *button(lv_obj_t *parent, const char *text, int x, int y, int width, int height)
{
    return app_ui_theme_button(parent, text, x, y, width, height);
}

static void navigation_event(lv_event_t *event)
{
    if (lv_event_get_code(event) == LV_EVENT_CLICKED && s_navigate) {
        close_detail();
        s_navigate((uint8_t)(uintptr_t)lv_event_get_user_data(event));
    }
}

static void demo_event(lv_event_t *event)
{
    if (lv_event_get_code(event) == LV_EVENT_CLICKED && s_demo) {
        s_demo((uint8_t)(uintptr_t)lv_event_get_user_data(event));
    }
}

void app_ui_insights_init(app_ui_navigation_cb_t navigate, app_ui_demo_cb_t demo)
{
    s_navigate = navigate;
    s_demo = demo;
    s_chart_current = 0U;
    s_control_view = 0U;
    s_detail_active = 0U;
    app_history_init(&s_history);
}

void app_ui_insights_observe(const app_ui_snapshot_t *snapshot, uint32_t now_ms)
{
    app_history_input_t input = {0};
    uint8_t source = snapshot->simulated ? (snapshot->demo_active ? 2U : 1U) : 0U;
    input.source = source;
    input.reason = (uint8_t)snapshot->climate.reason;
    input.presence = (uint8_t)snapshot->climate.presence;
    input.request_mask = app_climate_output_mask(&snapshot->climate);
    input.target_x10 = (int16_t)snapshot->climate.effective_target_temperature * 10;
    input.valid_flags = APP_HISTORY_VALID_REQUEST | APP_HISTORY_VALID_REASON | APP_HISTORY_VALID_PRESENCE;
    if (snapshot->climate.effective_target_temperature >= 16U && snapshot->climate.effective_target_temperature <= 30U) {
        input.valid_flags |= APP_HISTORY_VALID_TARGET;
    }
    if (snapshot->climate.sensor_fresh && isfinite(snapshot->temperature) &&
        snapshot->temperature >= -3276.0f && snapshot->temperature <= 3276.0f) {
        input.temperature_x10 = (int16_t)lroundf(snapshot->temperature * 10.0f);
        input.valid_flags |= APP_HISTORY_VALID_TEMPERATURE;
    }
    if (snapshot->climate.humidity_fresh && isfinite(snapshot->humidity) &&
        snapshot->humidity >= 0.0f && snapshot->humidity <= 100.0f) {
        input.humidity_x10 = (uint16_t)lroundf(snapshot->humidity * 10.0f);
        input.valid_flags |= APP_HISTORY_VALID_HUMIDITY;
    }
    if (snapshot->climate.als_fresh) {
        input.als = snapshot->als;
        input.valid_flags |= APP_HISTORY_VALID_ALS;
    }
    if (snapshot->climate.people_fresh && snapshot->people_count >= 0 && snapshot->people_count <= UINT16_MAX) {
        input.people = (uint16_t)snapshot->people_count;
        input.valid_flags |= APP_HISTORY_VALID_PEOPLE;
    }
    input.quality = (uint16_t)((unsigned int)snapshot->climate.temperature_quality |
        ((unsigned int)snapshot->climate.humidity_quality << 2U) |
        ((unsigned int)snapshot->climate.people_fresh << 4U) |
        ((unsigned int)snapshot->climate.als_fresh << 5U));
    app_history_observe(&s_history, &input, now_ms);
}

void app_ui_insights_sensing_text(const app_ui_snapshot_t *snapshot,
                                  char *text, size_t capacity)
{
    if (text == NULL || capacity == 0U) { return; }
    if (snapshot == NULL) {
        (void)snprintf(text, capacity, "温差 未知 | 检测 未知");
        return;
    }
    char temperature[64];
    if (snapshot->climate.sensor_fresh && isfinite(snapshot->temperature) &&
        snapshot->climate.effective_target_temperature >= 16U &&
        snapshot->climate.effective_target_temperature <= 30U) {
        (void)snprintf(temperature, sizeof(temperature), "温差 %+.1f C",
            (double)snapshot->temperature - snapshot->climate.effective_target_temperature);
    } else {
        (void)snprintf(temperature, sizeof(temperature), "温差 未知");
    }
    if (snapshot->climate.people_fresh && snapshot->people_valid &&
        snapshot->people_count >= 0 && !snapshot->ai.recovery_active) {
        (void)snprintf(text, capacity, "%s | 检测 %lu ms前", temperature,
            (unsigned long)(app_ui_tick_ms() - snapshot->people_updated_ms));
    } else {
        (void)snprintf(text, capacity, "%s | 检测 未知", temperature);
    }
}

static void make_chart(unsigned int index, const char *name, int x, int y,
                       int32_t minimum, int32_t maximum, uint32_t color)
{
    lv_obj_t *parent = card(s_data_body, x, y, 238, 118);
    s_chart_card[index] = parent;
    /* Only chart cards navigate to details; decorative cards stay passive. */
    lv_obj_add_flag(parent, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(parent, chart_detail_event, LV_EVENT_CLICKED, (void *)(uintptr_t)index);
    s_chart_title[index] = label(parent, name, 12, 6, 216);
    s_chart[index] = lv_chart_create(parent);
    lv_obj_set_pos(s_chart[index], 12, 34);
    lv_obj_set_size(s_chart[index], 214, 74);
    lv_obj_set_style_bg_color(s_chart[index], lv_color_hex(APP_UI_COLOR_CARD), 0);
    lv_obj_set_style_border_width(s_chart[index], 0, 0);
    lv_obj_set_style_line_color(s_chart[index], lv_color_hex(APP_UI_COLOR_BORDER), 0);
    lv_obj_set_style_pad_all(s_chart[index], 0, 0);
    /* The default theme's 10 px column gap cannot fit 60 samples in 214 px:
     * it gives BAR a negative block width and clips the newest samples. */
    lv_obj_set_style_pad_column(s_chart[index], 0, LV_PART_MAIN);
    lv_obj_set_style_pad_column(s_chart[index], 0, LV_PART_ITEMS);
    lv_obj_set_style_radius(s_chart[index], 0, LV_PART_ITEMS);
    lv_obj_set_style_line_width(s_chart[index], 2, LV_PART_ITEMS);
    lv_obj_set_style_size(s_chart[index], 0, 0, LV_PART_INDICATOR);
    lv_obj_remove_flag(s_chart[index], LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_chart[index], LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_chart[index], chart_detail_event, LV_EVENT_ALL, (void *)(uintptr_t)index);
    lv_obj_set_style_line_width(s_chart[index], 1, LV_PART_CURSOR);
    lv_obj_set_style_size(s_chart[index], 0, 0, LV_PART_CURSOR);
    lv_chart_set_point_count(s_chart[index], APP_HISTORY_SAMPLE_CAPACITY);
    lv_chart_set_range(s_chart[index], LV_CHART_AXIS_PRIMARY_Y, minimum, maximum);
    lv_chart_set_div_line_count(s_chart[index], 3, 4);
    s_series[index] = lv_chart_add_series(s_chart[index], lv_color_hex(color), LV_CHART_AXIS_PRIMARY_Y);
    lv_chart_set_ext_y_array(s_chart[index], s_series[index], s_values[index]);
    if (index == 0U) {
        s_target_series = lv_chart_add_series(s_chart[index], lv_color_hex(APP_UI_COLOR_CAUTION), LV_CHART_AXIS_PRIMARY_Y);
        lv_chart_set_ext_y_array(s_chart[index], s_target_series, s_values[4]);
    }
    s_cursor[index] = lv_chart_add_cursor(s_chart[index], lv_color_hex(APP_UI_COLOR_TEXT), LV_DIR_VER);
    if (s_cursor[index]) {
        lv_chart_set_cursor_point(s_chart[index], s_cursor[index], s_series[index], LV_CHART_POINT_NONE);
    }
}

static lv_obj_t *body_create(void)
{
    lv_obj_t *body = lv_obj_create(s_page);
    lv_obj_remove_style_all(body);
    lv_obj_set_pos(body, 0, 54);
    lv_obj_set_size(body, 800, 358);
    lv_obj_remove_flag(body, LV_OBJ_FLAG_SCROLLABLE);
    return body;
}

static void make_data_body(void)
{
    s_data_body = body_create();
    s_summary_card = card(s_data_body, 16, 0, 768, 66);
    s_summary = label(s_summary_card, "感知状态与控制原因", 12, 8, 744);
    make_chart(0U, "温度 C 10..40 / 目标", 16, 76, 100, 400, 0x64DFC0U);
    make_chart(1U, "湿度 % 0..100", 268, 76, 0, 1000, 0x73BFF4U);
    make_chart(2U, "光照原始值 0..1000", 16, 206, 0, 1000, 0xFFC46BU);
    make_chart(3U, "人数 0..10", 268, 206, 0, 10, 0x64DFC0U);
    s_event_card = card(s_data_body, 522, 76, 262, 248);
    label(s_event_card, "最近控制事件 / 软件请求", 12, 8, 238);
    s_event_label = label(s_event_card, "等待事件", 12, 40, 238);
    lv_label_set_long_mode(s_event_label, LV_LABEL_LONG_CLIP);
    lv_obj_set_height(s_event_label, 196);
    s_stats_label = label(s_data_body, "每 5 s 采样 / 最多 60 点 / 空缺表示无效", 16, 332, 768);
    s_chart_current = 0U;
    configure_chart_view();
}

static void configure_chart_view(void)
{
    static const char *const environment_titles[4] = {
        "温度 C 10..40 / 目标", "湿度 % 0..100", "光照原始值 0..1000", "人数 0..10"
    };
    static const char *const control_titles[4] = {
        "R1 制冷请求 / 0..1", "R2 风机请求 / 0..1", "R3 摆风请求 / 0..1", "R4 灯光请求 / 0..1"
    };
    static const int32_t minimum[4] = {100, 0, 0, 0};
    static const int32_t maximum[4] = {400, 1000, 1000, 10};
    if (s_data_body) {
        for (unsigned int i = 0U; i < 4U; i++) {
            text_changed(s_chart_title[i], s_control_view ? control_titles[i] : environment_titles[i]);
            lv_chart_set_type(s_chart[i], s_control_view ? LV_CHART_TYPE_BAR : LV_CHART_TYPE_LINE);
            lv_chart_set_range(s_chart[i], LV_CHART_AXIS_PRIMARY_Y,
                s_control_view ? 0 : minimum[i], s_control_view ? 1 : maximum[i]);
            lv_chart_set_div_line_count(s_chart[i], s_control_view ? 2 : 3, 4);
        }
        lv_chart_hide_series(s_chart[0], s_target_series, s_control_view != 0U);
    }
    if (s_view_label) { text_changed(s_view_label, s_control_view ? "环境趋势" : "控制请求"); }
    if (s_title && s_page_kind == APP_UI_PAGE_DATA) {
        text_changed(s_title, s_control_view ? "控制请求" : "环境趋势");
    }
    /* A view change never resets history or touches the controller. */
    s_chart_current = 0U;
}

static void chart_view_event(lv_event_t *event)
{
    if (lv_event_get_code(event) == LV_EVENT_CLICKED && s_page_kind == APP_UI_PAGE_DATA && !s_detail_active) {
        s_control_view ^= 1U;
        configure_chart_view();
        update_history();
    }
}

static void make_demo_body(void)
{
    s_demo_body = body_create();
    lv_obj_t *plan = card(s_demo_body, 16, 0, 768, 244);
    label(plan, "可重复场景 / 复用现有规则控制", 16, 10, 720);
    label(plan, "1 人员进入\n2 温度升高\n3 夜间睡眠\n4 无人节能与关机\n5 AI 故障\n6 数据恢复", 16, 52, 276);
    s_phase_label = label(plan, "演示尚未开始", 324, 52, 420);
    s_demo_reason = label(plan, "策略原因", 324, 112, 420);
    s_demo_bar = lv_bar_create(plan);
    lv_obj_set_pos(s_demo_bar, 324, 210);
    lv_obj_set_size(s_demo_bar, 420, 10);
    lv_bar_set_range(s_demo_bar, 0, 8);
    lv_obj_set_style_bg_color(s_demo_bar, lv_color_hex(APP_UI_COLOR_BORDER), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_demo_bar, lv_color_hex(APP_UI_COLOR_ACCENT), LV_PART_INDICATOR);
    s_start_button = button(s_demo_body, "开始 / 暂停", 20, 274, 210, 54);
    s_start_label = lv_obj_get_child(s_start_button, 0);
    lv_obj_add_event_cb(s_start_button, demo_event, LV_EVENT_CLICKED, (void *)(uintptr_t)APP_UI_DEMO_TOGGLE);
    s_stop_button = button(s_demo_body, "停止并恢复", 242, 274, 160, 54);
    lv_obj_add_event_cb(s_stop_button, demo_event, LV_EVENT_CLICKED, (void *)(uintptr_t)APP_UI_DEMO_STOP);
    label(s_demo_body, "PC：V 开始/暂停，X 停止；实板保留真实传感输入", 20, 338, 764);
}

void app_ui_insights_show(uint8_t page)
{
    if (!s_page) {
        s_page = app_ui_theme_screen(false);
        s_title = label(s_page, "环境趋势", 18, 14, 274);
        s_view_button = button(s_page, "控制请求", 300, 6, 116, 44);
        s_view_label = lv_obj_get_child(s_view_button, 0);
        lv_obj_add_event_cb(s_view_button, chart_view_event, LV_EVENT_CLICKED, NULL);
        s_source_label = label(s_page, "", 452, 14, 330);
        lv_obj_set_style_text_align(s_source_label, LV_TEXT_ALIGN_RIGHT, 0);
        static const char *const names[4] = {"主页", "视觉", "数据", "演示"};
        for (unsigned int i = 0U; i < 4U; i++) {
            s_navigation[i] = button(s_page, names[i], 16 + (int)i * 196, 422, 170, 48);
            lv_obj_add_event_cb(s_navigation[i], navigation_event, LV_EVENT_CLICKED, (void *)(uintptr_t)i);
        }
    }
    s_page_kind = page == APP_UI_PAGE_DEMO ? APP_UI_PAGE_DEMO : APP_UI_PAGE_DATA;
    for (unsigned int i = 0U; i < 4U; i++) {
        app_ui_theme_checked(s_navigation[i], i == s_page_kind);
    }
    if (s_page_kind != APP_UI_PAGE_DATA) { close_detail(); }
    if (s_page_kind == APP_UI_PAGE_DATA && !s_data_body) { make_data_body(); }
    if (s_page_kind == APP_UI_PAGE_DEMO && !s_demo_body) { make_demo_body(); }
    if (s_data_body) {
        if (s_page_kind == APP_UI_PAGE_DATA) { lv_obj_remove_flag(s_data_body, LV_OBJ_FLAG_HIDDEN); }
        else { lv_obj_add_flag(s_data_body, LV_OBJ_FLAG_HIDDEN); }
    }
    if (s_demo_body) {
        if (s_page_kind == APP_UI_PAGE_DEMO) { lv_obj_remove_flag(s_demo_body, LV_OBJ_FLAG_HIDDEN); }
        else { lv_obj_add_flag(s_demo_body, LV_OBJ_FLAG_HIDDEN); }
    }
    if (s_page_kind == APP_UI_PAGE_DATA) {
        if (!s_detail_active) { lv_obj_remove_flag(s_view_button, LV_OBJ_FLAG_HIDDEN); }
        text_changed(s_title, s_control_view ? "控制请求" : "环境趋势");
    } else {
        lv_obj_add_flag(s_view_button, LV_OBJ_FLAG_HIDDEN);
        text_changed(s_title, "场景演示");
    }
    lv_screen_load(s_page);
}

uint8_t app_ui_insights_active(void)
{
    return s_page && lv_screen_active() == s_page;
}

static const char *short_reason(uint8_t reason)
{
    switch (reason) {
    case APP_REASON_POWER_OFF: return "电源关闭";
    case APP_REASON_AI_UNKNOWN: return "人员未知";
    case APP_REASON_CONFIRMING: return "确认中";
    case APP_REASON_SENSOR_INVALID: return "温度失效";
    case APP_REASON_TEMP_SATISFIED: return "已达温度";
    case APP_REASON_COOLING_WAIT: return "保护等待";
    case APP_REASON_COOLING: return "制冷请求";
    case APP_REASON_ABSENCE_OFF: return "无人关机";
    default: return "未知";
    }
}

static void detail_widgets_visible(uint8_t visible)
{
    lv_obj_t *objects[] = {s_detail_back, s_detail_window, s_detail_time, s_detail_value,
        s_detail_hint, s_detail_axis_top, s_detail_axis_bottom};
    for (unsigned int i = 0U; i < sizeof(objects) / sizeof(objects[0]); i++) {
        if (!objects[i]) { continue; }
        if (visible) { lv_obj_remove_flag(objects[i], LV_OBJ_FLAG_HIDDEN); }
        else { lv_obj_add_flag(objects[i], LV_OBJ_FLAG_HIDDEN); }
    }
}

static void detail_back_event(lv_event_t *event)
{
    if (lv_event_get_code(event) == LV_EVENT_CLICKED) {
        close_detail();
        update_history();
    }
}

static void make_detail_widgets(void)
{
    if (s_detail_back) { return; }
    s_detail_back = button(s_data_body, "返回曲线", 640, 6, 132, 44);
    lv_obj_add_event_cb(s_detail_back, detail_back_event, LV_EVENT_CLICKED, NULL);
    s_detail_window = label(s_data_body, "窗口 等待真实采样", 28, 38, 580);
    lv_obj_set_height(s_detail_window, 40);
    s_detail_time = label(s_data_body, "实际采样 未知", 74, 250, 694);
    s_detail_value = label(s_data_body, "选中 未知", 28, 274, 744);
    lv_obj_set_height(s_detail_value, 42);
    s_detail_hint = label(s_data_body, "拖动选点 | 跟随最新 | 横轴为采样顺序", 16, 332, 768);
    s_detail_axis_top = label(s_data_body, "", 18, 82, 52);
    s_detail_axis_bottom = label(s_data_body, "", 18, 226, 52);
    lv_obj_set_style_text_align(s_detail_axis_top, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_style_text_align(s_detail_axis_bottom, LV_TEXT_ALIGN_RIGHT, 0);
    detail_widgets_visible(0U);
}

static void close_detail(void)
{
    static const int x[4] = {16, 268, 16, 268};
    static const int y[4] = {76, 76, 206, 206};
    if (!s_detail_active) { return; }
    s_detail_active = 0U;
    detail_widgets_visible(0U);
    lv_obj_remove_flag(s_summary_card, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(s_event_card, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(s_stats_label, LV_OBJ_FLAG_HIDDEN);
    if (s_page_kind == APP_UI_PAGE_DATA) { lv_obj_remove_flag(s_view_button, LV_OBJ_FLAG_HIDDEN); }
    for (unsigned int i = 0U; i < 4U; i++) {
        lv_obj_remove_flag(s_chart_card[i], LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_pos(s_chart_card[i], x[i], y[i]);
        lv_obj_set_size(s_chart_card[i], 238, 118);
        lv_obj_set_width(s_chart_title[i], 216);
        lv_obj_set_pos(s_chart[i], 12, 34);
        lv_obj_set_size(s_chart[i], 214, 74);
        lv_obj_set_style_size(s_chart[i], 0, 0, LV_PART_INDICATOR);
        lv_chart_set_point_count(s_chart[i], APP_HISTORY_SAMPLE_CAPACITY);
        if (s_cursor[i]) {
            lv_chart_set_cursor_point(s_chart[i], s_cursor[i], s_series[i], LV_CHART_POINT_NONE);
        }
    }
    configure_chart_view();
}

static void open_detail(unsigned int index)
{
    static const char *const environment_titles[4] = {
        "温度趋势 / C / 黄色为执行目标", "湿度趋势 / %", "光照趋势 / 原始值", "人数趋势 / 人"
    };
    static const char *const request_titles[4] = {
        "R1 制冷请求 / 0关 1请求", "R2 风机请求 / 0关 1请求",
        "R3 摆风请求 / 0关 1请求", "R4 灯光请求 / 0关 1请求"
    };
    if (index >= 4U || s_detail_active || s_page_kind != APP_UI_PAGE_DATA) { return; }
    make_detail_widgets();
    app_history_stats_t stats;
    app_history_get_stats(&s_history, &stats);
    s_detail_active = 1U;
    s_detail_index = (uint8_t)index;
    s_detail_follow_latest = 1U;
    s_detail_have_selected = 0U;
    s_detail_reset_note = 0U;
    s_detail_session = stats.session_id;
    lv_obj_add_flag(s_summary_card, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_event_card, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_stats_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_view_button, LV_OBJ_FLAG_HIDDEN);
    for (unsigned int i = 0U; i < 4U; i++) {
        if (i != index) { lv_obj_add_flag(s_chart_card[i], LV_OBJ_FLAG_HIDDEN); }
    }
    lv_obj_set_pos(s_chart_card[index], 16, 0);
    lv_obj_set_size(s_chart_card[index], 768, 324);
    lv_obj_set_width(s_chart_title[index], 560);
    text_changed(s_chart_title[index], s_control_view ? request_titles[index] : environment_titles[index]);
    lv_obj_set_pos(s_chart[index], 58, 86);
    lv_obj_set_size(s_chart[index], 694, 158);
    lv_obj_set_style_size(s_chart[index], s_control_view ? 0 : 5, s_control_view ? 0 : 5, LV_PART_INDICATOR);
    detail_widgets_visible(1U);
    lv_obj_update_layout(s_data_body);
    s_chart_current = 0U;
    update_history();
}

/* Cursor and hit testing share the same slot geometry. Invalid samples still
 * have a slot and a real timestamp, but never use POINT_NONE as a Y value. */
static int32_t detail_point_x(unsigned int index)
{
    lv_obj_t *chart = s_chart[s_detail_index];
    int32_t width = lv_obj_get_content_width(chart);
    uint32_t count = lv_chart_get_point_count(chart);
    int32_t x = 0;
    if (count < 2U || width < 1) { return 0; }
    if (s_control_view) {
        int32_t block_width = width / (int32_t)count;
        int32_t column_width = block_width / (s_detail_index == 0U ? 2 : 1);
        if (column_width < 1) { column_width = 1; }
        x = (width - block_width) * (int32_t)index / (int32_t)(count - 1U) + column_width / 2;
    } else {
        x = width * (int32_t)index / (int32_t)(count - 1U);
    }
    return x < width ? x : width - 1;
}

static void select_detail_point(lv_event_t *event)
{
    lv_indev_t *indev = lv_event_get_indev(event);
    if (!indev) { indev = lv_indev_active(); }
    if (!indev) { return; }
    /* A snapshot may have appended a sample since the slower label update. */
    update_history();
    app_history_stats_t stats;
    app_history_get_stats(&s_history, &stats);
    if (stats.sample_count == 0U) { return; }
    lv_point_t point;
    lv_area_t area;
    lv_indev_get_point(indev, &point);
    lv_obj_get_coords(s_chart[s_detail_index], &area);
    int32_t x = point.x - area.x1;
    unsigned int selected = 0U;
    int32_t best = INT32_MAX;
    for (unsigned int i = 0U; i < stats.sample_count; i++) {
        int32_t distance = x - detail_point_x(i);
        if (distance < 0) { distance = -distance; }
        if (distance < best) { best = distance; selected = i; }
    }
    const app_history_sample_t *sample = app_history_sample_at(&s_history, (uint16_t)selected);
    if (!sample) { return; }
    s_detail_selected_ms = sample->elapsed_ms;
    s_detail_session = stats.session_id;
    s_detail_have_selected = 1U;
    s_detail_follow_latest = 0U;
    s_detail_reset_note = 0U;
    update_detail(&stats);
}

static void chart_detail_event(lv_event_t *event)
{
    unsigned int index = (unsigned int)(uintptr_t)lv_event_get_user_data(event);
    lv_event_code_t code = lv_event_get_code(event);
    if (!s_detail_active) {
        if (code == LV_EVENT_CLICKED) { open_detail(index); }
    } else if (index == s_detail_index &&
               (code == LV_EVENT_PRESSED || code == LV_EVENT_PRESSING)) {
        select_detail_point(event);
    }
}

static uint8_t history_value(const app_history_input_t *data, unsigned int index, int32_t *value)
{
    if (s_control_view) {
        if (!(data->valid_flags & APP_HISTORY_VALID_REQUEST)) { return 0U; }
        *value = (data->request_mask >> index) & 1U;
        return 1U;
    }
    static const uint8_t flags[4] = {APP_HISTORY_VALID_TEMPERATURE, APP_HISTORY_VALID_HUMIDITY,
        APP_HISTORY_VALID_ALS, APP_HISTORY_VALID_PEOPLE};
    if (!(data->valid_flags & flags[index])) { return 0U; }
    switch (index) {
    case 0U: *value = data->temperature_x10; break;
    case 1U: *value = data->humidity_x10; break;
    case 2U: *value = data->als; break;
    default: *value = data->people; break;
    }
    return 1U;
}

static void format_detail_value(int32_t value, char *text, size_t capacity)
{
    if (s_control_view) { (void)snprintf(text, capacity, "%ld", (long)value); }
    else if (s_detail_index < 2U) {
        (void)snprintf(text, capacity, "%.1f %s", (double)value / 10.0,
            s_detail_index == 0U ? "C" : "%");
    } else {
        (void)snprintf(text, capacity, "%ld %s", (long)value,
            s_detail_index == 2U ? "原始值" : "人");
    }
}

static void format_elapsed(uint64_t elapsed_ms, char *text, size_t capacity)
{
    (void)snprintf(text, capacity, "%02lu:%02lu.%03lu",
        (unsigned long)(elapsed_ms / 60000U), (unsigned long)((elapsed_ms / 1000U) % 60U),
        (unsigned long)(elapsed_ms % 1000U));
}

static void update_detail(const app_history_stats_t *stats)
{
    if (!s_detail_active) { return; }
    char text[384], first_time[32], last_time[32], value_text[64], other_text[64];
    int32_t minimum = 0, maximum = 0;
    uint8_t have_range = 0U;
    for (unsigned int i = 0U; i < stats->sample_count; i++) {
        const app_history_sample_t *sample = app_history_sample_at(&s_history, (uint16_t)i);
        int32_t value;
        if (history_value(&sample->data, s_detail_index, &value)) {
            if (!have_range || value < minimum) { minimum = value; }
            if (!have_range || value > maximum) { maximum = value; }
            have_range = 1U;
        }
        if (!s_control_view && s_detail_index == 0U &&
            (sample->data.valid_flags & APP_HISTORY_VALID_TARGET)) {
            value = sample->data.target_x10;
            if (!have_range || value < minimum) { minimum = value; }
            if (!have_range || value > maximum) { maximum = value; }
            have_range = 1U;
        }
    }
    int32_t axis_minimum = 0, axis_maximum = 1;
    if (!s_control_view) {
        if (have_range) {
            static const int32_t minimum_margin[4] = {2, 5, 5, 1};
            int32_t margin = (maximum - minimum) / 8;
            if (margin < minimum_margin[s_detail_index]) { margin = minimum_margin[s_detail_index]; }
            axis_minimum = minimum - margin;
            axis_maximum = maximum + margin;
            if (s_detail_index != 0U && axis_minimum < 0) { axis_minimum = 0; }
            if (s_detail_index == 1U && axis_maximum > 1000) { axis_maximum = 1000; }
        } else {
            static const int32_t defaults_minimum[4] = {100, 0, 0, 0};
            static const int32_t defaults_maximum[4] = {400, 1000, 1000, 10};
            axis_minimum = defaults_minimum[s_detail_index];
            axis_maximum = defaults_maximum[s_detail_index];
        }
    }
    lv_chart_set_range(s_chart[s_detail_index], LV_CHART_AXIS_PRIMARY_Y, axis_minimum, axis_maximum);
    if (have_range) {
        format_detail_value(minimum, value_text, sizeof(value_text));
        format_detail_value(maximum, other_text, sizeof(other_text));
        (void)snprintf(text, sizeof(text), "窗口 %s .. %s | %u 点%s", value_text, other_text,
            (unsigned int)stats->sample_count,
            s_control_view ? "（软件请求，非电量）" : s_detail_index == 0U ? "（含执行目标）" : "");
    } else {
        (void)snprintf(text, sizeof(text), "窗口 无有效数据 | %u 点（空缺表示未知）",
            (unsigned int)stats->sample_count);
    }
    text_changed(s_detail_window, text);
    if (!s_control_view && s_detail_index < 2U) {
        (void)snprintf(text, sizeof(text), "%.1f", (double)axis_maximum / 10.0);
        text_changed(s_detail_axis_top, text);
        (void)snprintf(text, sizeof(text), "%.1f", (double)axis_minimum / 10.0);
    } else {
        (void)snprintf(text, sizeof(text), "%ld", (long)axis_maximum);
        text_changed(s_detail_axis_top, text);
        (void)snprintf(text, sizeof(text), "%ld", (long)axis_minimum);
    }
    text_changed(s_detail_axis_bottom, text);
    if (stats->sample_count == 0U) {
        s_detail_have_selected = 0U;
        text_changed(s_detail_time, "实际采样 未知");
        text_changed(s_detail_value, "选中 未知 | 等待真实采样");
        if (s_cursor[s_detail_index]) {
            lv_chart_set_cursor_point(s_chart[s_detail_index], s_cursor[s_detail_index],
                s_series[s_detail_index], LV_CHART_POINT_NONE);
        }
        return;
    }
    const app_history_sample_t *first = app_history_sample_at(&s_history, 0U);
    const app_history_sample_t *last = app_history_sample_at(&s_history, (uint16_t)(stats->sample_count - 1U));
    format_elapsed(first->elapsed_ms, first_time, sizeof(first_time));
    format_elapsed(last->elapsed_ms, last_time, sizeof(last_time));
    (void)snprintf(text, sizeof(text), "实际采样 %s -- %s", first_time, last_time);
    text_changed(s_detail_time, text);
    unsigned int selected = stats->sample_count - 1U;
    if (stats->session_id != s_detail_session) {
        s_detail_follow_latest = 1U;
        s_detail_have_selected = 0U;
        s_detail_reset_note = 2U;
        s_detail_session = stats->session_id;
    }
    if (!s_detail_follow_latest && s_detail_have_selected) {
        uint8_t found = 0U;
        for (unsigned int i = 0U; i < stats->sample_count; i++) {
            const app_history_sample_t *sample = app_history_sample_at(&s_history, (uint16_t)i);
            if (sample->elapsed_ms == s_detail_selected_ms) { selected = i; found = 1U; break; }
        }
        if (!found) {
            s_detail_follow_latest = 1U;
            s_detail_reset_note = 1U;
        }
    }
    const app_history_sample_t *sample = app_history_sample_at(&s_history, (uint16_t)selected);
    s_detail_selected_ms = sample->elapsed_ms;
    s_detail_have_selected = 1U;
    format_elapsed(sample->elapsed_ms, first_time, sizeof(first_time));
    int32_t value;
    uint8_t valid = history_value(&sample->data, s_detail_index, &value);
    if (valid) { format_detail_value(value, value_text, sizeof(value_text)); }
    else { (void)snprintf(value_text, sizeof(value_text), "未知"); }
    if (!s_control_view && s_detail_index == 0U) {
        if (sample->data.valid_flags & APP_HISTORY_VALID_TARGET) {
            (void)snprintf(other_text, sizeof(other_text), "%.1f C", (double)sample->data.target_x10 / 10.0);
        } else { (void)snprintf(other_text, sizeof(other_text), "未知"); }
        (void)snprintf(text, sizeof(text), "选中 %s | 温度 %s | 执行目标 %s",
            first_time, value_text, other_text);
    } else {
        (void)snprintf(text, sizeof(text), "选中 %s | %s %s", first_time,
            s_control_view ? "软件请求" : s_detail_index == 1U ? "湿度" : s_detail_index == 2U ? "光照" : "人数",
            value_text);
    }
    text_changed(s_detail_value, text);
    text_changed(s_detail_hint, s_detail_reset_note == 2U ? "会话已切换，跟随最新 | 横轴为采样顺序" :
        s_detail_reset_note == 1U ? "原样本已移出，跟随最新 | 横轴为采样顺序" :
        s_detail_follow_latest ? "拖动选点 | 跟随最新 | 横轴为采样顺序" :
        "拖动选点 | 固定时间点 | 横轴为采样顺序");
    if (s_cursor[s_detail_index]) {
        lv_point_t position = {detail_point_x(selected), 0};
        lv_chart_set_cursor_pos(s_chart[s_detail_index], s_cursor[s_detail_index], &position);
    }
}

static void update_history(void)
{
    app_history_stats_t stats;
    app_history_get_stats(&s_history, &stats);
    if (!s_chart_current || stats.revision != s_chart_revision) {
        for (unsigned int i = 0U; i < 5U; i++) {
            for (unsigned int n = 0U; n < APP_HISTORY_SAMPLE_CAPACITY; n++) { s_values[i][n] = LV_CHART_POINT_NONE; }
        }
        unsigned int offset = APP_HISTORY_SAMPLE_CAPACITY - stats.sample_count;
        if (s_detail_active) {
            /* Every series already owns an external static array. Changing
             * point_count here does not allocate or replace a sample buffer. */
            lv_chart_set_point_count(s_chart[s_detail_index], stats.sample_count < 2U ? 2U : stats.sample_count);
        }
        for (unsigned int i = 0U; i < stats.sample_count; i++) {
            const app_history_sample_t *sample = app_history_sample_at(&s_history, (uint16_t)i);
            const app_history_input_t *data = &sample->data;
            unsigned int n = offset + i;
            unsigned int point[4];
            for (unsigned int metric = 0U; metric < 4U; metric++) {
                point[metric] = s_detail_active && s_detail_index == metric ? i : n;
            }
            if (s_control_view) {
                if (data->valid_flags & APP_HISTORY_VALID_REQUEST) {
                    for (unsigned int output = 0U; output < 4U; output++) {
                        s_values[output][point[output]] = (data->request_mask >> output) & 1U;
                    }
                }
            } else {
                if (data->valid_flags & APP_HISTORY_VALID_TEMPERATURE) { s_values[0][point[0]] = data->temperature_x10; }
                if (data->valid_flags & APP_HISTORY_VALID_HUMIDITY) { s_values[1][point[1]] = data->humidity_x10; }
                if (data->valid_flags & APP_HISTORY_VALID_ALS) { s_values[2][point[2]] = data->als; }
                if (data->valid_flags & APP_HISTORY_VALID_PEOPLE) { s_values[3][point[3]] = data->people; }
                if (data->valid_flags & APP_HISTORY_VALID_TARGET) { s_values[4][point[0]] = data->target_x10; }
            }
        }
        update_detail(&stats);
        for (unsigned int i = 0U; i < 4U; i++) { lv_chart_refresh(s_chart[i]); }
        size_t used = 0U;
        s_events_text[0] = '\0';
        unsigned int first = stats.event_count > 8U ? stats.event_count - 8U : 0U;
        for (unsigned int i = first; i < stats.event_count; i++) {
            const app_history_event_t *event = app_history_event_at(&s_history, (uint16_t)i);
            unsigned long seconds = (unsigned long)(event->elapsed_ms / 1000U);
            int written = snprintf(s_events_text + used, sizeof(s_events_text) - used,
                "%02lu:%02lu  %02X  %s\n", seconds / 60U, seconds % 60U,
                (unsigned int)event->request_mask, short_reason(event->reason));
            if (written < 0 || (size_t)written >= sizeof(s_events_text) - used) { break; }
            used += (size_t)written;
        }
        text_changed(s_event_label, s_events_text[0] ? s_events_text : "等待事件");
        s_chart_revision = stats.revision;
        s_chart_current = 1U;
    }
    char text[200];
    (void)snprintf(text, sizeof(text), "5 s / 60 点 | 软件制冷 %lu s / 观测开启 %lu 次 | 未知 %lu s（非电量）",
        (unsigned long)(stats.cooling_request_ms / 1000U), (unsigned long)stats.cooling_request_starts,
        (unsigned long)(stats.request_unknown_ms / 1000U));
    text_changed(s_stats_label, text);
}

void app_ui_insights_update(const app_ui_snapshot_t *snapshot, const char *reason)
{
    if (!app_ui_insights_active()) { return; }
    char text[384];
    text_changed(s_source_label, snapshot->simulated ?
        (snapshot->demo_active ? "模拟回放 / 软件请求" : "PC 模拟 / 软件请求") : "实板采样 / 软件请求");
    if (s_page_kind == APP_UI_PAGE_DATA) {
        char sensing[128];
        app_ui_insights_sensing_text(snapshot, sensing, sizeof(sensing));
        (void)snprintf(text, sizeof(text), "策略原因：%s\n%s", reason, sensing);
        text_changed(s_summary, text);
        update_history();
    } else {
        static const char *const phases[] = {"尚未开始", "人员进入", "温度升高", "夜间睡眠", "无人节能", "无人关机", "AI 故障", "数据恢复", "演示完成"};
        unsigned int phase = snapshot->demo_phase <= 8U ? snapshot->demo_phase : 0U;
        (void)snprintf(text, sizeof(text), "%s%s\n回放时间 %lu s", phases[phase],
            snapshot->demo_paused ? " / 暂停" : "", (unsigned long)(snapshot->demo_elapsed_ms / 1000U));
        text_changed(s_phase_label, text);
        (void)snprintf(text, sizeof(text), "策略原因\n%s\n请求 %02X（未接负载）", reason,
            (unsigned int)app_climate_output_mask(&snapshot->climate));
        text_changed(s_demo_reason, text);
        lv_bar_set_value(s_demo_bar, (int32_t)phase, LV_ANIM_OFF);
        text_changed(s_start_label, snapshot->demo_active ? (snapshot->demo_paused ? "继续回放" : "暂停回放") : "开始回放");
        if (snapshot->simulated) {
            lv_obj_remove_state(s_start_button, LV_STATE_DISABLED);
            lv_obj_remove_state(s_stop_button, LV_STATE_DISABLED);
        } else {
            lv_obj_add_state(s_start_button, LV_STATE_DISABLED);
            lv_obj_add_state(s_stop_button, LV_STATE_DISABLED);
            text_changed(s_phase_label, "实板场景引导\n自动回放请在 PC 预览中操作");
        }
    }
}
