#include "app_ui_temperature_input.h"

#define INPUT_TARGET_MIN        (16U)
#define INPUT_TARGET_MAX        (30U)
#define INPUT_ARC_RANGE         (270)
#define INPUT_DRAG_DISTANCE     (8)
#define INPUT_ACTION_LIMIT      (28U)

static lv_obj_t *s_arc;
static app_ui_action_cb_t s_action;
static app_ui_temperature_preview_cb_t s_preview;
static uint8_t s_cursor;
static uint8_t s_cursor_valid;
static uint8_t s_pressed;
static uint8_t s_dragging;
static uint8_t s_syncing;
static lv_point_t s_press_point;

static void input_preprocess(lv_event_t *event);
static void input_value_event(lv_event_t *event);

static uint8_t valid_target(uint8_t target)
{
    return target >= INPUT_TARGET_MIN && target <= INPUT_TARGET_MAX;
}

static int32_t target_angle(uint8_t target)
{
    return ((int32_t)target - INPUT_TARGET_MIN) * INPUT_ARC_RANGE /
        (INPUT_TARGET_MAX - INPUT_TARGET_MIN);
}

static uint8_t angle_target(int32_t angle)
{
    if (angle < 0) { angle = 0; }
    if (angle > INPUT_ARC_RANGE) { angle = INPUT_ARC_RANGE; }
    /* Round once to a whole degree Celsius, while leaving the arc itself
     * in its finer 0..270 visual range for the shared feedback animation. */
    return (uint8_t)(INPUT_TARGET_MIN +
        (angle * (INPUT_TARGET_MAX - INPUT_TARGET_MIN) + INPUT_ARC_RANGE / 2) /
        INPUT_ARC_RANGE);
}

static lv_indev_t *pointer_device(void)
{
    lv_indev_t *device = lv_indev_active();
    return device && lv_indev_get_type(device) == LV_INDEV_TYPE_POINTER ? device : NULL;
}

static void restore_arc(void)
{
    if (!s_arc) { return; }
    s_syncing = 1U;
    lv_arc_set_value(s_arc, s_cursor_valid ? target_angle(s_cursor) : 0);
    s_syncing = 0U;
}

static uint8_t submit_step(int8_t direction)
{
    if (!s_cursor_valid || !s_action || direction == 0) { return 0U; }
    if ((direction > 0 && s_cursor == INPUT_TARGET_MAX) ||
        (direction < 0 && s_cursor == INPUT_TARGET_MIN)) { return 0U; }
    /* The public UI snapshot can remain unchanged until the next frame.
     * Advance the input cursor before the synchronous callback so another
     * event in this frame cannot submit the same delta twice. A callback
     * may publish a new baseline; never replace it after the callback. */
    s_cursor = (uint8_t)((int32_t)s_cursor + (direction > 0 ? 1 : -1));
    s_action(direction > 0 ? APP_ACTION_TEMP_UP : APP_ACTION_TEMP_DOWN);
    return 1U;
}

static void request_target(uint8_t desired)
{
    uint8_t changed = 0U;
    if (!valid_target(desired)) { return; }
    /* Re-read the cursor for each action: stopping a PC demonstration can
     * synchronously restore and publish different preferences inside the
     * first callback. The bounded loop covers both 14-degree ranges. */
    for (unsigned int i = 0U; i < INPUT_ACTION_LIMIT && s_cursor_valid; ++i) {
        if (s_cursor == desired) { break; }
        if (!submit_step(desired > s_cursor ? 1 : -1)) { break; }
        changed = 1U;
    }
    if (changed && s_cursor_valid && s_preview) { s_preview(s_cursor); }
}

static void finish_gesture(void)
{
    s_pressed = 0U;
    s_dragging = 0U;
    /* Snap the visual end to the accepted whole-degree setpoint. The
     * climate actions were already submitted by the real drag events. */
    restore_arc();
}

static void input_preprocess(lv_event_t *event)
{
    if (lv_event_get_current_target(event) != s_arc) { return; }
    lv_event_code_t code = lv_event_get_code(event);
    if (code == LV_EVENT_DELETE) {
        s_arc = NULL;
        s_action = NULL;
        s_preview = NULL;
        s_cursor_valid = 0U;
        s_pressed = s_dragging = s_syncing = 0U;
        return;
    }
    if (code == LV_EVENT_PRESSED) {
        lv_indev_t *device = pointer_device();
        s_pressed = device && s_cursor_valid ? 1U : 0U;
        s_dragging = 0U;
        if (s_pressed) {
            lv_indev_get_point(device, &s_press_point);
            lv_anim_delete(s_arc, NULL);
            restore_arc();
        }
    } else if (code == LV_EVENT_PRESSING) {
        lv_indev_t *device = pointer_device();
        if (!s_pressed || !device || !s_cursor_valid) {
            lv_event_stop_processing(event);
            return;
        }
        if (!s_dragging) {
            lv_point_t point;
            lv_indev_get_point(device, &point);
            int64_t dx = (int64_t)point.x - s_press_point.x;
            int64_t dy = (int64_t)point.y - s_press_point.y;
            if (dx * dx + dy * dy < INPUT_DRAG_DISTANCE * INPUT_DRAG_DISTANCE) {
                /* Preprocessing precedes the stock arc's PRESSING handler.
                 * A stationary tap/hold therefore cannot jump temperature. */
                lv_event_stop_processing(event);
                return;
            }
            s_dragging = 1U;
        }
    } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        finish_gesture();
    }
}

static void input_value_event(lv_event_t *event)
{
    if (lv_event_get_current_target(event) != s_arc || s_syncing) { return; }
    /* Programmatic changes and feedback animations are display updates.
     * Only a captured pointer gesture beyond the movement threshold can
     * request a temperature, even if a caller sends VALUE_CHANGED itself. */
    if (!s_pressed || !s_dragging || !pointer_device()) {
        restore_arc();
        return;
    }
    request_target(angle_target(lv_arc_get_value(s_arc)));
}

void app_ui_temperature_input_init(lv_obj_t *arc,
                                   app_ui_action_cb_t action,
                                   app_ui_temperature_preview_cb_t preview)
{
    if (s_arc) {
        lv_obj_remove_event_cb(s_arc, input_preprocess);
        lv_obj_remove_event_cb(s_arc, input_value_event);
    }
    s_arc = arc;
    s_action = action;
    s_preview = preview;
    s_cursor = 0U;
    s_cursor_valid = s_pressed = s_dragging = s_syncing = 0U;
    if (!s_arc) { return; }
    lv_arc_set_range(s_arc, 0, INPUT_ARC_RANGE);
    lv_obj_add_flag(s_arc, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_ADV_HITTEST);
    lv_obj_remove_flag(s_arc, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_EVENT_BUBBLE |
        LV_OBJ_FLAG_GESTURE_BUBBLE);
    /* The thin decorative stroke keeps its appearance; hit testing gets
     * an inner/outer band wide enough for a fingertip and excludes center. */
    lv_obj_set_ext_click_area(s_arc, 16);
    lv_obj_add_event_cb(s_arc, input_preprocess,
        (lv_event_code_t)(LV_EVENT_ALL | LV_EVENT_PREPROCESS), NULL);
    lv_obj_add_event_cb(s_arc, input_value_event, LV_EVENT_VALUE_CHANGED, NULL);
}

void app_ui_temperature_input_observe_target(uint8_t target)
{
    s_cursor_valid = valid_target(target);
    s_cursor = target;
    if (!s_cursor_valid) { app_ui_temperature_input_cancel(); }
}

void app_ui_temperature_input_step(int8_t direction)
{
    /* The sign denotes one button click, not an arbitrary action batch. */
    if (submit_step(direction) && s_cursor_valid && s_preview) { s_preview(s_cursor); }
}

uint8_t app_ui_temperature_input_target(uint8_t fallback)
{
    return s_cursor_valid ? s_cursor : fallback;
}

uint8_t app_ui_temperature_input_dragging(void)
{
    return s_dragging;
}

void app_ui_temperature_input_cancel(void)
{
    s_pressed = s_dragging = 0U;
    if (s_arc) { lv_anim_delete(s_arc, NULL); }
    restore_arc();
}
