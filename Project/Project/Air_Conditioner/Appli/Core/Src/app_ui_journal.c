#include "app_ui_journal.h"
#include "app_ui_theme.h"

#include <stdio.h>
#include <string.h>

static lv_obj_t *s_parent;
static lv_obj_t *s_backdrop;
static lv_obj_t *s_panel;
static lv_obj_t *s_list[APP_EVENT_JOURNAL_CAPACITY];
static lv_obj_t *s_detail;
static lv_obj_t *s_image_status;
static lv_obj_t *s_image;
static lv_obj_t *s_empty;
static lv_obj_t *s_capture_button;
static app_event_journal_t *s_journal;
static app_ui_journal_capture_cb_t s_capture;
static void *s_capture_context;
static lv_image_dsc_t s_image_descriptor;
static uint32_t s_selected_id;
static uint32_t s_revision;
static uint8_t s_selected_lost;
static char s_list_text[APP_EVENT_JOURNAL_CAPACITY][112];
static char s_detail_text[448];
static char s_image_status_text[176];
static char s_empty_text[160];

static const char *event_name(uint8_t type)
{
    static const char *const names[APP_EVENT_TYPE_COUNT] = {
        "确认有人", "确认无人", "温度范围提醒", "湿度范围提醒",
        "环境恢复", "手动快照"
    };
    return type < APP_EVENT_TYPE_COUNT ? names[type] : "未知事件";
}

static const char *source_name(uint8_t source)
{
    return source == APP_EVENT_SOURCE_BOARD ? "开发板实测" :
        source == APP_EVENT_SOURCE_SIMULATED ? "PC 模拟" : "演示数据";
}

static const char *quality_name(uint8_t quality)
{
    return quality == APP_EVENT_QUALITY_VALID ? "有效" :
        quality == APP_EVENT_QUALITY_STALE ? "采样过期" : "未取得";
}

static void set_text(lv_obj_t *label, char *storage, size_t capacity,
                      const char *text)
{
    if (strcmp(storage, text) != 0)
    {
        (void)snprintf(storage, capacity, "%s", text);
        lv_label_set_text_static(label, storage);
    }
}

static void selection_event(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED ||
        !app_ui_journal_active()) { return; }
    uint32_t index = (uint32_t)(uintptr_t)lv_event_get_user_data(event);
    const app_event_record_t *record = app_event_journal_latest(s_journal, index);
    if (!record) { return; }
    s_selected_id = record->id;
    s_selected_lost = 0U;
    s_revision = ~s_journal->revision;
    app_ui_journal_update();
}

static void close_event(lv_event_t *event)
{
    if (lv_event_get_code(event) == LV_EVENT_CLICKED) { app_ui_journal_close(); }
}

static void capture_event(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED ||
        !app_ui_journal_active() || !s_capture) { return; }
    s_capture(s_capture_context);
    const app_event_record_t *record = app_event_journal_latest(s_journal, 0U);
    s_selected_id = record ? record->id : 0U;
    s_selected_lost = 0U;
    s_revision = ~s_journal->revision;
    app_ui_journal_update();
}

static void create_modal(void)
{
    s_backdrop = lv_obj_create(s_parent);
    lv_obj_remove_style_all(s_backdrop);
    lv_obj_set_pos(s_backdrop, 0, 0);
    lv_obj_set_size(s_backdrop, 800, 480);
    lv_obj_set_style_bg_color(s_backdrop, lv_color_hex(0x000000U), 0);
    lv_obj_set_style_bg_opa(s_backdrop, LV_OPA_60, 0);
    lv_obj_add_flag(s_backdrop, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(s_backdrop, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_EVENT_BUBBLE |
                                  LV_OBJ_FLAG_GESTURE_BUBBLE);
    s_panel = app_ui_theme_card(s_backdrop, 32, 40, 736, 390);
    lv_obj_add_flag(s_panel, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(s_panel, LV_OBJ_FLAG_EVENT_BUBBLE | LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_t *title = app_ui_theme_label(s_panel, "事件回看", 24, 16, 688);
    app_ui_theme_text_accent(title);
    app_ui_theme_text_muted(app_ui_theme_label(s_panel,
        "仅当前检测视野 · 图片为事件附近显示帧", 24, 48, 688));
    for (uint32_t i = 0U; i < APP_EVENT_JOURNAL_CAPACITY; ++i)
    {
        s_list[i] = app_ui_theme_button(s_panel, s_list_text[i], 24,
            88 + (int32_t)i * 60, 224, 48);
        lv_obj_add_event_cb(s_list[i], selection_event, LV_EVENT_CLICKED,
                            (void *)(uintptr_t)i);
    }
    s_detail = app_ui_theme_label(s_panel, s_detail_text, 272, 88, 284);
    s_empty = app_ui_theme_label(s_panel, s_empty_text, 272, 150, 432);
    app_ui_theme_text_caution(s_empty);
    app_ui_theme_text_muted(app_ui_theme_label(s_panel,
        "事件附近显示帧", 572, 80, 140));
    s_image = lv_image_create(s_panel);
    lv_obj_remove_style_all(s_image);
    lv_obj_set_pos(s_image, 576, 132);
    lv_obj_set_size(s_image, APP_EVENT_THUMB_WIDTH, APP_EVENT_THUMB_HEIGHT);
    lv_obj_remove_flag(s_image, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE |
        LV_OBJ_FLAG_EVENT_BUBBLE | LV_OBJ_FLAG_GESTURE_BUBBLE);
    memset(&s_image_descriptor, 0, sizeof(s_image_descriptor));
    s_image_descriptor.header.magic = LV_IMAGE_HEADER_MAGIC;
    s_image_descriptor.header.cf = LV_COLOR_FORMAT_RGB565;
    s_image_descriptor.header.w = APP_EVENT_THUMB_WIDTH;
    s_image_descriptor.header.h = APP_EVENT_THUMB_HEIGHT;
    s_image_descriptor.header.stride = APP_EVENT_THUMB_WIDTH * sizeof(uint16_t);
    s_image_descriptor.data_size = APP_EVENT_THUMB_BYTES;
    s_image_status = app_ui_theme_label(s_panel, s_image_status_text, 572, 268, 140);
    app_ui_theme_text_caution(s_image_status);
    app_ui_theme_text_muted(app_ui_theme_label(s_panel,
        "最近 4 条 · 断电清空", 24, 338, 400));
    s_capture_button = app_ui_theme_button(s_panel, "拍一张", 444, 318, 128, 48);
    lv_obj_add_event_cb(s_capture_button, capture_event, LV_EVENT_CLICKED, NULL);
    lv_obj_t *close = app_ui_theme_button(s_panel, "关闭", 588, 318, 124, 48);
    lv_obj_add_event_cb(close, close_event, LV_EVENT_CLICKED, NULL);
    lv_obj_add_flag(s_backdrop, LV_OBJ_FLAG_HIDDEN);
}

void app_ui_journal_init(app_ui_journal_capture_cb_t capture, void *context)
{
    app_ui_journal_close();
    s_capture = capture;
    s_capture_context = context;
}

void app_ui_journal_open(lv_obj_t *parent, app_event_journal_t *journal)
{
    if (!parent || !journal) { return; }
    s_parent = parent;
    s_journal = journal;
    if (!s_backdrop) { create_modal(); }
    else if (lv_obj_get_parent(s_backdrop) != parent)
    { lv_obj_set_parent(s_backdrop, parent); }
    const app_event_record_t *record = app_event_journal_latest(journal, 0U);
    s_selected_id = record ? record->id : 0U;
    s_selected_lost = 0U;
    s_revision = ~journal->revision;
    lv_obj_set_state(s_capture_button, LV_STATE_DISABLED, s_capture == NULL);
    lv_obj_remove_flag(s_backdrop, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_to_index(s_backdrop, -1);
    app_ui_journal_update();
}

void app_ui_journal_close(void)
{
    if (s_backdrop) { lv_obj_add_flag(s_backdrop, LV_OBJ_FLAG_HIDDEN); }
    if (s_image) { lv_obj_add_flag(s_image, LV_OBJ_FLAG_HIDDEN); }
    if (s_image) { lv_image_cache_drop(&s_image_descriptor); }
}

uint8_t app_ui_journal_active(void)
{
    return s_backdrop && s_parent && s_journal &&
        !lv_obj_has_flag(s_backdrop, LV_OBJ_FLAG_HIDDEN) &&
        lv_screen_active() == lv_obj_get_screen(s_parent);
}

void app_ui_journal_update(void)
{
    if (!app_ui_journal_active() || s_revision == s_journal->revision) { return; }
    s_revision = s_journal->revision;
    const app_event_record_t *selected =
        app_event_journal_lookup(s_journal, s_selected_id);
    if (s_selected_id && !selected)
    { s_selected_id = 0U; s_selected_lost = 1U; }
    if (!s_selected_id && !s_selected_lost)
    {
        selected = app_event_journal_latest(s_journal, 0U);
        if (selected) { s_selected_id = selected->id; }
    }
    char text[448];
    for (uint32_t i = 0U; i < APP_EVENT_JOURNAL_CAPACITY; ++i)
    {
        const app_event_record_t *record = app_event_journal_latest(s_journal, i);
        if (!record)
        { lv_obj_add_flag(s_list[i], LV_OBJ_FLAG_HIDDEN); continue; }
        lv_obj_remove_flag(s_list[i], LV_OBJ_FLAG_HIDDEN);
        uint32_t seconds = record->meta.run_ms / 1000U;
        (void)snprintf(text, sizeof(text), "%s\n%02lu:%02lu:%02lu",
            event_name(record->meta.type), (unsigned long)(seconds / 3600U),
            (unsigned long)((seconds / 60U) % 60U), (unsigned long)(seconds % 60U));
        set_text(lv_obj_get_child(s_list[i], 0), s_list_text[i],
            sizeof(s_list_text[i]), text);
        app_ui_theme_checked(s_list[i], record->id == s_selected_id);
    }
    lv_image_cache_drop(&s_image_descriptor);
    lv_obj_add_flag(s_image, LV_OBJ_FLAG_HIDDEN);
    if (!selected)
    {
        lv_obj_add_flag(s_detail, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_image_status, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(s_empty, LV_OBJ_FLAG_HIDDEN);
        set_text(s_empty, s_empty_text, sizeof(s_empty_text), s_selected_lost ?
            "所选记录已被新事件替换。\n请点击左侧记录重新查看。" :
            "暂无事件。\n可点击“拍一张”保存当前读数与图片。");
        return;
    }
    lv_obj_add_flag(s_empty, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(s_detail, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(s_image_status, LV_OBJ_FLAG_HIDDEN);
    char people[32], temperature[64], humidity[64];
    if (selected->meta.people_valid)
    { (void)snprintf(people, sizeof(people), "%d", (int)selected->meta.people_count); }
    else { (void)snprintf(people, sizeof(people), "未取得"); }
    if (selected->meta.temperature_quality == APP_EVENT_QUALITY_UNAVAILABLE)
    { (void)snprintf(temperature, sizeof(temperature), "未取得"); }
    else if (selected->meta.temperature_quality == APP_EVENT_QUALITY_STALE)
    { (void)snprintf(temperature, sizeof(temperature), "采样过期"); }
    else { (void)snprintf(temperature, sizeof(temperature), "%s%d.%d C · %s",
        selected->meta.temperature_x10 < 0 ? "-" : "",
        (int)(selected->meta.temperature_x10 < 0 ? -selected->meta.temperature_x10 :
            selected->meta.temperature_x10) / 10,
        (int)(selected->meta.temperature_x10 < 0 ? -selected->meta.temperature_x10 :
            selected->meta.temperature_x10) % 10,
        quality_name(selected->meta.temperature_quality)); }
    if (selected->meta.humidity_quality == APP_EVENT_QUALITY_UNAVAILABLE)
    { (void)snprintf(humidity, sizeof(humidity), "未取得"); }
    else if (selected->meta.humidity_quality == APP_EVENT_QUALITY_STALE)
    { (void)snprintf(humidity, sizeof(humidity), "采样过期"); }
    else { (void)snprintf(humidity, sizeof(humidity), "%u.%u %% · %s",
        (unsigned int)selected->meta.humidity_x10 / 10U,
        (unsigned int)selected->meta.humidity_x10 % 10U,
        quality_name(selected->meta.humidity_quality)); }
    uint32_t seconds = selected->meta.run_ms / 1000U;
    const char *name = selected->meta.type == APP_EVENT_ENV_RECOVERY ?
        (selected->meta.environment_channel == APP_EVENT_CHANNEL_TEMPERATURE ?
            "温度恢复" : selected->meta.environment_channel == APP_EVENT_CHANNEL_HUMIDITY ?
            "湿度恢复" : "环境恢复") : event_name(selected->meta.type);
    (void)snprintf(text, sizeof(text), "%s\n运行 %02lu:%02lu:%02lu\n来源：%s\n人数：%s\n温度：%s\n湿度：%s",
        name, (unsigned long)(seconds / 3600U),
        (unsigned long)((seconds / 60U) % 60U), (unsigned long)(seconds % 60U),
        source_name(selected->meta.source), people, temperature, humidity);
    if (selected->image_status == APP_EVENT_IMAGE_READY)
    {
        uint32_t image_seconds = selected->image_captured_ms / 1000U;
        size_t used = strlen(text);
        (void)snprintf(text + used, sizeof(text) - used, "\n图时 %02lu:%02lu:%02lu",
            (unsigned long)(image_seconds / 3600U),
            (unsigned long)((image_seconds / 60U) % 60U),
            (unsigned long)(image_seconds % 60U));
    }
    set_text(s_detail, s_detail_text, sizeof(s_detail_text), text);
    const uint16_t *pixels = app_event_journal_image(s_journal, selected->id);
    if (pixels)
    {
        s_image_descriptor.data = (const uint8_t *)pixels;
        lv_image_set_src(s_image, &s_image_descriptor);
        lv_obj_remove_flag(s_image, LV_OBJ_FLAG_HIDDEN);
        (void)snprintf(text, sizeof(text), "%s\n帧 %lu",
            selected->meta.source == APP_EVENT_SOURCE_BOARD ? "显示缩略图" : "模拟图片",
            (unsigned long)selected->display_frame_id);
    }
    else
    {
        const char *status = selected->image_status == APP_EVENT_IMAGE_PENDING ?
            "图片等待采集" :
            selected->image_status == APP_EVENT_IMAGE_CAMERA_UNAVAILABLE ? "相机不可用" :
            selected->image_status == APP_EVENT_IMAGE_OLD ? "画面过期\n未保存图片" :
            selected->image_status == APP_EVENT_IMAGE_SKIPPED ? "未取得新帧\n未保存图片" :
            "未取得可用画面\n未保存图片";
        (void)snprintf(text, sizeof(text), "%s", status);
    }
    set_text(s_image_status, s_image_status_text, sizeof(s_image_status_text), text);
}
