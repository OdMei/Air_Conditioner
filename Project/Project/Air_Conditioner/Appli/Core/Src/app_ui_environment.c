#include "app_ui_environment.h"
#include "app_ui_port.h"
#include "app_ui_theme.h"

#include <stdio.h>
#include <string.h>

static app_environment_alert_t *s_state;
static app_ui_environment_event_cb_t s_event;
static lv_obj_t *s_backdrop;
static lv_obj_t *s_panel;
static lv_obj_t *s_source;
static lv_obj_t *s_status[APP_ENV_CHANNEL_COUNT];
static lv_obj_t *s_config[3];
static lv_obj_t *s_ack;
static lv_obj_t *s_snooze;

static const char *status_name(app_environment_status_t status)
{
    switch (status) {
    case APP_ENV_NORMAL: return "正常";
    case APP_ENV_PENDING: return "持续确认";
    case APP_ENV_ACTIVE: return "持续超限";
    case APP_ENV_ACKNOWLEDGED: return "已确认，仍超限";
    case APP_ENV_SNOOZED: return "稍后提醒";
    default: return "采样不可用";
    }
}

static void event(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED || !s_state ||
        !app_ui_environment_active()) { return; }
    unsigned int command = (unsigned int)(uintptr_t)lv_event_get_user_data(e);
    uint32_t now = app_ui_tick_ms();
    app_environment_events_t changes = 0U;
    app_environment_config_t config = s_state->config;
    switch (command) {
    case 0U: config.temperature_high_x10 -= 10; break;
    case 1U: config.temperature_high_x10 += 10; break;
    case 2U: config.humidity_high = config.humidity_high >= 35U ? config.humidity_high - 5U : 30U; break;
    case 3U: config.humidity_high = config.humidity_high <= 90U ? config.humidity_high + 5U : 95U; break;
    case 4U: config.hold_ms = config.hold_ms > APP_ENV_HOLD_MIN_MS ? config.hold_ms - 30000U : APP_ENV_HOLD_MIN_MS; break;
    case 5U: config.hold_ms += 30000U; break;
    case 6U: changes = app_environment_ack(s_state, APP_ENV_CHANNEL_MASK_ALL, now); break;
    case 7U: changes = app_environment_snooze(s_state, APP_ENV_CHANNEL_MASK_ALL, now); break;
    default: app_ui_environment_close(); return;
    }
    if (command < 6U) { changes = app_environment_set_config(s_state, &config, now); }
    if (changes && s_event) { s_event(changes); }
    app_ui_environment_update();
}

static lv_obj_t *button(const char *name, unsigned int command,
                         int x, int y, int width)
{
    lv_obj_t *object = app_ui_theme_button(s_panel, name, x, y, width, 44);
    lv_obj_add_event_cb(object, event, LV_EVENT_CLICKED, (void *)(uintptr_t)command);
    return object;
}

static void create(lv_obj_t *parent)
{
    s_backdrop = lv_obj_create(parent);
    lv_obj_remove_style_all(s_backdrop);
    lv_obj_set_size(s_backdrop, LCD_FG_WIDTH, LCD_FG_HEIGHT);
    lv_obj_set_style_bg_color(s_backdrop, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_backdrop, LV_OPA_60, 0);
    lv_obj_add_flag(s_backdrop, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(s_backdrop, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_EVENT_BUBBLE |
                                  LV_OBJ_FLAG_GESTURE_BUBBLE);
    s_panel = app_ui_theme_card(s_backdrop, 96, 28, 608, 424);
    lv_obj_add_flag(s_panel, LV_OBJ_FLAG_CLICKABLE);
    app_ui_theme_text_accent(app_ui_theme_label(s_panel, "环境持续提醒", 20, 12, 268));
    s_source = app_ui_theme_label(s_panel, "个人阈值", 314, 12, 270);
    lv_obj_set_style_text_align(s_source, LV_TEXT_ALIGN_RIGHT, 0);
    s_status[0] = app_ui_theme_label(s_panel, "温度 等待采样", 20, 50, 568);
    s_status[1] = app_ui_theme_label(s_panel, "湿度 等待采样", 20, 82, 568);
    lv_obj_t *note = app_ui_theme_label(s_panel,
        "个人设定，仅用于提醒；不改变空调控制\n阈值仅本次上电保存，采样失效不计为恢复", 20, 114, 568);
    app_ui_theme_text_muted(note);
    static const char *const names[6] = {"温限 -", "温限 +", "湿限 -", "湿限 +", "时长 -", "时长 +"};
    static const char *const captions[3] = {"温度高限 30 C", "湿度高限 70 %", "连续超限 120 s"};
    for (unsigned int i = 0U; i < 3U; i++) {
        int y = 166 + (int)i * 52;
        s_config[i] = app_ui_theme_label(s_panel, captions[i], 20, y + 10, 306);
        button(names[i * 2U], i * 2U, 334, y, 108);
        button(names[i * 2U + 1U], i * 2U + 1U, 454, y, 128);
    }
    s_ack = button("确认提醒", 6U, 20, 332, 176);
    s_snooze = button("稍后提醒", 7U, 212, 332, 176);
    button("关闭", 8U, 404, 332, 178);
    note = app_ui_theme_label(s_panel, "恢复迟滞：温度 1 C / 湿度 3 %；静默 5 分钟", 20, 386, 568);
    app_ui_theme_text_muted(note);
}

void app_ui_environment_init(app_environment_alert_t *state,
                              app_ui_environment_event_cb_t event_cb)
{
    s_state = state;
    s_event = event_cb;
}

void app_ui_environment_open(lv_obj_t *parent)
{
    if (!parent || !s_state) { return; }
    if (!s_backdrop) { create(parent); }
    else if (lv_obj_get_parent(s_backdrop) != parent) { lv_obj_set_parent(s_backdrop, parent); }
    lv_obj_remove_flag(s_backdrop, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_to_index(s_backdrop, -1);
    app_ui_environment_update();
}

void app_ui_environment_close(void)
{
    if (s_backdrop) { lv_obj_add_flag(s_backdrop, LV_OBJ_FLAG_HIDDEN); }
}

uint8_t app_ui_environment_active(void)
{
    return s_backdrop && !lv_obj_has_flag(s_backdrop, LV_OBJ_FLAG_HIDDEN) &&
        lv_obj_get_screen(s_backdrop) == lv_screen_active();
}

void app_ui_environment_update(void)
{
    if (!s_state || !app_ui_environment_active()) { return; }
    app_environment_view_t view;
    app_environment_get_view(s_state, app_ui_tick_ms(), &view);
    app_ui_theme_text(s_source, view.source == APP_ENV_SOURCE_BOARD ? "实板采样 / 个人阈值" :
        view.source == APP_ENV_SOURCE_DEMO ? "模拟回放 / 个人阈值" : "PC 模拟 / 个人阈值");
    char text[192];
    uint8_t any_raised = 0U;
    for (unsigned int i = 0U; i < APP_ENV_CHANNEL_COUNT; i++) {
        const app_environment_channel_view_t *channel = &view.channels[i];
        const char *name = i == APP_ENV_CHANNEL_TEMPERATURE ? "温度" : "湿度";
        if (channel->fresh) {
            (void)snprintf(text, sizeof(text), "%s %.1f %s | %s", name,
                (double)channel->value_x10 / 10.0, i == 0U ? "C" : "%",
                status_name(channel->status));
        } else {
            (void)snprintf(text, sizeof(text), "%s -- | 采样不可用%s", name,
                channel->raised ? "，保留原提醒" : "，连续确认已清零");
        }
        size_t used = strlen(text);
        if (channel->status == APP_ENV_PENDING) {
            (void)snprintf(text + used, sizeof(text) - used, " %lu/%lu s",
                (unsigned long)(channel->pending_ms / 1000U), (unsigned long)(view.config.hold_ms / 1000U));
        } else if (channel->status == APP_ENV_SNOOZED) {
            (void)snprintf(text + used, sizeof(text) - used, " %lu s", (unsigned long)(channel->snooze_remaining_ms / 1000U));
        }
        app_ui_theme_text(s_status[i], text);
        lv_obj_set_style_text_color(s_status[i], lv_color_hex(
            !channel->fresh || channel->raised ? APP_UI_COLOR_CAUTION :
            channel->status == APP_ENV_PENDING ? APP_UI_COLOR_ACCENT : APP_UI_COLOR_TEXT), 0);
        any_raised |= channel->raised;
    }
    (void)snprintf(text, sizeof(text), "温度高限 %.0f C", (double)view.config.temperature_high_x10 / 10.0);
    app_ui_theme_text(s_config[0], text);
    (void)snprintf(text, sizeof(text), "湿度高限 %u %%", (unsigned int)view.config.humidity_high);
    app_ui_theme_text(s_config[1], text);
    (void)snprintf(text, sizeof(text), "连续超限 %lu s", (unsigned long)(view.config.hold_ms / 1000U));
    app_ui_theme_text(s_config[2], text);
    lv_obj_set_state(s_ack, LV_STATE_DISABLED, !any_raised);
    lv_obj_set_state(s_snooze, LV_STATE_DISABLED, !any_raised);
}

uint8_t app_ui_environment_summary(char *text, size_t capacity)
{
    if (!s_state || !text || !capacity) { return 0U; }
    app_environment_view_t view;
    app_environment_get_view(s_state, app_ui_tick_ms(), &view);
    /* An unacknowledged fresh warning takes priority over an older snoozed
       warning in the other channel. The modal still shows both channels. */
    for (unsigned int priority = 0U; priority < 4U; priority++) {
      for (unsigned int i = 0U; i < APP_ENV_CHANNEL_COUNT; i++) {
        const app_environment_channel_view_t *channel = &view.channels[i];
        if ((priority == 0U && channel->fresh && channel->status == APP_ENV_ACTIVE) ||
            (priority == 1U && channel->raised && !channel->fresh) ||
            (priority == 2U && channel->raised) ||
            (priority == 3U && channel->status == APP_ENV_PENDING)) {
            const char *name = i == APP_ENV_CHANNEL_TEMPERATURE ? "温度" : "湿度";
            if (channel->status == APP_ENV_PENDING) {
                (void)snprintf(text, capacity, "%s超限确认 %lu/%lu s", name,
                    (unsigned long)(channel->pending_ms / 1000U), (unsigned long)(view.config.hold_ms / 1000U));
            } else {
                (void)snprintf(text, capacity, "%s提醒：%s", name,
                    !channel->fresh ? "采样不可用" : status_name(channel->status));
            }
            return 1U;
        }
      }
    }
    text[0] = '\0';
    return 0U;
}
