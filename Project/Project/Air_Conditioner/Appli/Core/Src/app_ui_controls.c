#include "app_ui_controls.h"
#include "app_ui_theme.h"

#include <stdio.h>
#include <string.h>

#define CONTROL_BUTTON_COUNT (11U)

static lv_obj_t *s_parent;
static lv_obj_t *s_backdrop;
static lv_obj_t *s_panel;
static lv_obj_t *s_control_row;
static lv_obj_t *s_title;
static lv_obj_t *s_status_label;
static lv_obj_t *s_limit_label;
static lv_obj_t *s_buttons[CONTROL_BUTTON_COUNT];
static app_ui_action_cb_t s_action;
static uint8_t s_selected[CONTROL_BUTTON_COUNT];
static char s_status[256];
static char s_limit[160];

static const app_climate_action_t s_actions[CONTROL_BUTTON_COUNT] = {
    APP_ACTION_FAN_1, APP_ACTION_FAN_2, APP_ACTION_FAN_3,
    APP_ACTION_FAN_4, APP_ACTION_FAN_5,
    APP_ACTION_SWING_LR, APP_ACTION_SWING_UD,
    APP_ACTION_LAMP, APP_ACTION_SLEEP_TOGGLE, APP_ACTION_ECO_TOGGLE,
    APP_ACTION_TURBO
};

static lv_obj_t *make_label(lv_obj_t *parent, const char *text,
                            int x, int y, int width)
{
    return app_ui_theme_label(parent, text, x, y, width);
}

static void set_text(lv_obj_t *label, char *storage, size_t capacity,
                      const char *text)
{
    if (strcmp(storage, text) != 0)
    {
        (void)snprintf(storage, capacity, "%s", text);
        /* Storage belongs to this module for the modal lifetime. Updating
         * a label never allocates a new string during a control click. */
        lv_label_set_text_static(label, storage);
    }
}

static void control_event(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED ||
        !app_ui_controls_active() || !s_action) { return; }
    unsigned int index = (unsigned int)(uintptr_t)lv_event_get_user_data(event);
    if (index >= CONTROL_BUTTON_COUNT ||
        lv_obj_has_flag(s_buttons[index], LV_OBJ_FLAG_HIDDEN)) { return; }
    s_action(s_actions[index]);
    /* No optimistic state or local policy: the next published snapshot is
     * the authoritative result, including AUTO and preference restrictions. */
}

static void close_event(lv_event_t *event)
{
    if (lv_event_get_code(event) == LV_EVENT_CLICKED) { app_ui_controls_close(); }
}

static void create_modal(void)
{
    static const char *const names[CONTROL_BUTTON_COUNT] = {
        "风档 1", "风档 2", "风档 3", "风档 4", "风档 5",
        "左右摆风", "上下摆风", "灯光", "手动睡眠", "手动节能", "强劲"
    };
    s_backdrop = lv_obj_create(s_parent);
    lv_obj_remove_style_all(s_backdrop);
    lv_obj_set_pos(s_backdrop, 0, 0);
    lv_obj_set_size(s_backdrop, 800, 480);
    lv_obj_set_style_bg_color(s_backdrop, lv_color_hex(0x000000U), 0);
    lv_obj_set_style_bg_opa(s_backdrop, LV_OPA_60, 0);
    lv_obj_add_flag(s_backdrop, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(s_backdrop, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_EVENT_BUBBLE |
                                  LV_OBJ_FLAG_GESTURE_BUBBLE);

    s_panel = app_ui_theme_card(s_backdrop, 140, 60, 520, 360);
    lv_obj_add_flag(s_panel, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(s_panel, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_EVENT_BUBBLE |
                               LV_OBJ_FLAG_GESTURE_BUBBLE);
    s_title = make_label(s_panel, "风档选择", 24, 20, 472);
    app_ui_theme_text_accent(s_title);
    s_status_label = make_label(s_panel, s_status, 24, 58, 472);
    s_limit_label = make_label(s_panel, s_limit, 24, 136, 472);
    app_ui_theme_text_caution(s_limit_label);

    /* One row is reused by all three panels. Hidden children do not occupy
     * Flex slots; MORE wraps the four existing choices into two rows. */
    s_control_row = app_ui_theme_row(s_panel, 24, 164, 472, 52, 12);

    for (unsigned int i = 0U; i < CONTROL_BUTTON_COUNT; i++)
    {
        int width = i < 5U ? 80 : 228;
        s_buttons[i] = app_ui_theme_button(s_control_row, names[i],
            0, 0, width, 52);
        lv_obj_add_event_cb(s_buttons[i], control_event, LV_EVENT_CLICKED,
                            (void *)(uintptr_t)i);
        lv_obj_add_flag(s_buttons[i], LV_OBJ_FLAG_HIDDEN);
        s_selected[i] = UINT8_MAX;
    }
    make_label(s_panel, "软件请求，未接负载", 24, 310, 300);
    lv_obj_t *done = app_ui_theme_button(s_panel, "完成", 360, 294, 136, 48);
    lv_obj_add_event_cb(done, close_event, LV_EVENT_CLICKED, NULL);
    lv_obj_add_flag(s_backdrop, LV_OBJ_FLAG_HIDDEN);
}

void app_ui_controls_init(lv_obj_t *parent, app_ui_action_cb_t action)
{
    app_ui_controls_close();
    s_parent = parent;
    s_action = action;
    if (s_backdrop && parent && lv_obj_get_parent(s_backdrop) != parent)
    { lv_obj_set_parent(s_backdrop, parent); }
}

uint8_t app_ui_controls_active(void)
{
    return s_backdrop && s_parent &&
        !lv_obj_has_flag(s_backdrop, LV_OBJ_FLAG_HIDDEN) &&
        lv_screen_active() == lv_obj_get_screen(s_parent);
}

void app_ui_controls_close(void)
{
    if (s_backdrop) { lv_obj_add_flag(s_backdrop, LV_OBJ_FLAG_HIDDEN); }
}

void app_ui_controls_show(uint8_t kind, const app_ui_snapshot_t *snapshot)
{
    if (!s_parent || !snapshot ||
        kind < APP_UI_CONTROLS_FAN || kind > APP_UI_CONTROLS_MORE) { return; }
    if (!s_backdrop) { create_modal(); }
    lv_label_set_text_static(s_title, kind == APP_UI_CONTROLS_FAN ? "风档选择" :
        kind == APP_UI_CONTROLS_SWING ? "摆风设置" : "更多设置");
    lv_obj_set_flex_flow(s_control_row, kind == APP_UI_CONTROLS_MORE ?
        LV_FLEX_FLOW_ROW_WRAP : LV_FLEX_FLOW_ROW);
    lv_obj_set_height(s_control_row, kind == APP_UI_CONTROLS_MORE ? 114 : 52);
    lv_obj_set_style_pad_column(s_control_row, kind == APP_UI_CONTROLS_FAN ? 12 : 16, 0);
    lv_obj_set_style_pad_row(s_control_row, kind == APP_UI_CONTROLS_MORE ? 10 : 0, 0);
    for (unsigned int i = 0U; i < CONTROL_BUTTON_COUNT; i++)
    {
        uint8_t visible = (kind == APP_UI_CONTROLS_FAN && i < 5U) ||
            (kind == APP_UI_CONTROLS_SWING && i >= 5U && i < 7U) ||
            (kind == APP_UI_CONTROLS_MORE && i >= 7U);
        if (visible) { lv_obj_remove_flag(s_buttons[i], LV_OBJ_FLAG_HIDDEN); }
        else { lv_obj_add_flag(s_buttons[i], LV_OBJ_FLAG_HIDDEN); }
    }
    lv_obj_remove_flag(s_backdrop, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_to_index(s_backdrop, -1);
    app_ui_controls_update(snapshot);
}

void app_ui_controls_update(const app_ui_snapshot_t *snapshot)
{
    if (!snapshot || !app_ui_controls_active()) { return; }
    const app_climate_state_t *state = &snapshot->climate;
    uint8_t automatic = state->mode == APP_CLIMATE_MODE_AUTO;
    uint8_t execution_fan = (app_climate_output_mask(state) & 0x02U) ?
        state->effective_fan_speed : 0U;
    uint8_t selected[CONTROL_BUTTON_COUNT];
    char text[256];
    (void)snprintf(text, sizeof(text),
        "已选风档 %u | 执行风档 %u\n手动睡眠 %s / 手动节能 %s\n自动睡眠 %s / 自动节能 %s",
        (unsigned int)state->fan_speed, (unsigned int)execution_fan,
        state->manual_sleep ? "开启" : "关闭",
        state->manual_energy_saving ? "开启" : "关闭",
        automatic && state->auto_sleep ? "开启" : "关闭",
        automatic && state->auto_energy_saving ? "开启" : "关闭");
    set_text(s_status_label, s_status, sizeof(s_status), text);
    const char *limit = !state->power_on ? "电源关闭：设置保留，软件输出关闭" :
        state->effective_sleep ? "睡眠优先：执行风档 1" :
        state->effective_eco ? "节能限制：执行风档最多 2" :
        state->turbo ? "强劲已选：执行风档 5" : "按已选风档执行";
    set_text(s_limit_label, s_limit, sizeof(s_limit), limit);
    for (unsigned int i = 0U; i < 5U; i++)
    { selected[i] = state->fan_speed == i + 1U; }
    selected[5] = state->swing_lr;
    selected[6] = state->swing_ud;
    selected[7] = state->lamp_on;
    selected[8] = state->manual_sleep;
    selected[9] = state->manual_energy_saving;
    selected[10] = state->turbo;
    for (unsigned int i = 0U; i < CONTROL_BUTTON_COUNT; i++)
    {
        if (!lv_obj_has_flag(s_buttons[i], LV_OBJ_FLAG_HIDDEN) &&
            s_selected[i] != selected[i])
        { app_ui_theme_checked(s_buttons[i], selected[i] != 0U);
          s_selected[i] = selected[i]; }
    }
}
