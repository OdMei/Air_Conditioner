#include <stdbool.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app_input_logic.h"
#include "app_demo_engine.h"
#include "app_ui.h"
#include "app_ui_controls.h"
#include "app_ui_insights.h"
#include "app_ui_port.h"
#include "app_ui_theme.h"
#include "app_ui_watch_home.h"
#include "app_ui_watch_assets.h"
#include "app_ui_lifecycle.h"
#include "app_ui_environment.h"
#include "app_ui_journal.h"
#include "app_model_config.h"
#include "lvgl.h"
#include "pc_port.h"

LV_FONT_DECLARE(app_font_ui_16)

static app_ui_snapshot_t s_state;
static bool s_ai_online = true;
static bool s_als_online = true;
static bool s_sensor_online = true;
static bool s_skip_automatic_samples;
static uint32_t s_last_sample_tick;
static uint32_t s_last_wall_tick;
static bool s_have_log;
static app_climate_reason_t s_log_reason;
static uint8_t s_log_output;
static app_demo_engine_t s_demo;
static app_ui_snapshot_t s_before_demo;
static uint32_t s_before_demo_tick;
static bool s_before_ai_online, s_before_als_online, s_before_sensor_online;
static bool s_before_skip_samples;
static bool s_demo_driving;
static bool s_have_saved_demo;
static bool s_mock_vision = true;
static bool s_person_right;
static bool s_mock_jitter;
static bool s_before_mock_vision, s_before_person_right;
static bool s_before_mock_jitter;

static void reset_simulation(void);
static void publish(void);
static void advance_model_time(uint64_t milliseconds);
static void handle_demo_command(uint8_t command);
static void demo_step(app_demo_phase_t phase, uint8_t enter,
                      uint32_t model_ms, void *context);
static void stop_demo(bool restore);
static void update_mock_vision(uint32_t now)
{
    if (!s_mock_vision) { return; }
    app_vision_result_t *vision = &s_state.vision;
    vision->frame_id++;
    vision->captured_ms = vision->completed_ms = now;
    vision->valid = s_ai_online ? 1U : 0U;
    vision->count = (uint8_t)(s_ai_online && s_state.people_count > 0 ?
        (s_state.people_count > 1 ? 2U : 1U) : 0U);
    if (vision->count) {
        vision->boxes[0] = (app_ui_detection_t){
            (uint16_t)(s_person_right && vision->count == 1U ? 500U : 200U),
            150U, 120U, 260U, 0.93f, 0};
    }
    if (vision->count > 1U) {
        vision->boxes[1] = (app_ui_detection_t){500U, 150U, 120U, 260U, 0.91f, 0};
    }
    if (s_mock_jitter) {
        int16_t jitter = (vision->frame_id & 1U) ? 8 : -8;
        for (uint8_t i = 0U; i < vision->count; i++) {
            vision->boxes[i].x = (uint16_t)((int32_t)vision->boxes[i].x + jitter);
            vision->boxes[i].y = (uint16_t)((int32_t)vision->boxes[i].y - jitter / 2);
        }
    }
    pc_camera_person_t people[2] = {{0}};
    for (uint8_t i = 0U; i < vision->count; i++) {
        people[i] = (pc_camera_person_t){vision->boxes[i].x, vision->boxes[i].y,
            vision->boxes[i].width, vision->boxes[i].height};
    }
    pc_port_camera_sample(vision->frame_id, now, true, people, vision->count);
}
static void apply_wall_delta(uint32_t milliseconds)
{
    if (s_demo.active) {
        app_demo_advance(&s_demo, milliseconds, demo_step, NULL);
        return;
    }
    uint32_t speed = s_state.simulation_speed ? s_state.simulation_speed : 1U;
    advance_model_time((uint64_t)milliseconds * speed);
}
static void refresh_climate(uint32_t now)
{
    app_climate_inputs_t inputs = app_ui_snapshot_inputs(&s_state);
    app_climate_step(&s_state.climate, &inputs, now);
    s_state.relay_mask = app_climate_output_mask(&s_state.climate);
}
static uint32_t s_last_climate_tick;
static bool s_running = true;
static unsigned int s_action_count;

static void apply_action(app_climate_action_t action)
{
    bool stopped_demo = s_demo.active && !s_demo_driving;
    if (stopped_demo) { stop_demo(true); }
    app_climate_apply_action(&s_state.climate, action);
    refresh_climate(app_ui_tick_ms());
    s_action_count++;
    /* Stopping a script publishes its restored preferences first. Publish
       the completed manual action as well so same-frame pointer callbacks
       observe the authoritative result, rather than the restored pre-action
       temperature. Regular actions retain the existing frame publication. */
    if (stopped_demo) { publish(); }
}

static void publish(void)
{
    s_state.demo_active = s_demo.active;
    s_state.demo_paused = s_demo.paused;
    s_state.demo_phase = s_demo.phase;
    s_state.demo_elapsed_ms = s_demo.elapsed_ms;
    s_state.relay_mask = app_climate_output_mask(&s_state.climate);
    app_ui_set_snapshot(&s_state);
}

static void restore_demo_state(void)
{
    if (!s_have_saved_demo) { return; }
    uint32_t now = app_ui_tick_ms();
    uint32_t shift = now - s_before_demo_tick;
    app_touch_state_t touch = s_state.touch;
    app_climate_state_t preferences = s_before_demo.climate;
    s_state = s_before_demo;
    s_state.touch = touch;
    /* Preserve the original sample ages at the new virtual clock. Restoring
       old absolute timestamps after 30 simulated minutes would fake a fault. */
    s_state.people_updated_ms += shift;
    s_state.als_updated_ms += shift;
    s_state.sensor_updated_ms += shift;
    s_state.humidity_updated_ms += shift;
    s_state.vision.captured_ms += shift;
    s_state.vision.completed_ms += shift;
    s_ai_online = s_before_ai_online;
    s_als_online = s_before_als_online;
    s_sensor_online = s_before_sensor_online;
    s_skip_automatic_samples = s_before_skip_samples;
    s_mock_vision = s_before_mock_vision;
    s_person_right = s_before_person_right;
    s_mock_jitter = s_before_mock_jitter;
    /* Restore controls through the same action API. Restart confirmation and
       the conservative cooling-off interval; never inherit script absence or
       a saved partially elapsed shutdown timer into manual operation. */
    app_climate_init(&s_state.climate, preferences.target_temperature);
    if (preferences.mode == APP_CLIMATE_MODE_MANUAL_COOL) {
        app_climate_apply_action(&s_state.climate, APP_ACTION_MODE_NEXT);
    }
    app_climate_apply_action(&s_state.climate,
        (app_climate_action_t)(APP_ACTION_FAN_1 + preferences.fan_speed - 1U));
    if (preferences.turbo) { app_climate_apply_action(&s_state.climate, APP_ACTION_TURBO); }
    if (preferences.swing_lr) { app_climate_apply_action(&s_state.climate, APP_ACTION_SWING_LR); }
    if (preferences.swing_ud) { app_climate_apply_action(&s_state.climate, APP_ACTION_SWING_UD); }
    if (!preferences.lamp_on) { app_climate_apply_action(&s_state.climate, APP_ACTION_LAMP); }
    if (preferences.manual_sleep) { app_climate_apply_action(&s_state.climate, APP_ACTION_SLEEP_TOGGLE); }
    if (preferences.manual_energy_saving) { app_climate_apply_action(&s_state.climate, APP_ACTION_ECO_TOGGLE); }
    if (preferences.power_on) { app_climate_apply_action(&s_state.climate, APP_ACTION_POWER); }
    s_last_sample_tick = s_last_climate_tick = now;
    s_have_log = false;
    s_have_saved_demo = false;
    refresh_climate(now);
    printf("DEMO,source=SIMULATED_SCRIPT,manual_state=restored,timers=restarted\n");
    fflush(stdout);
}

static void stop_demo(bool restore)
{
    app_demo_stop(&s_demo);
    if (restore) { restore_demo_state(); }
    else { s_have_saved_demo = false; }
    publish();
}

static void demo_step(app_demo_phase_t phase, uint8_t enter,
                      uint32_t model_ms, void *context)
{
    (void)context;
    if (enter && phase == APP_DEMO_COMPLETE) {
        restore_demo_state();
        return;
    }
    if (enter) {
        switch (phase) {
        case APP_DEMO_ENTER:
            app_climate_init(&s_state.climate, 26U);
            s_state.temperature = 25.0f;
            s_state.humidity = 55.0f;
            s_state.people_count = 1;
            s_state.als = 250U;
            s_ai_online = s_als_online = s_sensor_online = true;
            s_skip_automatic_samples = false;
            s_mock_vision = true;
            s_person_right = false;
            s_mock_jitter = false;
            break;
        case APP_DEMO_WARM: s_state.temperature = 29.0f; break;
        case APP_DEMO_NIGHT: s_state.als = 50U; break;
        case APP_DEMO_ABSENCE_ECO: s_state.people_count = 0; break;
        case APP_DEMO_AI_FAULT:
            s_ai_online = false;
            s_state.temperature = 25.0f;
            break;
        case APP_DEMO_RECOVER:
            s_ai_online = true;
            s_state.people_count = 1;
            s_state.als = 250U;
            s_state.temperature = 27.0f;
            break;
        default: break;
        }
        s_state.people_valid = s_ai_online;
        s_state.als_valid = s_als_online;
        s_state.sensor_valid = s_state.humidity_valid = s_sensor_online;
        s_state.people_updated_ms = s_state.als_updated_ms = app_ui_tick_ms();
        s_state.sensor_updated_ms = s_state.humidity_updated_ms = app_ui_tick_ms();
        update_mock_vision(app_ui_tick_ms());
        if ((phase == APP_DEMO_ENTER || phase == APP_DEMO_AI_FAULT) &&
            !s_state.climate.power_on) {
            s_demo_driving = true;
            apply_action(APP_ACTION_POWER);
            s_demo_driving = false;
        } else {
            refresh_climate(app_ui_tick_ms());
        }
        printf("DEMO,source=SIMULATED_SCRIPT,phase=%u,name=%s,wall_ms=%lu\n",
            (unsigned)phase, app_demo_phase_name(phase), (unsigned long)s_demo.elapsed_ms);
        fflush(stdout);
    }
    s_state.simulated = 1U;
    s_state.temperature_source = APP_TEMPERATURE_SIMULATED;
    s_state.simulation_speed = app_demo_speed(&s_demo);
    if (model_ms) { advance_model_time(model_ms); }
}

static void handle_demo_command(uint8_t command)
{
    if (command == APP_UI_DEMO_STOP) {
        stop_demo(true);
    } else if (command == APP_UI_DEMO_TOGGLE) {
        if (s_demo.active) {
            app_demo_toggle_pause(&s_demo);
        } else {
            s_before_demo = s_state;
            s_before_demo_tick = app_ui_tick_ms();
            s_before_ai_online = s_ai_online;
            s_before_als_online = s_als_online;
            s_before_sensor_online = s_sensor_online;
            s_before_skip_samples = s_skip_automatic_samples;
            s_before_mock_vision = s_mock_vision;
            s_before_person_right = s_person_right;
            s_before_mock_jitter = s_mock_jitter;
            s_have_saved_demo = true;
            app_demo_start(&s_demo, demo_step, NULL);
        }
        publish();
    }
}

static void handle_event(const pc_event_t *event)
{
    if (event->type == PC_EVENT_QUIT) { s_running = false; return; }
    if (event->type == PC_EVENT_POINTER) {
        /* Captured releases can arrive outside the window. Still deliver
           them, otherwise a drag outside the preview leaves touch pressed. */
        s_state.touch.x = (uint16_t)(event->x < 0 ? 0 :
                                     event->x > UINT16_MAX ? UINT16_MAX : event->x);
        s_state.touch.y = (uint16_t)(event->y < 0 ? 0 :
                                     event->y > UINT16_MAX ? UINT16_MAX : event->y);
        s_state.touch.pressed = event->pressed ? 1U : 0U;
        s_state.touch.finger_count = s_state.touch.pressed;
        s_state.touch.scan_count++;
        s_state.touch.last_success_ms = app_ui_tick_ms();
        publish();
        /* Read every transition before dequeuing the next event. A fast
           down/up pair therefore reaches the real UI's lv_indev callback. */
        for (lv_indev_t *indev = lv_indev_get_next(NULL); indev;
             indev = lv_indev_get_next(indev)) {
            if (lv_indev_get_type(indev) == LV_INDEV_TYPE_POINTER) {
                lv_indev_read(indev);
            }
        }
        return;
    }
    if (event->type == PC_EVENT_KEY) {
        if (event->key == 'v') { handle_demo_command(APP_UI_DEMO_TOGGLE); return; }
        if (event->key == 'x') { handle_demo_command(APP_UI_DEMO_STOP); return; }
        if (event->key == PC_KEY_ESCAPE) { s_running = false; return; }
        if (s_demo.active && event->key != 'r') {
            switch (event->key) {
            case '0': case '1': case '2': case 'j': case 'k': case 'g':
            case 'd': case 'b': case 't': case 'h':
            case 'c': case 'a': case 's': case 'f': case 'p': case 'm':
            case PC_KEY_DOWN: case PC_KEY_UP: stop_demo(true); break;
            default: break;
            }
        }
        float delta = event->shift ? -1.0f : 1.0f;
        switch (event->key) {
        case '0': s_state.people_count = 0; break;
        case '1': s_state.people_count = 1; break;
        case '2': s_state.people_count = 2; break;
        case 'j': s_person_right = false; break;
        case 'k': s_person_right = true; break;
        case 'g': s_mock_jitter = !s_mock_jitter; break;
        case 'd': s_state.als = 50U; break;
        case 'b': s_state.als = 250U; break;
        case 't': s_state.temperature += delta; break;
        case 'h':
            s_state.humidity += delta;
            if (s_state.humidity < 0.0f) { s_state.humidity = 0.0f; }
            if (s_state.humidity > 100.0f) { s_state.humidity = 100.0f; }
            break;
        case 'c': s_ai_online = !s_ai_online; break;
        case 'a': s_als_online = !s_als_online; break;
        case 's': s_sensor_online = !s_sensor_online; break;
        case 'f': s_state.simulation_speed = s_state.simulation_speed == 1U ? 60U : 1U; break;
        case 'r':
            reset_simulation();
            /* Cancel an in-flight mouse gesture before the old release can
               click a button against the freshly reset state. */
            for (lv_indev_t *indev = lv_indev_get_next(NULL); indev;
                 indev = lv_indev_get_next(indev)) {
                lv_indev_reset(indev, NULL);
            }
            break;
        case 'p': apply_action(app_input_key_to_action(APP_INPUT_KEY_WKUP)); break;
        case 'm': apply_action(app_input_key_to_action(APP_INPUT_KEY0)); break;
        case PC_KEY_DOWN: apply_action(app_input_key_to_action(APP_INPUT_KEY1)); break;
        case PC_KEY_UP: apply_action(app_input_key_to_action(APP_INPUT_KEY2)); break;
        default: break;
        }
    }
}

static void process_model_tick(void)
{
    uint32_t now = app_ui_tick_ms();
    if (!s_skip_automatic_samples && now - s_last_sample_tick >= 500U) {
        s_state.people_valid = s_ai_online ? 1U : 0U;
        s_state.als_valid = s_als_online ? 1U : 0U;
        s_state.sensor_valid = s_sensor_online ? 1U : 0U;
        s_state.humidity_valid = s_sensor_online ? 1U : 0U;
        if (s_ai_online) { s_state.people_updated_ms = now; }
        if (s_als_online) { s_state.als_updated_ms = now; }
        if (s_sensor_online) {
            s_state.sensor_updated_ms = now;
            s_state.humidity_updated_ms = now;
        }
        update_mock_vision(now);
        s_last_sample_tick = now;
    }
    if (now - s_last_climate_tick >= 500U) {
        refresh_climate(now);
        s_last_climate_tick = now;
        uint8_t output = app_climate_output_mask(&s_state.climate);
        if (!s_have_log || s_log_reason != s_state.climate.reason || s_log_output != output) {
            printf("CLIMATE,%lu,%s,request=0x%02X,people=%ld,temp=%.1f,sim_speed=%lu\n",
                (unsigned long)now, app_climate_reason_name(s_state.climate.reason),
                output, (long)s_state.people_count, (double)s_state.temperature,
                (unsigned long)s_state.simulation_speed);
            fflush(stdout);
            s_have_log = true; s_log_reason = s_state.climate.reason; s_log_output = output;
        }
    }
}

static void advance_model_time(uint64_t milliseconds)
{
    /* Match the same 500 ms sample/control boundaries at every speed. A wall
       stall cannot become a spurious three-second hole in simulated data. */
    while (milliseconds != 0U) {
        uint32_t until_sample = 500U - (app_ui_tick_ms() - s_last_sample_tick);
        uint32_t until_control = 500U - (app_ui_tick_ms() - s_last_climate_tick);
        uint32_t slice = until_sample < until_control ? until_sample : until_control;
        if (slice == 0U || slice > 500U) { slice = 500U; }
        if (milliseconds < slice) { slice = (uint32_t)milliseconds; }
        pc_port_advance_time(slice);
        process_model_tick();
        milliseconds -= slice;
    }
}

static void process_frame(void)
{
    pc_event_t event;
    while (pc_port_poll_event(&event)) { handle_event(&event); }
    process_model_tick();
    if (s_mock_vision && app_ui_tick_ms() - s_state.vision.captured_ms >= 33U) {
        update_mock_vision(app_ui_tick_ms());
    }
    publish();
    app_ui_process();
}

static void run_wall_frame(uint32_t wall_elapsed_ms)
{
    apply_wall_delta(wall_elapsed_ms);
    process_frame();
}

static void advance(uint32_t elapsed)
{
    while (elapsed != 0U) {
        uint32_t slice = elapsed > 500U ? 500U : elapsed;
        if (s_demo.active) { run_wall_frame(slice); }
        else { advance_model_time(slice); process_frame(); }
        elapsed -= slice;
    }
}

static lv_obj_t *find_button(lv_obj_t *object, const char *text)
{
    if (lv_obj_has_flag(object, LV_OBJ_FLAG_HIDDEN)) { return NULL; }
    if (lv_obj_check_type(object, &lv_label_class) &&
        strcmp(lv_label_get_text(object), text) == 0) {
        lv_obj_t *parent = lv_obj_get_parent(object);
        if (parent && lv_obj_check_type(parent, &lv_button_class)) { return parent; }
    }
    uint32_t children = lv_obj_get_child_count(object);
    /* A visible modal can contain the same caption as a lower HOME control.
       Locate the topmost candidate in the actual drawing order. */
    for (uint32_t i = children; i > 0U; i--) {
        lv_obj_t *found = find_button(lv_obj_get_child(object, (int32_t)(i - 1U)), text);
        if (found) { return found; }
    }
    return NULL;
}

static lv_obj_t *find_label_containing(lv_obj_t *object, const char *text)
{
    if (lv_obj_check_type(object, &lv_label_class) &&
        strstr(lv_label_get_text(object), text) != NULL) { return object; }
    uint32_t children = lv_obj_get_child_count(object);
    for (uint32_t i = 0U; i < children; i++) {
        lv_obj_t *found = find_label_containing(lv_obj_get_child(object, (int32_t)i), text);
        if (found) { return found; }
    }
    return NULL;
}

static lv_obj_t *button_caption(lv_obj_t *button, const char *text)
{
    if (!button) { return NULL; }
    for (uint32_t i = 0U; i < lv_obj_get_child_count(button); i++) {
        lv_obj_t *child = lv_obj_get_child(button, (int32_t)i);
        if (lv_obj_check_type(child, &lv_label_class) &&
            strcmp(lv_label_get_text(child), text) == 0) { return child; }
    }
    return NULL;
}

static lv_obj_t *find_widget_index(lv_obj_t *object, const lv_obj_class_t *type,
                                    uint32_t *index)
{
    if (lv_obj_check_type(object, type)) {
        if (*index == 0U) { return object; }
        (*index)--;
    }
    for (uint32_t i = 0U; i < lv_obj_get_child_count(object); i++) {
        lv_obj_t *found = find_widget_index(lv_obj_get_child(object, (int32_t)i), type, index);
        if (found) { return found; }
    }
    return NULL;
}

static lv_obj_t *find_visible_label(lv_obj_t *object, const char *text)
{
    if (lv_obj_has_flag(object, LV_OBJ_FLAG_HIDDEN)) { return NULL; }
    if (lv_obj_check_type(object, &lv_label_class) &&
        strstr(lv_label_get_text(object), text)) { return object; }
    for (uint32_t i = 0U; i < lv_obj_get_child_count(object); i++) {
        lv_obj_t *found = find_visible_label(lv_obj_get_child(object, (int32_t)i), text);
        if (found) { return found; }
    }
    return NULL;
}

static lv_obj_t *find_sensor_reading(bool humidity)
{
    lv_obj_t *caption = find_visible_label(lv_screen_active(), humidity ? "实测湿度" : "实测温度");
    if (!caption) { caption = find_visible_label(lv_screen_active(), humidity ? "上次湿度" : "上次温度"); }
    lv_obj_t *card = caption ? lv_obj_get_parent(caption) : NULL;
    if (!card) { return NULL; }
    for (uint32_t i = 0U; i < lv_obj_get_child_count(card); i++) {
        lv_obj_t *child = lv_obj_get_child(card, (int32_t)i);
        if (lv_obj_check_type(child, &lv_label_class) &&
            lv_obj_get_style_text_font(child, LV_PART_MAIN) == &lv_font_montserrat_40) { return child; }
    }
    return NULL;
}

static bool sensor_reading_is(bool humidity, const char *value)
{
    lv_obj_t *reading = find_sensor_reading(humidity);
    return reading && strcmp(lv_label_get_text(reading), value) == 0;
}

static lv_obj_t *find_widget(const lv_obj_class_t *type, uint32_t index)
{
    return find_widget_index(lv_screen_active(), type, &index);
}

static lv_obj_t *find_record_thumbnail(void)
{
    /* HOME now contains background and icon images as well. Select the
       journal's established 128px image, including its hidden no-image state,
       rather than accidentally examining the first decorative asset. */
    lv_obj_t *image;
    for (uint32_t i = 0U; (image = find_widget(&lv_image_class, i)) != NULL; i++) {
        if (lv_obj_get_width(image) == APP_EVENT_THUMB_WIDTH &&
            lv_obj_get_height(image) == APP_EVENT_THUMB_HEIGHT) { return image; }
    }
    return NULL;
}

static int32_t chart_last(lv_obj_t *chart, uint32_t series_index)
{
    lv_chart_series_t *series = lv_chart_get_series_next(chart, NULL);
    while (series && series_index--) { series = lv_chart_get_series_next(chart, series); }
    if (!series) { return LV_CHART_POINT_NONE; }
    int32_t *values = lv_chart_get_y_array(chart, series);
    return values[lv_chart_get_point_count(chart) - 1U];
}

static bool chart_has_value(lv_obj_t *chart, int32_t value)
{
    lv_chart_series_t *series = lv_chart_get_series_next(chart, NULL);
    if (!series) { return false; }
    int32_t *values = lv_chart_get_y_array(chart, series);
    for (uint32_t i = 0U; i < lv_chart_get_point_count(chart); i++) {
        if (values[i] == value) { return true; }
    }
    return false;
}

static bool camera_preview_uncovered(void)
{
    const uint16_t *foreground = (const uint16_t *)app_lcd_get_fg_draw_buffer();
    /* This interior is occupied by the full diagnostic panel when open. A
       zero alpha after closing proves the preview is restored, not just that
       the LVGL object carries its HIDDEN flag. */
    for (uint32_t y = 88U; y < 390U; y++) {
        for (uint32_t x = 408U; x < 780U; x++) {
            if ((foreground[y * LCD_FG_WIDTH + x] & 0xF000U) != 0U) { return false; }
        }
    }
    return true;
}

static bool click(const char *text)
{
    lv_obj_update_layout(lv_screen_active());
    lv_obj_t *button = find_button(lv_screen_active(), text);
    if (!button) { fprintf(stderr, "FAIL: visible button %s missing\n", text); return false; }
    lv_area_t area;
    lv_obj_get_coords(button, &area);
    int32_t x = (area.x1 + area.x2) / 2;
    int32_t y = (area.y1 + area.y2) / 2;
    if (x < 0 || x >= LCD_FG_WIDTH || y < 0 || y >= LCD_FG_HEIGHT ||
        !pc_port_push_pointer(x, y, true) || !pc_port_push_pointer(x, y, false)) {
        fprintf(stderr, "FAIL: cannot enqueue mouse click for %s\n", text);
        return false;
    }
    process_frame();
    advance(220U);
    return true;
}

static bool check(bool condition, const char *message)
{
    fprintf(condition ? stdout : stderr, "%s: %s\n", condition ? "PASS" : "FAIL", message);
    return condition;
}

static bool click_point(int32_t x, int32_t y)
{
    if (!pc_port_push_pointer(x, y, true) || !pc_port_push_pointer(x, y, false)) { return false; }
    process_frame(); advance(220U);
    return true;
}

static bool press_key(int32_t key, bool shift)
{
    if (!pc_port_push_key(key, shift)) {
        fprintf(stderr, "FAIL: cannot enqueue keyboard input: %s\n", pc_port_last_error());
        return false;
    }
    process_frame();
    return true;
}

static bool save_image(const char *directory, const char *name)
{
    char path[2048];
    int length = snprintf(path, sizeof(path), "%s/%s.bmp", directory, name);
    if (length < 0 || (size_t)length >= sizeof(path) || !pc_port_save_bmp(path)) {
        fprintf(stderr, "FAIL: BMP export %s: %s\n", name, pc_port_last_error());
        return false;
    }
    printf("Saved: %s\n", path);
    return true;
}

static bool inspect_home_details(const char *first, const char *second, const char *message)
{
    unsigned int actions = s_action_count;
    if (!click("详情") || !check(find_visible_label(lv_screen_active(), first) &&
        (!second || find_visible_label(lv_screen_active(), second)), message) ||
        !click("完成")) { return false; }
    return check(s_action_count == actions,
        "opening and closing real HOME strategy details submit no control action");
}

static bool check_request_charts(lv_obj_t *const charts[4], uint8_t mask)
{
    for (unsigned int i = 0U; i < 4U; i++) {
        if (!check(lv_chart_get_type(charts[i]) == LV_CHART_TYPE_BAR &&
                   chart_last(charts[i], 0U) == (int32_t)((mask >> i) & 1U),
                   "request BAR plots the matching real software-request bit")) { return false; }
    }
    return check(chart_last(charts[0], 1U) == LV_CHART_POINT_NONE,
                 "request view removes temperature setpoint points from the shared second series");
}

static bool check_request_pixels(lv_obj_t *const charts[4], uint8_t mask, bool all_history_off)
{
    const uint16_t *foreground = (const uint16_t *)app_lcd_get_fg_draw_buffer();
    if (!foreground) { return check(false, "request charts have a rendered foreground buffer"); }
    for (unsigned int i = 0U; i < 4U; i++) {
        lv_area_t plot;
        lv_obj_get_coords(charts[i], &plot);
        lv_chart_series_t *series = lv_chart_get_series_next(charts[i], NULL);
        lv_color_t color = lv_chart_get_series_color(charts[i], series);
        uint16_t expected = (uint16_t)(0xF000U | ((uint16_t)(color.red & 0xF0U) << 4U) |
                                      (color.green & 0xF0U) | (color.blue >> 4U));
        unsigned int pixels = 0U;
        /* Ignore the baseline and borders. A 0-valued sample has no tall bar,
           while an ON sample must render in the series color above it. */
        for (int32_t y = plot.y1 + 2; y < plot.y2 - 5; y++) {
            for (int32_t x = plot.x1 + 1; x < plot.x2; x++) {
                if (x >= 0 && x < LCD_FG_WIDTH && y >= 0 && y < LCD_FG_HEIGHT &&
                    foreground[(uint32_t)y * LCD_FG_WIDTH + (uint32_t)x] == expected) { pixels++; }
            }
        }
        printf("Request chart R%u rendered series pixels: %u\n", i + 1U, pixels);
        if ((mask & (1U << i)) &&
            !check(pixels >= 20U, "ON request renders visible series-colored BAR pixels above baseline")) {
            return false;
        }
        if (all_history_off &&
            !check(pixels == 0U, "all-OFF request history renders no false ON pixels")) { return false; }
    }
    return true;
}

static bool check_control_views(lv_obj_t *const charts[4], const char *directory)
{
    int32_t *arrays[5];
    for (unsigned int i = 0U; i < 4U; i++) {
        arrays[i] = lv_chart_get_y_array(charts[i], lv_chart_get_series_next(charts[i], NULL));
    }
    lv_chart_series_t *actual = lv_chart_get_series_next(charts[0], NULL);
    lv_chart_series_t *target = lv_chart_get_series_next(charts[0], actual);
    arrays[4] = lv_chart_get_y_array(charts[0], target);
    unsigned int actions = s_action_count;
    app_climate_state_t climate = s_state.climate;
    float temperature = s_state.temperature, humidity = s_state.humidity;
    int32_t people = s_state.people_count;
    uint16_t als = s_state.als;
    uint32_t speed = s_state.simulation_speed;
    for (unsigned int repeat = 0U; repeat < 4U; repeat++) {
        if (!click("控制请求") ||
            !check(find_label_containing(lv_screen_active(), "R1 制冷请求 / 0..1") &&
                   find_label_containing(lv_screen_active(), "R2 风机请求 / 0..1") &&
                   find_label_containing(lv_screen_active(), "R3 摆风请求 / 0..1") &&
                   find_label_containing(lv_screen_active(), "R4 灯光请求 / 0..1"),
                   "control view labels all four request channels") ||
            !check_request_charts(charts, 0U) || !click("环境趋势")) { return false; }
        for (unsigned int i = 0U; i < 4U; i++) {
            lv_obj_t *current = find_widget(&lv_chart_class, i);
            if (!check(current == charts[i] && lv_chart_get_type(current) == LV_CHART_TYPE_LINE &&
                       lv_chart_get_y_array(current, lv_chart_get_series_next(current, NULL)) == arrays[i],
                       "view switching reuses the same chart object and sample array")) { return false; }
        }
        if (!check(!find_widget(&lv_chart_class, 4U) &&
                   lv_chart_get_y_array(charts[0], target) == arrays[4] &&
                   chart_last(charts[0], 0U) == 270 && chart_last(charts[0], 1U) == 260,
                   "environment view restores the existing temperature and setpoint series")) { return false; }
        app_climate_state_t after = s_state.climate;
        /* Rendering advances virtual time; the normal control heartbeat may
           move its timestamp without a user action. The remaining model is
           unchanged while this occupied, powered-off fixture is stable. */
        after.step_updated_ms = climate.step_updated_ms;
        if (!check(s_action_count == actions && memcmp(&climate, &after, sizeof(climate)) == 0 &&
                   s_state.temperature == temperature && s_state.humidity == humidity &&
                   s_state.people_count == people && s_state.als == als &&
                   s_state.simulation_speed == speed && s_state.relay_mask == 0U,
                   "DATA toggles never modify controls, measurements, request mask or simulation speed")) {
            return false;
        }
    }
    if (!click("控制请求") || !check_request_pixels(charts, 0U, true) ||
        !save_image(directory, "v9_2_requests_off")) { return false; }
    /* Drive the shared action/control API, allowing production history to
       record each request state. Request validity is always known here;
       missing-request samples are covered by the pure history tests. */
    s_state.temperature = 25.0f;
    apply_action(APP_ACTION_POWER);
    advance(5500U);
    if (!check(app_climate_output_mask(&s_state.climate) == 0x0AU,
               "cold occupied powered fixture requests fan and lamp only") ||
        !check_request_charts(charts, 0x0AU)) { return false; }
    apply_action(APP_ACTION_SWING_LR);
    advance(5500U);
    if (!check_request_charts(charts, 0x0EU)) { return false; }
    s_state.temperature = 29.0f;
    advance(APP_COOLING_MIN_OFF_MS + 5500U);
    if (!check(s_state.climate.cooling_request, "request history uses the real delayed cooling decision") ||
        !check_request_charts(charts, 0x0FU) ||
        !check_request_pixels(charts, 0x0FU, false) ||
        !save_image(directory, "v9_2_requests_active")) { return false; }
    if (!press_key('s', false)) { return false; } advance(5500U);
    if (!check(!s_state.climate.sensor_fresh && !s_state.climate.cooling_request,
               "temperature failure cancels cooling through the production controller") ||
        !check_request_charts(charts, 0x0EU) || !click("环境趋势") ||
        !check(chart_last(charts[0], 0U) == LV_CHART_POINT_NONE &&
               chart_last(charts[0], 1U) == 260 &&
               find_label_containing(lv_screen_active(), "温差 未知"),
               "invalid temperature remains a chart gap and unknown difference while target stays valid") ||
        !save_image(directory, "v9_2_environment_temperature_failure") || !click("控制请求") ||
        !check_request_charts(charts, 0x0EU) ||
        !save_image(directory, "v9_2_requests_temperature_failure") || !press_key('s', false)) { return false; }
    advance(APP_COOLING_MIN_OFF_MS + 5500U);
    if (!check_request_charts(charts, 0x0FU)) { return false; }
    apply_action(APP_ACTION_LAMP);
    advance(5500U);
    if (!check_request_charts(charts, 0x07U)) { return false; }
    apply_action(APP_ACTION_SWING_LR);
    advance(5500U);
    if (!check_request_charts(charts, 0x03U)) { return false; }
    s_state.temperature = 25.0f;
    advance(5500U);
    if (!check_request_charts(charts, 0x02U)) { return false; }
    apply_action(APP_ACTION_POWER);
    advance(5500U);
    if (!check_request_charts(charts, 0U) || !click("环境趋势")) { return false; }
    reset_simulation(); advance(5500U);
    return check(chart_last(charts[0], 0U) == 270 && chart_last(charts[0], 1U) == 260 &&
                 chart_last(charts[1], 0U) == 550 && chart_last(charts[2], 0U) == 250 &&
                 chart_last(charts[3], 0U) == 1,
                 "environment mode retains all original valid-data mappings after control history");
}

static bool check_debug_refresh_throttle(lv_obj_t *panel)
{
    lv_obj_t *text = find_label_containing(panel, "CAM CFG");
    char frozen[3072];
    if (!text) { return check(false, "full diagnostic text exists for refresh checks"); }
    (void)snprintf(frozen, sizeof(frozen), "%s", lv_label_get_text(text));
    /* click() has already advanced 220 ms after the forced opening refresh. */
    advance(400U);
    if (!check(strcmp(frozen, lv_label_get_text(text)) == 0,
        "full diagnostics remain unchanged before the one-second refresh")) { return false; }
    advance(600U);
    if (!check(strcmp(frozen, lv_label_get_text(text)) != 0,
        "full diagnostics refresh after one second")) { return false; }

    (void)snprintf(frozen, sizeof(frozen), "%s", lv_label_get_text(text));
    if (!pc_port_push_pointer(600, 240, true)) { return false; } process_frame();
    advance(1500U);
    if (!check(s_state.touch.pressed && strcmp(frozen, lv_label_get_text(text)) == 0,
        "holding a real SDL pointer freezes the large diagnostic label")) { return false; }
    if (!pc_port_push_pointer(600, 240, false)) { return false; } process_frame();
    advance(220U);
    if (!check(!s_state.touch.pressed && strcmp(frozen, lv_label_get_text(text)) != 0,
        "diagnostic updates resume after pointer release")) { return false; }

    if (!pc_port_push_pointer(600, 350, true)) { return false; } process_frame();
    advance(50U);
    if (!pc_port_push_pointer(600, 150, true)) { return false; } process_frame();
    if (!check(lv_obj_is_scrolling(panel), "SDL pointer drag starts real LVGL panel scrolling")) { return false; }
    advance(1200U);
    (void)snprintf(frozen, sizeof(frozen), "%s", lv_label_get_text(text));
    /* Give the final movement a nonzero velocity before release, even if the
       event-mode input timer sampled a stationary pointer during the hold. */
    if (!pc_port_push_pointer(600, 110, true)) { return false; } process_frame();
    if (!pc_port_push_pointer(600, 110, false)) { return false; } process_frame();
    if (!check(!s_state.touch.pressed && lv_obj_is_scrolling(panel),
        "released SDL drag retains LVGL scroll momentum")) { return false; }
    advance(200U);
    if (!check(strcmp(frozen, lv_label_get_text(text)) == 0,
        "scroll momentum also freezes the large diagnostic label")) { return false; }
    for (uint32_t elapsed = 0U; lv_obj_is_scrolling(panel) && elapsed < 5000U; elapsed += 50U) {
        advance(50U);
    }
    if (!check(!lv_obj_is_scrolling(panel), "LVGL scrolling completes normally")) { return false; }
    advance(220U);
    if (!check(strcmp(frozen, lv_label_get_text(text)) != 0,
        "full diagnostics resume after scrolling finishes")) { return false; }
    lv_obj_scroll_to_y(panel, 0, LV_ANIM_OFF);
    advance(220U);
    return true;
}

static bool check_wall_scheduler(void)
{
    app_climate_state_t reference[3];
    const uint32_t elapsed[3] = {1500U, 600000U, 1200000U};
    for (uint32_t speed = 1U; speed <= 60U; speed += 59U) {
        reset_simulation();
        s_state.simulation_speed = speed;
        s_state.people_count = 0;
        apply_action(APP_ACTION_POWER);
        for (unsigned i = 0U; i < 3U; i++) {
            /* This is the actual interactive main-loop scheduler, including
               model catch-up and one render per supplied wall interval. */
            run_wall_frame(elapsed[i] / speed);
            if (speed == 1U) {
                reference[i] = s_state.climate;
            } else if (!check(
                s_state.climate.presence == reference[i].presence &&
                s_state.climate.absence_ms == reference[i].absence_ms &&
                s_state.climate.power_on == reference[i].power_on &&
                s_state.climate.effective_eco == reference[i].effective_eco &&
                s_state.climate.cooling_request == reference[i].cooling_request &&
                s_state.climate.reason == reference[i].reason,
                "actual wall scheduler gives equal 1x/60x occupancy, ECO and shutdown")) {
                return false;
            }
        }
    }
    reset_simulation();
    s_state.simulation_speed = 60U;
    s_state.people_count = 0;
    apply_action(APP_ACTION_POWER);
    run_wall_frame(10025U); /* 601500 virtual ms: ten-minute ECO is active. */
    uint32_t before = s_state.climate.absence_ms;
    if (!check(s_state.climate.effective_eco, "60x scheduler reaches automatic ECO")) { return false; }
    const uint32_t hiccups[] = {50U, 100U, 500U};
    for (unsigned i = 0U; i < sizeof(hiccups) / sizeof(hiccups[0]); i++) {
        run_wall_frame(hiccups[i]);
        before += hiccups[i] * 60U;
        if (!check(s_state.climate.presence == APP_PRESENCE_ABSENT &&
                   s_state.climate.absence_ms == before && s_state.climate.effective_eco,
                   "60x wall jitter preserves continuous observations and absence timer")) { return false; }
    }
    s_ai_online = false;
    run_wall_frame(25U);
    if (!check(!s_state.climate.people_fresh &&
               s_state.climate.presence == APP_PRESENCE_UNKNOWN &&
               s_state.climate.absence_ms == 0U,
               "fixed-step catch-up preserves real simulated AI faults")) { return false; }
    s_ai_online = true;
    run_wall_frame(25U);
    if (!check(s_state.climate.presence == APP_PRESENCE_ABSENT,
               "AI recovery is confirmed after fixed-step catch-up")) { return false; }
    reset_simulation();
    return true;
}

static bool check_detection_overlay(const char *directory)
{
    s_mock_vision = false; /* Explicit fixtures must not be replaced by samples. */
    s_state.vision.valid = 0U; publish(); advance(33U); /* Begin a fresh stable track. */
    s_state.people_count = 2;
    s_state.vision.valid = 1U;
    s_state.vision.count = 2U;
    s_state.vision.frame_id = 17U;
    s_state.vision.captured_ms = app_ui_tick_ms();
    s_state.vision.completed_ms = app_ui_tick_ms();
    s_state.vision.boxes[0] = (app_ui_detection_t){450U, 120U, 120U, 160U, 0.93f, 0};
    s_state.vision.boxes[1] = (app_ui_detection_t){795U, 470U, 100U, 100U, 0.81f, 0};
    advance(220U);
    lv_obj_t *label = find_label_containing(lv_screen_active(), "93%");
    lv_obj_t *edge_label = find_label_containing(lv_screen_active(), "81%");
    if (!check(label && edge_label, "detection snapshot draws confidence labels")) { return false; }
    lv_obj_t *box = lv_obj_get_parent(label);
    lv_obj_t *edge = lv_obj_get_parent(edge_label);
    const uint16_t *foreground = (const uint16_t *)app_lcd_get_fg_draw_buffer();
    if (!check(lv_obj_get_x(box) == 450 && lv_obj_get_y(box) == 120 &&
               lv_obj_get_width(box) == 120 && lv_obj_get_height(box) == 160 &&
               lv_obj_get_style_bg_opa(box, 0) == LV_OPA_TRANSP &&
               (foreground[150U * LCD_FG_WIDTH + 450U] & 0xF000U) != 0U &&
               (foreground[250U * LCD_FG_WIDTH + 500U] & 0xF000U) == 0U,
               "LCD coordinates are used once, with visible border and transparent interior") ||
        !check(lv_obj_get_width(edge) == 5 && lv_obj_get_height(edge) == 10,
               "out-of-display detection extents are clipped")) { return false; }
    if (!save_image(directory, "debug-detections-test-fixture")) { return false; }
    /* All four new-frame positions must reach LVGL within 50 ms. The old
       200 ms diagnostic cadence could update at most one of these. */
    for (unsigned int i = 0U; i < 4U; i++) {
        int32_t previous_x = lv_obj_get_x(box);
        s_state.vision.boxes[0].x = (uint16_t)(460U + i * 10U);
        s_state.vision.frame_id++;
        s_state.vision.captured_ms = app_ui_tick_ms();
        advance(50U);
        if (!check(lv_obj_get_x(box) > previous_x &&
                   lv_obj_get_x(box) <= (int32_t)s_state.vision.boxes[0].x,
                   "new stable detection frames move within 50 ms rather than the 200 ms diagnostic cadence")) { return false; }
    }
    s_state.vision.boxes[0].x = 450U;
    s_state.vision.frame_id++;
    s_state.vision.captured_ms = app_ui_tick_ms();
    advance(50U);
    int32_t retained_x = lv_obj_get_x(box);
    s_state.vision.boxes[0].x = 100U;
    s_state.vision.frame_id++;
    pc_port_block_fg_access(2U);
    advance(220U);
    process_frame();
    if (!check(lv_obj_get_x(box) == retained_x &&
               (foreground[150U * LCD_FG_WIDTH + (uint32_t)retained_x] & 0xF000U) != 0U,
               "unavailable draw buffer postpones rendering without losing old content")) { return false; }
    advance(220U);
    foreground = (const uint16_t *)app_lcd_get_fg_draw_buffer();
    if (!check((foreground[150U * LCD_FG_WIDTH + (uint32_t)retained_x] & 0xF000U) == 0U,
               "moving a detection clears the previous transparent overlay area")) { return false; }
    s_state.simulated = 0U; /* Explicit test fixture, never a default PC source. */
    s_state.nn_period_ms = 100U;
    s_state.ai.completed_rate_valid = 1U;
    s_state.ai.completed_rate_x10 = 317U;
    advance(220U);
    if (!check(find_label_containing(lv_screen_active(), "AI 31.7 Hz") != NULL,
               "AI rate uses completed-result window, not inverse last period")) { return false; }
    s_state.simulated = 1U;
    s_ai_online = false;
    advance(600U);
    if (!check(lv_obj_has_flag(box, LV_OBJ_FLAG_HIDDEN) &&
               lv_obj_has_flag(edge, LV_OBJ_FLAG_HIDDEN),
               "unknown AI hides every retained detection") ||
        !check(find_label_containing(lv_screen_active(), "AI -- Hz") != NULL,
               "PC preview and stale AI do not display hardware throughput")) { return false; }
    s_ai_online = true;
    s_state.vision.valid = 0U;
    s_state.vision.count = 0U;
    advance(1500U);
    foreground = (const uint16_t *)app_lcd_get_fg_draw_buffer();
    bool clear = check(camera_preview_uncovered() &&
                       (foreground[200U * LCD_FG_WIDTH + 400U] & 0xF000U) == 0U,
                       "invalid results clear all camera overlay boxes and leave the center transparent");
    s_state.people_count = 1;
    s_mock_vision = true;
    return clear;
}

static bool check_v9_pages(const char *directory)
{
    reset_simulation();
    advance(6500U);
    lv_obj_t *home = lv_screen_active();
    lv_obj_t *arc = find_widget(&lv_arc_class, 0U);
    if (!check(arc && lv_obj_get_width(arc) == 252 && lv_obj_get_height(arc) == 252 &&
               lv_obj_has_flag(arc, LV_OBJ_FLAG_CLICKABLE),
               "HOME retains the compact 252px ring and enables direct temperature dragging")) { return false; }
    unsigned int actions = s_action_count;
    lv_area_t arc_area; lv_obj_get_coords(arc, &arc_area);
    int32_t ring_x = (arc_area.x1 + arc_area.x2) / 2;
    if (!pc_port_push_pointer(ring_x, arc_area.y1 + 6, true) ||
        !pc_port_push_pointer(ring_x, arc_area.y1 + 6, false)) { return false; }
    process_frame(); advance(500U);
    if (!check(s_state.climate.target_temperature == 26U && s_action_count == actions,
               "a stationary real SDL ring tap leaves the chosen temperature unchanged") ||
        !click("+") || !click("+") ||
        !check(s_state.climate.target_temperature == 28U && s_action_count == actions + 2U,
               "two real plus clicks send exactly two unified temperature actions") ||
        !click("-") || !click("-") ||
        !check(s_state.climate.target_temperature == 26U && s_action_count == actions + 4U,
               "two real minus clicks use the same authoritative control path") ||
        !save_image(directory, "v9_home") || !click("数据")) { return false; }
    lv_obj_t *charts[4];
    for (unsigned int i = 0U; i < 4U; i++) { charts[i] = find_widget(&lv_chart_class, i); }
    advance(5500U);
    if (!check(lv_screen_active() != home && charts[0] && charts[1] && charts[2] && charts[3] &&
               !find_widget(&lv_chart_class, 4U), "DATA lazily creates exactly four real LVGL charts") ||
        !check(find_label_containing(lv_screen_active(), "PC 模拟 / 软件请求") &&
               chart_last(charts[0], 0U) == 270 && chart_last(charts[0], 1U) == 260 &&
               chart_last(charts[1], 0U) == 550 && chart_last(charts[2], 0U) == 250 &&
               chart_last(charts[3], 0U) == 1,
               "DATA identifies PC input and plots valid temperature/target/humidity/light/people") ||
        !save_image(directory, "v9_data") || !check_control_views(charts, directory)) { return false; }
    if (!press_key('s', false) || !press_key('a', false) || !press_key('c', false)) { return false; }
    advance(5500U);
    if (!check(chart_last(charts[0], 0U) == LV_CHART_POINT_NONE &&
               chart_last(charts[1], 0U) == LV_CHART_POINT_NONE &&
               chart_last(charts[2], 0U) == LV_CHART_POINT_NONE &&
               chart_last(charts[3], 0U) == LV_CHART_POINT_NONE &&
               chart_last(charts[0], 1U) == 260,
               "missing sources leave real chart gaps while the valid setpoint remains") ||
        !save_image(directory, "v9_data_missing")) { return false; }
    s_state.simulated = 0U; /* Explicit board-like fixture, not a hardware measurement. */
    s_state.temperature_source = APP_TEMPERATURE_UNAVAILABLE;
    process_frame(); advance(220U);
    if (!check(find_label_containing(lv_screen_active(), "实板采样 / 软件请求") &&
               chart_last(charts[0], 0U) == LV_CHART_POINT_NONE,
               "board-like fixture labels its source and never invents unavailable temperature") ||
        !click("演示")) { return false; }
    lv_obj_t *start = find_button(lv_screen_active(), "开始回放");
    lv_obj_t *stop = find_button(lv_screen_active(), "停止并恢复");
    if (!check(start && stop && lv_obj_has_state(start, LV_STATE_DISABLED) &&
               lv_obj_has_state(stop, LV_STATE_DISABLED) &&
               find_label_containing(lv_screen_active(), "实板场景引导"),
               "board demonstration page disables automatic simulated input injection")) { return false; }
    reset_simulation(); advance(220U);
    if (!click("开始回放") ||
        !check(s_demo.active && find_label_containing(lv_screen_active(), "模拟回放 / 软件请求"),
               "DEMO start button enters the same marked script as keyboard V") ||
        !save_image(directory, "v9_demo") || !click("暂停回放") ||
        !check(s_demo.paused && find_button(lv_screen_active(), "继续回放"),
               "DEMO pause button displays resume state")) { return false; }
    uint32_t frozen_elapsed = s_demo.elapsed_ms;
    run_wall_frame(5000U);
    if (!check(s_demo.elapsed_ms == frozen_elapsed, "DEMO UI pause freezes script duration") ||
        !click("停止并恢复") || !check(!s_demo.active && !s_state.climate.power_on,
               "DEMO stop button restores the pre-scene manual controls") ||
        !click("数据") || !click("视觉")) { return false; }
    advance(2000U);
    const uint16_t *foreground = (const uint16_t *)app_lcd_get_fg_draw_buffer();
    lv_obj_t *person_label = find_label_containing(lv_screen_active(), "93%");
    if (!check(person_label && lv_obj_get_x(lv_obj_get_parent(person_label)) == 200 &&
               !find_label_containing(lv_screen_active(), "区域 A") &&
               !find_label_containing(lv_screen_active(), "区域 B") &&
               !find_label_containing(lv_screen_active(), "送风建议") &&
               !find_label_containing(lv_screen_active(), "分区") &&
               (foreground[200U * LCD_FG_WIDTH + 400U] & 0xF000U) == 0U,
               "DEBUG retains detection while removing fixed zone captions and center divider") ||
        !save_image(directory, "v9_2_vision")) { return false; }
    if (!press_key('k', false)) { return false; } advance(2000U);
    person_label = find_label_containing(lv_screen_active(), "93%");
    if (!check(person_label && lv_obj_get_x(lv_obj_get_parent(person_label)) == 500,
               "K still moves the mock detection without geographic control implications") ||
        !press_key('2', false)) { return false; } advance(2000U);
    if (!check(s_state.people_count == 2 && s_state.vision.valid && s_state.vision.count == 2U &&
               find_label_containing(lv_screen_active(), "93%") &&
               find_label_containing(lv_screen_active(), "91%"),
               "mock people count matches two real overlay fixtures") ||
        !save_image(directory, "v9_2_vision_two_people") || !press_key('c', false)) { return false; }
    advance(600U);
    person_label = find_label_containing(lv_screen_active(), "93%");
    if (!check(person_label && lv_obj_has_flag(lv_obj_get_parent(person_label), LV_OBJ_FLAG_HIDDEN) &&
               camera_preview_uncovered(),
               "AI fault clears retained detection overlays and leaves the preview transparent") ||
        !press_key('c', false) || !press_key('1', false) || !press_key('j', false)) { return false; }
    advance(2000U);
    person_label = find_label_containing(lv_screen_active(), "93%");
    if (!check(person_label && !lv_obj_has_flag(lv_obj_get_parent(person_label), LV_OBJ_FLAG_HIDDEN) &&
               lv_obj_get_x(lv_obj_get_parent(person_label)) == 200 && s_state.vision.count == 1U,
               "AI recovery restores one fresh detection from the same mock fixture") ||
        !click("返回主页") || !click("数据") || !click("主页") ||
        !check(lv_screen_active() == home &&
               !find_label_containing(home, "分区") && !find_label_containing(home, "送风建议"),
               "HOME preserves context entry without removed zone or direction suggestions") ||
        !inspect_home_details("温差 +1.0 C", "ms前",
               "real strategy details retain the measured temperature delta and sensing age")) { return false; }
    reset_simulation();
    advance(1500U);
    if (!click("数据") || !click("演示") || !click("开始回放") ||
        !click("数据")) { return false; }
    /* Drive the real script in small wall slices and publish each result, as
       the interactive loop does. These are observed values, never fabricated
       chart points. At 21 s its occupied/warm/dark/absent history still fits
       the bounded five-minute window; the later view verifies actual ECO. */
    while (s_demo.elapsed_ms < 21000U) {
        uint32_t slice = 21000U - s_demo.elapsed_ms;
        /* At 60x, 20 wall ms is 1200 model ms, below the three-second data
           lifetime. A 100 ms render gap would conservatively become unknown. */
        run_wall_frame(slice > 20U ? 20U : slice);
    }
    if (!check(find_label_containing(lv_screen_active(), "模拟回放 / 软件请求") &&
               chart_has_value(charts[0], 250) && chart_has_value(charts[0], 290) &&
               chart_has_value(charts[2], 250) && chart_has_value(charts[2], 50) &&
               chart_has_value(charts[3], 1) && chart_has_value(charts[3], 0),
               "DATA charts retain observed entry/warm/dark/absence transitions from real script") ||
        !save_image(directory, "v9_data_history")) { return false; }
    while (s_demo.elapsed_ms < 27700U) {
        uint32_t slice = 27700U - s_demo.elapsed_ms;
        run_wall_frame(slice > 20U ? 20U : slice);
    }
    if (!check(s_state.climate.auto_energy_saving && s_state.climate.power_on,
               "small-step DATA script actually reaches ten-minute automatic ECO") ||
        !save_image(directory, "v9_data_eco") || !press_key('x', false) ||
        !click("主页")) { return false; }
    reset_simulation(); advance(1500U);
    return true;
}

static bool check_demo_scene(const char *directory)
{
    reset_simulation();
    s_state.temperature = 26.5f;
    s_state.humidity = 63.0f;
    s_state.people_count = 0;
    s_state.als = 75U;
    s_state.simulation_speed = 60U;
    apply_action(APP_ACTION_TEMP_DOWN);
    apply_action(APP_ACTION_TEMP_DOWN);
    apply_action(APP_ACTION_FAN_4);
    apply_action(APP_ACTION_MODE_NEXT);
    apply_action(APP_ACTION_POWER);
    apply_action(APP_ACTION_SWING_LR);
    apply_action(APP_ACTION_ECO_TOGGLE);
    apply_action(APP_ACTION_LAMP);
    if (!press_key('v', false) ||
        !check(s_demo.active && s_demo.phase == APP_DEMO_ENTER &&
               s_state.simulated && s_state.temperature_source == APP_TEMPERATURE_SIMULATED,
               "V starts explicitly simulated script through the normal control model")) { return false; }
    run_wall_frame(6000U);
    if (!press_key('v', false)) { return false; }
    uint32_t paused_tick = app_ui_tick_ms();
    uint32_t paused_elapsed = s_demo.elapsed_ms;
    app_climate_state_t paused_climate = s_state.climate;
    run_wall_frame(10000U);
    if (!check(s_demo.paused && s_demo.elapsed_ms == paused_elapsed &&
               app_ui_tick_ms() == paused_tick &&
               memcmp(&paused_climate, &s_state.climate, sizeof(paused_climate)) == 0,
               "paused scene freezes script, virtual samples and control timers") ||
        !press_key('v', false)) { return false; }
    run_wall_frame(5999U);
    if (!check(s_demo.phase == APP_DEMO_WARM && s_state.climate.cooling_request,
               "warm scene uses real hysteresis and minimum-off cooling request")) { return false; }
    run_wall_frame(1U);
    if (!check(s_demo.phase == APP_DEMO_NIGHT && s_state.climate.effective_sleep,
               "dark occupied phase activates existing sleep policy")) { return false; }
    run_wall_frame(5000U);
    run_wall_frame(10999U);
    if (!check(s_demo.phase == APP_DEMO_ABSENCE_ECO && s_state.climate.auto_energy_saving &&
               s_state.climate.power_on && s_state.climate.absence_ms >= APP_ABSENCE_ECO_MS,
               "60x sampled absence reaches ten-minute ECO without timestamp shortcuts")) { return false; }
    run_wall_frame(1U);
    run_wall_frame(19999U);
    if (!check(s_demo.phase == APP_DEMO_ABSENCE_OFF && !s_state.climate.power_on &&
               s_state.climate.automatic_off && !app_climate_output_mask(&s_state.climate),
               "continuous observations reach thirty-minute automatic request shutdown")) { return false; }
    run_wall_frame(1U);
    if (!check(s_demo.phase == APP_DEMO_AI_FAULT &&
               s_state.climate.presence == APP_PRESENCE_UNKNOWN &&
               s_state.climate.reason == APP_REASON_AI_UNKNOWN && !s_state.climate.absence_ms,
               "AI fault is unknown occupancy, never fictitious absence")) { return false; }
    run_wall_frame(4000U);
    run_wall_frame(3999U);
    if (!check(s_demo.phase == APP_DEMO_RECOVER && s_state.climate.people_fresh &&
               s_state.climate.presence == APP_PRESENCE_PRESENT,
               "recovery passes existing occupancy confirmation")) { return false; }
    run_wall_frame(1U);
    if (!check(!s_demo.active && s_demo.phase == APP_DEMO_COMPLETE &&
               s_demo.elapsed_ms == APP_DEMO_DURATION_MS &&
               s_state.temperature == 26.5f && s_state.humidity == 63.0f &&
               s_state.people_count == 0 && s_state.als == 75U &&
               s_state.simulation_speed == 60U && s_state.climate.power_on &&
               s_state.climate.target_temperature == 24U && s_state.climate.fan_speed == 4U &&
               s_state.climate.mode == APP_CLIMATE_MODE_MANUAL_COOL &&
               s_state.climate.swing_lr && s_state.climate.manual_energy_saving &&
               !s_state.climate.lamp_on && !s_state.climate.absence_ms &&
               s_state.climate.sensor_fresh && !s_state.climate.cooling_request,
               "completed script restores manual environment/preferences and restarts timers")) { return false; }
    if (!save_image(directory, "scene-complete-manual-restored")) { return false; }
    if (!press_key('v', false)) { return false; }
    run_wall_frame(20000U);
    if (!press_key('x', false) ||
        !check(!s_demo.active && s_demo.phase == APP_DEMO_IDLE &&
               s_state.temperature == 26.5f && s_state.climate.target_temperature == 24U &&
               !s_state.climate.absence_ms,
               "X cancels mid-absence and restores original manual state")) { return false; }
    if (!press_key('v', false)) { return false; }
    run_wall_frame(5000U);
    if (!press_key('t', false) ||
        !check(!s_demo.active && s_state.temperature == 27.5f,
               "manual input cancels script, restores baseline, then applies requested change")) { return false; }
    if (!press_key('v', false) || !press_key('r', false) ||
        !check(!s_demo.active && !s_state.climate.power_on && s_state.temperature == 27.0f,
               "R resets an active scene to the original preview defaults")) { return false; }
    return true;
}

static bool check_dht_presentation(const char *directory)
{
    reset_simulation();
    advance(1500U);
    s_skip_automatic_samples = true;
    s_state.simulated = 0U; /* Board-like fixture; never an actual measurement. */
    s_state.temperature_source = APP_TEMPERATURE_DHT11;
    s_state.sensor_valid = s_state.humidity_valid = 0U;
    memset(&s_state.dht, 0, sizeof(s_state.dht));
    refresh_climate(app_ui_tick_ms());
    publish(); advance(220U);
    if (!check(find_visible_label(lv_screen_active(), "等待首次采样"),
               "DHT with no successful sample waits without fabricated values")) { return false; }
    s_state.dht.attempts = s_state.dht.failures = 1U;
    s_state.dht.last_status = 4U;
    s_state.dht.retry_interval_ms = 2000U;
    publish(); advance(220U);
    if (!check(find_visible_label(lv_screen_active(), "首次采样失败 E4"),
               "DHT initial failure is distinct from never attempted")) { return false; }
    s_state.temperature = 27.0f;
    s_state.humidity = 55.0f;
    s_state.dht.has_sample = 1U;
    s_state.dht.successes = 1U;
    s_state.dht.attempts = 3U;
    s_state.dht.failures = 2U;
    s_state.dht.consecutive_failures = 1U;
    s_state.dht.last_status = 8U;
    s_state.dht.last_success_ms = app_ui_tick_ms() - 2000U;
    s_state.sensor_updated_ms = s_state.humidity_updated_ms = s_state.dht.last_success_ms;
    apply_action(APP_ACTION_POWER);
    publish(); advance(220U);
    if (!check((find_visible_label(lv_screen_active(), "上次温度") && sensor_reading_is(false, "27.0")) &&
               (find_visible_label(lv_screen_active(), "上次湿度") && sensor_reading_is(true, "55")) &&
               find_visible_label(lv_screen_active(), "采样失败 E8") &&
               !s_state.climate.sensor_fresh && !s_state.climate.cooling_request,
               "failed DHT displays labelled last-good values while cooling stays invalid") ||
        !inspect_home_details("温差 未知", NULL,
               "failed DHT strategy details explicitly leave the control delta unknown") ||
        !save_image(directory, "dht-last-good-failure")) { return false; }
    if (!click("数据")) { return false; }
    lv_obj_t *temperature_chart = find_widget(&lv_chart_class, 0U);
    if (!check(temperature_chart && chart_last(temperature_chart, 0U) == LV_CHART_POINT_NONE,
               "last-good DHT cache is not inserted as fresh history data") ||
        !click("主页")) { return false; }
    s_state.dht.last_read_ok = 1U;
    s_state.dht.last_status = 1U;
    s_state.dht.successes++;
    s_state.dht.attempts++;
    s_state.dht.consecutive_failures = 0U;
    s_state.dht.last_success_ms = app_ui_tick_ms();
    s_state.sensor_valid = s_state.humidity_valid = 1U;
    s_state.sensor_updated_ms = s_state.humidity_updated_ms = s_state.dht.last_success_ms;
    refresh_climate(app_ui_tick_ms());
    publish(); advance(220U);
    if (!check(find_visible_label(lv_screen_active(), "DHT11 正常") &&
               !find_visible_label(lv_screen_active(), "上次温度") && s_state.climate.sensor_fresh,
               "successful DHT recovery restores fresh display and control quality")) { return false; }
    /* Modal inspection shifts the UI/model phases. Include one 500 ms
       controller period rather than depending on the previous alignment. */
    advance(APP_SENSOR_STALE_MS + 600U);
    if (!check(find_visible_label(lv_screen_active(), "数据过期") &&
               (find_visible_label(lv_screen_active(), "上次温度") && sensor_reading_is(false, "27.0")) &&
               !s_state.climate.sensor_fresh && !s_state.climate.cooling_request,
               "expired DHT cache remains visibly old and never authorizes cooling") ||
        !inspect_home_details("温差 未知", NULL,
               "expired DHT details retain the unknown delta instead of treating cached readings as fresh")) { return false; }
    reset_simulation(); advance(1500U);
    return true;
}

static bool check_v93_controls(const char *directory)
{
    reset_simulation(); advance(1200U);
    unsigned int actions = s_action_count;
    if (!click("风速") || !check(app_ui_controls_active() && s_action_count == actions,
        "opening FAN selects no action and preserves preferences")) { return false; }
    for (unsigned int speed = 1U; speed <= 5U; speed++) {
        char caption[24]; (void)snprintf(caption, sizeof(caption), "风档 %u", speed);
        if (!click(caption) || !check(s_state.climate.fan_speed == speed &&
            s_action_count == ++actions, "each explicit fan choice submits exactly one existing action")) { return false; }
    }
    if (!save_image(directory, "v9_3_fan_selection") || !click("完成") ||
        !check(!app_ui_controls_active() && s_action_count == actions,
        "closing FAN submits no extra action")) { return false; }
    if (!click("摆风") || !click("上下摆风") ||
        !check(s_state.climate.swing_ud && !s_state.climate.swing_lr && s_action_count == ++actions,
        "up/down swing uses its own existing action") || !click("左右摆风") ||
        !check(s_state.climate.swing_ud && s_state.climate.swing_lr && s_action_count == ++actions,
        "left/right swing remains independently selectable") || !click("完成")) { return false; }
    if (!click("更多") || !click("灯光") ||
        !check(!s_state.climate.lamp_on && s_action_count == ++actions,
        "LCD exposes the existing lamp preference exactly once") || !click("完成")) { return false; }
    /* An automatic sleep source remains visible while the manual button
       represents only what the user selected. Turbo must report its limit. */
    s_state.als = 50U; advance(1200U);
    if (!check(s_state.climate.auto_sleep && !s_state.climate.manual_sleep,
        "automatic sleep remains independent from the manual preference") ||
        !inspect_home_details("自动睡眠", NULL,
        "strategy details explain the automatic source without pretending manual sleep was selected") ||
        !click("电源") || !click("强劲") || !click("更多")) { return false; }
    if (!check(find_visible_label(lv_screen_active(), "已选风档 5 | 执行风档 1") &&
        find_visible_label(lv_screen_active(), "睡眠优先"),
        "selected Turbo and actual sleep-limited fan speed are explained together") ||
        !save_image(directory, "v9_3_auto_sleep_limited")) { return false; }
    unsigned int before = s_action_count;
    if (!click("电源") || !check(s_action_count == before && s_state.climate.power_on,
        "modal backdrop prevents a click from reaching the underlying POWER button")) { return false; }
    if (!click("手动睡眠") || !check(s_state.climate.manual_sleep,
        "modal manual sleep changes only the manual preference") || !click("手动睡眠") ||
        !check(!s_state.climate.manual_sleep && s_state.climate.effective_sleep,
        "clearing manual sleep leaves the separately explained automatic source effective") || !click("完成")) { return false; }
    lv_mem_monitor_t first, last;
    lv_mem_monitor(&first);
    for (unsigned int i = 0U; i < 10U; i++) {
        if (!click("风速") || !click("完成") || !click("摆风") ||
            !click("完成") || !click("更多") || !click("完成")) { return false; }
    }
    lv_mem_monitor(&last);
    if (!check(last.free_size + 512U >= first.free_size,
        "repeated control dialogs reuse their persistent objects and text storage")) { return false; }
    reset_simulation(); advance(1200U);
    return save_image(directory, "v9_3_home");
}

static bool check_v93_chart_details(const char *directory)
{
    reset_simulation(); advance(5500U);
    s_state.temperature = 29.0f; advance(5500U);
    if (!click("数据")) { return false; }
    lv_obj_t *charts[4]; int32_t *arrays[5];
    for (unsigned int i = 0U; i < 4U; i++) {
        charts[i] = find_widget(&lv_chart_class, i);
        lv_chart_series_t *series = lv_chart_get_series_next(charts[i], NULL);
        arrays[i] = lv_chart_get_y_array(charts[i], series);
        if (i == 0U) { arrays[4] = lv_chart_get_y_array(charts[0], lv_chart_get_series_next(charts[0], series)); }
    }
    unsigned int actions = s_action_count;
    for (unsigned int i = 0U; i < 4U; i++) {
        lv_area_t area; lv_obj_get_coords(charts[i], &area);
        if (!click_point((area.x1 + area.x2) / 2, (area.y1 + area.y2) / 2) ||
            !check(find_button(lv_screen_active(), "返回曲线") && lv_obj_get_width(charts[i]) > 600 &&
                find_visible_label(lv_screen_active(), "实际采样") &&
                find_visible_label(lv_screen_active(), "选中"),
                "each actual DATA chart opens a readable enlarged sample inspector")) { return false; }
        if (!pc_port_push_pointer(90, 220, true)) { return false; } process_frame();
        if (!pc_port_push_pointer(700, 220, true)) { return false; } process_frame();
        if (!pc_port_push_pointer(700, 220, false)) { return false; } process_frame(); advance(220U);
        if (!check(find_widget(&lv_chart_class, 4U) == NULL && s_action_count == actions,
            "dragging DATA samples neither creates another chart nor submits a control action")) { return false; }
        if (i == 0U) {
            if (!save_image(directory, "v9_3_temperature_detail")) { return false; }
            lv_obj_t *fixed = find_visible_label(lv_screen_active(), "选中");
            char saved_value[256];
            (void)snprintf(saved_value, sizeof(saved_value), "%s", lv_label_get_text(fixed));
            advance(5500U);
            if (!check(strcmp(saved_value, lv_label_get_text(fixed)) == 0,
                "new history samples preserve the inspected timestamp and its value")) { return false; }
            advance(310000U);
            if (!check(find_visible_label(lv_screen_active(), "原样本已移出") &&
                lv_chart_get_point_count(charts[0]) == 60U,
                "ring-buffer rollover explicitly releases an evicted inspected sample")) { return false; }
            /* Explicit source-switch fixture. No hardware data is supplied. */
            s_state.simulated = 0U; publish(); advance(220U);
            s_state.simulated = 1U; publish(); advance(220U);
            if (!check(find_visible_label(lv_screen_active(), "会话已切换"),
                "a source session change never reuses a selected sample from the previous session")) { return false; }
        }
        if (!click("返回曲线") || !check(lv_obj_get_width(charts[i]) == 214 &&
            lv_obj_get_height(charts[i]) == 74 && lv_chart_get_point_count(charts[i]) == 60U &&
            lv_chart_get_y_array(charts[i], lv_chart_get_series_next(charts[i], NULL)) == arrays[i],
            "leaving an inspector restores layout and the same external history array")) { return false; }
    }
    if (!check(lv_chart_get_y_array(charts[0], lv_chart_get_series_next(charts[0],
        lv_chart_get_series_next(charts[0], NULL))) == arrays[4],
        "enlarging charts also preserves the original effective-target array")) { return false; }
    if (!press_key('s', false)) { return false; } advance(6500U);
    if (!click_point(145, 202)) { return false; }
    lv_obj_t *selected = find_visible_label(lv_screen_active(), "选中");
    if (!check(selected && strstr(lv_label_get_text(selected), "未知"),
        "latest invalid temperature is explicitly unknown in the sample inspector") ||
        !save_image(directory, "v9_3_temperature_unknown") || !click("返回曲线") ||
        !press_key('s', false)) { return false; }
    s_state.temperature = 29.0f;
    apply_action(APP_ACTION_POWER);
    advance(APP_COOLING_MIN_OFF_MS + 5500U);
    if (!click("控制请求") || !click_point(145, 202) ||
        !check(lv_chart_get_type(charts[0]) == LV_CHART_TYPE_BAR,
        "the enlarged request inspector retains binary request bars")) { return false; }
    lv_area_t plot; lv_obj_get_coords(charts[0], &plot);
    lv_color_t color = lv_chart_get_series_color(charts[0], lv_chart_get_series_next(charts[0], NULL));
    uint16_t ink = (uint16_t)(0xF000U | ((uint16_t)(color.red & 0xF0U) << 4U) |
                             (color.green & 0xF0U) | (color.blue >> 4U));
    const uint16_t *foreground = (const uint16_t *)app_lcd_get_fg_draw_buffer();
    unsigned int pixels = 0U;
    for (int32_t y = plot.y1 + 4; y < plot.y2 - 6; y++) {
        for (int32_t x = plot.x1 + 4; x < plot.x2 - 4; x++) {
            if (foreground[y * LCD_FG_WIDTH + x] == ink) { pixels++; }
        }
    }
    printf("Enlarged R1 ON rendered series pixels=%u\n", pixels);
    if (!check(s_state.climate.cooling_request && pixels > 100U,
        "enlarged active R1 request bars are actually visible in the foreground pixels") ||
        !save_image(directory, "v9_3_request_detail") || !click("返回曲线") ||
        !click("环境趋势") || !click("主页")) { return false; }
    reset_simulation(); advance(1200U);
    return true;
}

static bool check_v93_stable_overlay(const char *directory)
{
    reset_simulation(); advance(1200U);
    if (!click("视觉")) { return false; }
    s_mock_vision = false;
    s_state.vision.valid = 1U; s_state.vision.count = 1U;
    s_state.vision.frame_id = 500000U;
    s_state.vision.boxes[0] = (app_ui_detection_t){200U, 160U, 120U, 210U, 0.93f, 0};
    s_state.vision.captured_ms = app_ui_tick_ms(); advance(33U);
    lv_obj_t *caption = find_visible_label(lv_screen_active(), "93%");
    if (!caption) { return check(false, "stable preview has a visible detection fixture"); }
    lv_obj_t *box = lv_obj_get_parent(caption);
    double raw_error = 0.0, stable_error = 0.0;
    unsigned int actions = s_action_count;
    for (unsigned int i = 0U; i < 60U; i++) {
        int32_t offset = (i & 1U) ? 8 : -8;
        s_state.vision.boxes[0].x = (uint16_t)(200 + offset);
        s_state.vision.frame_id++; s_state.vision.captured_ms = app_ui_tick_ms();
        advance(33U);
        double deviation = (double)lv_obj_get_x(box) - 200.0;
        stable_error += deviation * deviation; raw_error += offset * offset;
    }
    printf("Overlay jitter same input: raw RMS=%.3f stable RMS=%.3f px\n",
        sqrt(raw_error / 60.0), sqrt(stable_error / 60.0));
    if (!check(stable_error < raw_error * 0.65 && s_action_count == actions && s_state.people_count == 1,
        "real LVGL stable overlay reduces identical 30Hz jitter without changing people or controls") ||
        !save_image(directory, "v9_3_vision_stable") ||
        !check(!find_button(lv_screen_active(), "框：稳定") &&
               !find_button(lv_screen_active(), "框：原始"),
        "stable display remains enabled without preview mode-switch controls")) { return false; }
    s_state.vision.count = 0U; advance(33U);
    if (!check(lv_obj_has_flag(box, LV_OBJ_FLAG_HIDDEN),
        "same-frame empty result immediately removes the stable drawing")) { return false; }
    s_state.vision.count = 1U; advance(33U);
    if (!check(!lv_obj_has_flag(box, LV_OBJ_FLAG_HIDDEN),
        "same-frame valid recovery rebuilds a visible current box")) { return false; }
    s_state.vision.valid = 0U; advance(33U);
    if (!check(lv_obj_has_flag(box, LV_OBJ_FLAG_HIDDEN),
        "stable display clears immediately on invalid AI even with the same frame ID")) { return false; }
    s_mock_vision = true;
    if (!click("返回主页")) { return false; }
    reset_simulation(); advance(1200U);
    return true;
}

static bool area_equal(const lv_area_t *first, const lv_area_t *second)
{
    return first->x1 == second->x1 && first->y1 == second->y1 &&
        first->x2 == second->x2 && first->y2 == second->y2;
}

static bool area_inside(const lv_area_t *inner, const lv_area_t *outer)
{
    return inner->x1 >= outer->x1 && inner->y1 >= outer->y1 &&
        inner->x2 <= outer->x2 && inner->y2 <= outer->y2;
}

static uint32_t object_count(lv_obj_t *object)
{
    uint32_t count = 1U;
    for (uint32_t i = 0U; i < lv_obj_get_child_count(object); i++) {
        count += object_count(lv_obj_get_child(object, (int32_t)i));
    }
    return count;
}

static bool check_v94_modal_layout(const char *const names[], unsigned int count,
                                    unsigned int columns, const char *description)
{
    lv_obj_update_layout(lv_screen_active());
    lv_obj_t *row = NULL;
    lv_area_t areas[5];
    bool valid = count <= 5U && columns > 0U;
    for (unsigned int i = 0U; valid && i < count; i++) {
        lv_obj_t *button = find_button(lv_screen_active(), names[i]);
        if (!button) { valid = false; break; }
        if (!row) { row = lv_obj_get_parent(button); }
        lv_obj_get_coords(button, &areas[i]);
        lv_area_t caption;
        lv_obj_get_coords(lv_obj_get_child(button, 0), &caption);
        valid = lv_obj_get_parent(button) == row &&
            lv_obj_get_height(button) >= 48 && area_inside(&caption, &areas[i]) &&
            !lv_obj_has_flag(button, LV_OBJ_FLAG_CHECKABLE);
        if (i > 0U && i % columns != 0U) {
            valid = valid && areas[i].y1 == areas[i - 1U].y1 &&
                areas[i].x1 > areas[i - 1U].x2;
        } else if (i >= columns) {
            valid = valid && areas[i].x1 == areas[i - columns].x1 &&
                areas[i].y1 > areas[i - columns].y2;
        }
    }
    unsigned int visible = 0U;
    if (row) {
        lv_area_t row_area;
        lv_obj_get_coords(row, &row_area);
        for (unsigned int i = 0U; valid && i < count; i++) {
            valid = area_inside(&areas[i], &row_area);
        }
        for (uint32_t i = 0U; i < lv_obj_get_child_count(row); i++) {
            lv_obj_t *child = lv_obj_get_child(row, (int32_t)i);
            if (lv_obj_check_type(child, &lv_button_class) &&
                !lv_obj_has_flag(child, LV_OBJ_FLAG_HIDDEN)) { visible++; }
        }
    }
    return check(valid && visible == count, description);
}

static bool check_v94_navigation(unsigned int selected)
{
    static const char *const names[4] = {"主页", "视觉", "数据", "演示"};
    bool valid = true;
    lv_area_t prior = {0};
    for (unsigned int i = 0U; i < 4U; i++) {
        lv_obj_t *button = find_button(lv_screen_active(), names[i]);
        if (!button) { valid = false; break; }
        lv_area_t area, caption;
        lv_obj_get_coords(button, &area);
        lv_obj_get_coords(lv_obj_get_child(button, 0), &caption);
        valid = valid && lv_obj_has_state(button, LV_STATE_CHECKED) == (i == selected) &&
            !lv_obj_has_flag(button, LV_OBJ_FLAG_CHECKABLE) &&
            area.x1 >= 0 && area.x2 < LCD_FG_WIDTH && area.y1 >= 0 &&
            area.y2 < LCD_FG_HEIGHT && lv_obj_get_height(button) == 48 &&
            area_inside(&caption, &area) && (i == 0U || area.x1 > prior.x2);
        prior = area;
    }
    return check(valid, "V9.4 Chinese navigation fits four touch targets and checks only the actual page");
}

static uint16_t color_argb4444(uint32_t color)
{
    return (uint16_t)(0xF000U | ((color & 0xF00000U) >> 12U) |
        ((color & 0xF000U) >> 8U) | ((color & 0xF0U) >> 4U));
}

static bool check_v94_theme(const char *directory)
{
    static const char *const controls[6] = {
        "风速", "摆风", "睡眠", "节能", "强劲", "灯光"
    };
    reset_simulation(); advance(1200U);
    lv_obj_update_layout(lv_screen_active());
    if (!save_image(directory, "bento_home")) { return false; }
    lv_area_t previous = {0};
    lv_area_t prior_caption = {0}, prior_status = {0}, prior_icon = {0};
    const lv_font_t *control_font = NULL;
    for (unsigned int i = 0U; i < 6U; i++) {
        lv_obj_t *button = find_button(lv_screen_active(), controls[i]);
        lv_obj_t *text = button_caption(button, controls[i]);
        if (!button || !text) { return check(false, "F HOME directly exposes all six Chinese preference controls"); }
        lv_obj_t *status = NULL, *glyph = NULL;
        for (uint32_t child_index = 0U; child_index < lv_obj_get_child_count(button); child_index++) {
            lv_obj_t *child = lv_obj_get_child(button, (int32_t)child_index);
            if (lv_obj_check_type(child, &lv_label_class) && child != text) { status = child; }
            if (lv_obj_check_type(child, &lv_image_class)) { glyph = child; }
        }
        lv_area_t area, caption, status_area = {0}, icon_area = {0};
        lv_obj_get_coords(button, &area);
        lv_obj_get_coords(text, &caption);
        if (status) { lv_obj_get_coords(status, &status_area); }
        if (glyph) { lv_obj_get_coords(glyph, &icon_area); }
        if (i == 0U) { control_font = lv_obj_get_style_text_font(text, LV_PART_MAIN); }
        char message[160];
        (void)snprintf(message, sizeof(message),
            "Bento preference %u has a contained caption and a separate 66px touch target", i + 1U);
        if (!area_inside(&caption, &area)) {
            fprintf(stderr, "Bento geometry %u: button=[%ld,%ld,%ld,%ld] caption=[%ld,%ld,%ld,%ld] align=%u\n",
                i + 1U, (long)area.x1, (long)area.y1, (long)area.x2, (long)area.y2,
                (long)caption.x1, (long)caption.y1, (long)caption.x2, (long)caption.y2,
                (unsigned)lv_obj_get_style_align(text, LV_PART_MAIN));
        }
        if (!check(area.x1 >= 0 && area.x2 < LCD_FG_WIDTH && area.y1 >= 0 &&
            area.y2 < LCD_FG_HEIGHT && lv_obj_get_height(button) >= 66 &&
            area_inside(&caption, &area) &&
            (i == 0U || (area.x1 > previous.x2 && area.y1 == previous.y1)), message)) { return false; }
        (void)snprintf(message, sizeof(message),
            "Bento preference %u uses the same Chinese font and aligned title/status rows", i + 1U);
        if (!check(status && strlen(lv_label_get_text(status)) != 0U &&
            lv_obj_get_style_text_font(text, LV_PART_MAIN) == control_font &&
            lv_obj_get_style_text_font(status, LV_PART_MAIN) == control_font &&
            control_font == &app_font_ui_16 && area_inside(&status_area, &area) &&
            (i == 0U || (caption.y1 == prior_caption.y1 && status_area.y1 == prior_status.y1)), message)) { return false; }
        (void)snprintf(message, sizeof(message),
            "Bento preference %u uses a contained 28px alpha icon at native scale and common alignment", i + 1U);
        const lv_image_dsc_t *glyph_source = glyph ? (const lv_image_dsc_t *)lv_image_get_src(glyph) : NULL;
        if (!check(glyph && glyph_source && lv_obj_get_width(glyph) == 28 &&
            lv_obj_get_height(glyph) == 28 && lv_image_get_scale_x(glyph) == 256 &&
            lv_image_get_scale_y(glyph) == 256 && glyph_source->header.w == 28 &&
            glyph_source->header.h == 28 && glyph_source->header.cf == LV_COLOR_FORMAT_A8 &&
            area_inside(&icon_area, &area) &&
            (i == 0U || icon_area.y1 == prior_icon.y1), message)) { return false; }
        previous = area;
        prior_caption = caption; prior_status = status_area; prior_icon = icon_area;
    }
    lv_obj_t *temperature = find_sensor_reading(false);
    lv_obj_t *humidity = find_sensor_reading(true);
    lv_area_t temperature_area = {0}, humidity_area = {0};
    if (temperature) { lv_obj_get_coords(temperature, &temperature_area); }
    if (humidity) { lv_obj_get_coords(humidity, &humidity_area); }
    if (!check(temperature && humidity && temperature != humidity &&
        sensor_reading_is(false, "27.0") && sensor_reading_is(true, "55") &&
        lv_obj_get_style_text_font(temperature, LV_PART_MAIN) == &lv_font_montserrat_40 &&
        lv_obj_get_style_text_font(humidity, LV_PART_MAIN) == &lv_font_montserrat_40 &&
        temperature_area.x1 > LCD_FG_WIDTH / 2 && temperature_area.x2 < 724 &&
        humidity_area.x1 > LCD_FG_WIDTH / 2 && humidity_area.x2 < 724 &&
        temperature_area.y1 >= 60 && temperature_area.y1 == humidity_area.y1 &&
        temperature_area.x2 < humidity_area.x1 && humidity_area.y2 < 142,
        "Bento live temperature and humidity remain independent compact readings side by side") ||
        !save_image(directory, "v9_4_home")) { return false; }
    lv_color_t temperature_live_color = lv_obj_get_style_text_color(temperature, LV_PART_MAIN);
    lv_color_t humidity_live_color = lv_obj_get_style_text_color(humidity, LV_PART_MAIN);
    lv_color_t temperature_live_card = lv_obj_get_style_bg_color(lv_obj_get_parent(temperature), LV_PART_MAIN);
    lv_color_t humidity_live_card = lv_obj_get_style_bg_color(lv_obj_get_parent(humidity), LV_PART_MAIN);

    lv_obj_t *power = find_button(lv_screen_active(), "电源");
    lv_area_t power_area; lv_obj_get_coords(power, &power_area);
    int32_t px = (power_area.x1 + power_area.x2) / 2;
    int32_t py = (power_area.y1 + power_area.y2) / 2;
    lv_color_t pressed_feedback = lv_color_black();
    for (unsigned int cycle = 0U; cycle < 2U; cycle++) {
        unsigned int actions = s_action_count;
        bool before = s_state.climate.power_on != 0U;
        lv_color_t before_color = lv_obj_get_style_bg_color(power, LV_PART_MAIN);
        if (!pc_port_push_pointer(px, py, true)) { return false; }
        process_frame(); advance(33U); lv_obj_update_layout(lv_screen_active());
        lv_area_t pressed; lv_obj_get_coords(power, &pressed);
        if (!check(s_action_count == actions && (s_state.climate.power_on != 0U) == before &&
            lv_obj_has_state(power, LV_STATE_CHECKED) == before &&
            lv_obj_has_state(power, LV_STATE_PRESSED),
            "V9.4 real pointer DOWN does not submit POWER or optimistically toggle its snapshot state") ||
            !check(!lv_color_eq(lv_obj_get_style_bg_color(power, LV_PART_MAIN), before_color) &&
                (cycle == 0U || lv_color_eq(lv_obj_get_style_bg_color(power, LV_PART_MAIN), pressed_feedback)) &&
                area_equal(&pressed, &power_area),
                "F pressed feedback is distinct in both checked states and preserves the touch rectangle")) { return false; }
        pressed_feedback = lv_obj_get_style_bg_color(power, LV_PART_MAIN);
        const uint16_t *foreground = (const uint16_t *)app_lcd_get_fg_draw_buffer();
        lv_color32_t pressed_argb = lv_color_to_32(pressed_feedback, LV_OPA_COVER);
        uint32_t pressed_rgb = ((uint32_t)pressed_argb.red << 16U) |
            ((uint32_t)pressed_argb.green << 8U) | pressed_argb.blue;
        if (!check(foreground && foreground[(power_area.y2 - 5) * LCD_FG_WIDTH + px] ==
            color_argb4444(pressed_rgb),
            "F the actual pressed-state color is visible in rendered foreground pixels")) { return false; }
        if (cycle == 0U && !save_image(directory, "v9_4_pressed")) { return false; }
        if (!pc_port_push_pointer(px, py, false)) { return false; }
        process_frame(); advance(220U);
        lv_area_t released; lv_obj_get_coords(power, &released);
        if (!check(s_action_count == actions + 1U && (s_state.climate.power_on != 0U) != before &&
            lv_obj_has_state(power, LV_STATE_CHECKED) == (s_state.climate.power_on != 0U) &&
            !lv_obj_has_state(power, LV_STATE_PRESSED) && area_equal(&released, &power_area) &&
            !lv_color_eq(lv_obj_get_style_bg_color(power, LV_PART_MAIN), pressed_feedback),
            "V9.4 pointer UP submits POWER exactly once and renders the resulting checked state without resizing")) { return false; }
    }

    lv_obj_t *more = find_button(lv_screen_active(), "更多");
    lv_area_t more_area; lv_obj_get_coords(more, &more_area);
    lv_area_t more_touch; lv_obj_get_click_area(more, &more_touch);
    if (!check(more_touch.y2 - more_touch.y1 + 1 >= 44 && more_area.x1 >= 0 && more_area.x2 < LCD_FG_WIDTH &&
        more_area.y1 >= 0 && more_area.y2 < LCD_FG_HEIGHT,
        "F top MORE has a visible compact surface and an at least 44px effective touch height")) { return false; }
    for (unsigned int edge = 0U; edge < 4U; edge++) {
        int32_t x = (more_area.x1 + more_area.x2) / 2;
        int32_t y = (more_area.y1 + more_area.y2) / 2;
        if (edge == 0U) { x = more_area.x1 + 3; }
        else if (edge == 1U) { x = more_area.x2 - 3; }
        else if (edge == 2U) { y = more_area.y1 + 3; }
        else { y = more_area.y2 - 3; }
        unsigned int actions = s_action_count;
        bool power_before = s_state.climate.power_on != 0U;
        if (!click_point(x, y) || !check(app_ui_controls_active() && s_action_count == actions &&
            (s_state.climate.power_on != 0U) == power_before,
            "V9.4 MORE opens at each real 3px inset edge without injecting a control action") ||
            !click("完成") || !check(!app_ui_controls_active() && s_action_count == actions,
            "V9.4 closing the edge-opened modal does not click through or submit an extra action")) { return false; }
    }

    static const char *const fans[5] = {"风档 1", "风档 2", "风档 3", "风档 4", "风档 5"};
    static const char *const swing[2] = {"左右摆风", "上下摆风"};
    static const char *const options[4] = {"灯光", "手动睡眠", "手动节能", "强劲"};
    unsigned int actions = s_action_count;
    if (!click("风速") || !check_v94_modal_layout(fans, 5U, 5U,
        "V9.4 FAN lays out five choices in one row while hidden SWING/MORE choices occupy no slots")) { return false; }
    lv_obj_t *fan_five = find_button(lv_screen_active(), "风档 5");
    if (!check(!lv_obj_has_state(fan_five, LV_STATE_CHECKED),
        "V9.4 FAN initially checks only the authoritative default fan choice") ||
        !click("风档 5") || !check(s_action_count == actions + 1U && s_state.climate.fan_speed == 5U &&
            lv_obj_has_state(fan_five, LV_STATE_CHECKED),
            "V9.4 a real FAN click renders the acknowledged preference exactly once") ||
        !save_image(directory, "v9_4_fan") || !click("完成") || !click("摆风") ||
        !check_v94_modal_layout(swing, 2U, 2U,
            "V9.4 SWING places its two independent choices in one row without hidden FAN slots") ||
        !click("完成") || !click("更多") ||
        !check_v94_modal_layout(options, 4U, 2U,
            "V9.4 MORE presents a non-overlapping two-by-two grid and excludes hidden FAN/SWING slots") ||
        !save_image(directory, "v9_4_more") || !click("完成") ||
        !check(s_action_count == actions + 1U,
            "V9.4 modal layout changes preserve control preferences and inject no automatic toggle")) { return false; }

    if (!click("数据") || !check_v94_navigation(APP_UI_PAGE_DATA)) { return false; }
    lv_obj_t *charts[4], *cards[4];
    for (unsigned int i = 0U; i < 4U; i++) {
        charts[i] = find_widget(&lv_chart_class, i);
        if (!charts[i]) { return check(false, "V9.4 DATA retains the original four charts"); }
        cards[i] = lv_obj_get_parent(charts[i]);
    }
    uint32_t data_objects = object_count(lv_screen_active());
    actions = s_action_count;
    if (!save_image(directory, "v9_4_data")) { return false; }
    for (unsigned int i = 0U; i < 4U; i++) {
        for (unsigned int target = 0U; target < 2U; target++) {
            lv_obj_update_layout(lv_screen_active());
            lv_area_t area; lv_obj_get_coords(cards[i], &area);
            int32_t x = target == 0U ? area.x1 + 90 : area.x2 - 3;
            int32_t y = target == 0U ? area.y1 + 18 : (area.y1 + area.y2) / 2;
            if (!click_point(x, y) || !check(find_button(lv_screen_active(), "返回曲线") &&
                lv_obj_get_width(charts[i]) > 600 && s_action_count == actions,
                "V9.4 chart-card title and empty side inset each open details through the actual pointer")) { return false; }
            if (i == 0U && target == 0U && !save_image(directory, "v9_4_temperature_detail")) { return false; }
            if (!click("返回曲线") || !check(lv_obj_get_width(charts[i]) == 214 &&
                find_widget(&lv_chart_class, i) == charts[i] && lv_obj_get_parent(charts[i]) == cards[i] &&
                object_count(lv_screen_active()) == data_objects && s_action_count == actions,
                "V9.4 card inspection reuses all existing objects and restores its original chart")) { return false; }
        }
    }
    if (!click("演示") || !check_v94_navigation(APP_UI_PAGE_DEMO)) { return false; }
    /* This fixture changes only the presentation's source flag; PC model
       samples remain controlled, and no real hardware stream is claimed. */
    s_state.simulated = 0U; s_state.temperature_source = APP_TEMPERATURE_DHT11;
    publish(); advance(220U);
    lv_obj_t *demo_start = find_button(lv_screen_active(), "开始回放");
    lv_obj_t *demo_stop = find_button(lv_screen_active(), "停止并恢复");
    if (!check(demo_start && demo_stop && lv_obj_has_state(demo_start, LV_STATE_DISABLED) &&
        lv_obj_has_state(demo_stop, LV_STATE_DISABLED) &&
        lv_color_eq(lv_obj_get_style_text_color(lv_obj_get_child(demo_start, 0), LV_PART_MAIN),
            lv_color_hex(APP_UI_COLOR_MUTED)) &&
        lv_color_eq(lv_obj_get_style_text_color(lv_obj_get_child(demo_stop, 0), LV_PART_MAIN),
            lv_color_hex(APP_UI_COLOR_MUTED)),
        "V9.4 real-board DEMO disables both controls and its child captions inherit the muted state color") ||
        !click("开始回放") || !click("停止并恢复") ||
        !check(!s_demo.active && !s_state.demo_active && s_action_count == actions,
            "V9.4 real pointer clicks on disabled real-board DEMO controls inject no simulated scene or action") ||
        !save_image(directory, "v9_4_demo_disabled") || !click("主页")) { return false; }

    reset_simulation(); advance(1200U);
    s_skip_automatic_samples = true;
    s_state.sensor_valid = 0U; s_state.humidity_valid = 0U;
    refresh_climate(app_ui_tick_ms()); publish(); advance(220U);
    temperature = find_sensor_reading(false);
    humidity = find_sensor_reading(true);
    if (!check(temperature && humidity && temperature != humidity &&
        sensor_reading_is(false, "--") && sensor_reading_is(true, "--") &&
        lv_obj_get_style_text_font(temperature, LV_PART_MAIN) == &lv_font_montserrat_40 &&
        lv_obj_get_style_text_font(humidity, LV_PART_MAIN) == &lv_font_montserrat_40 &&
        !s_state.climate.sensor_fresh && !s_state.climate.humidity_fresh &&
        !lv_color_eq(lv_obj_get_style_text_color(temperature, LV_PART_MAIN), temperature_live_color) &&
        !lv_color_eq(lv_obj_get_style_text_color(humidity, LV_PART_MAIN), humidity_live_color) &&
        !lv_color_eq(lv_obj_get_style_bg_color(lv_obj_get_parent(temperature), LV_PART_MAIN), temperature_live_card) &&
        !lv_color_eq(lv_obj_get_style_bg_color(lv_obj_get_parent(humidity), LV_PART_MAIN), humidity_live_card) &&
        find_visible_label(lv_screen_active(), "温度未更新 / 湿度未更新"),
        "Bento unavailable sources show separate compact unknowns, neutral cards and explicit quality without fabricated values") ||
        !save_image(directory, "v9_4_missing_measurements")) { return false; }
    reset_simulation(); advance(1200U);
    s_state.humidity = NAN; advance(600U);
    temperature = find_sensor_reading(false);
    humidity = find_sensor_reading(true);
    if (!check(temperature && humidity && s_state.climate.sensor_fresh &&
        !s_state.climate.humidity_fresh &&
        sensor_reading_is(false, "27.0") && sensor_reading_is(true, "--") &&
        lv_color_eq(lv_obj_get_style_text_color(temperature, LV_PART_MAIN), temperature_live_color) &&
        lv_color_eq(lv_obj_get_style_bg_color(lv_obj_get_parent(temperature), LV_PART_MAIN), temperature_live_card) &&
        !lv_color_eq(lv_obj_get_style_text_color(humidity, LV_PART_MAIN), humidity_live_color) &&
        !lv_color_eq(lv_obj_get_style_bg_color(lv_obj_get_parent(humidity), LV_PART_MAIN), humidity_live_card) &&
        find_visible_label(lv_screen_active(), "温度有效 / 湿度无效"),
        "F invalid humidity leaves temperature live and healthy while its own card and quality become unavailable") ||
        !save_image(directory, "v9_4_humidity_unknown")) { return false; }
    reset_simulation(); advance(1200U);
    if (!click("视觉") || !save_image(directory, "v9_4_vision") || !click("返回主页")) { return false; }
    reset_simulation(); advance(1200U);
    return true;
}

static bool check_watch_home(const char *directory)
{
    reset_simulation(); advance(1500U);
    lv_obj_t *target = find_visible_label(lv_screen_active(), "26");
    lv_obj_t *automatic = find_button(lv_screen_active(), "智能自动");
    lv_obj_t *manual = find_button(lv_screen_active(), "手动制冷");
    if (!check(target && lv_obj_get_style_text_font(target, LV_PART_MAIN) == &app_font_watch_96 &&
        automatic && manual && lv_obj_has_state(automatic, LV_STATE_CHECKED) &&
        !lv_obj_has_state(manual, LV_STATE_CHECKED) &&
        !lv_obj_has_flag(automatic, LV_OBJ_FLAG_CHECKABLE) && !lv_obj_has_flag(manual, LV_OBJ_FLAG_CHECKABLE),
        "F uses the real 96px digit asset and checks only the authoritative selected mode")) { return false; }
    lv_obj_t *readout = lv_obj_get_parent(target), *target_unit = NULL;
    for (uint32_t i = 0U; i < lv_obj_get_child_count(readout); i++) {
        lv_obj_t *child = lv_obj_get_child(readout, (int32_t)i);
        if (lv_obj_check_type(child, &lv_label_class) &&
            strcmp(lv_label_get_text(child), "°C") == 0) { target_unit = child; }
    }
    lv_area_t number_area = {0}, unit_area = {0}, readout_area = {0}, arc_area = {0};
    lv_obj_get_coords(target, &number_area); lv_obj_get_coords(readout, &readout_area);
    if (target_unit) { lv_obj_get_coords(target_unit, &unit_area); }
    lv_obj_t *readout_arc = find_widget(&lv_arc_class, 0U);
    if (readout_arc) { lv_obj_get_coords(readout_arc, &arc_area); }
    const lv_font_t *number_font = lv_obj_get_style_text_font(target, LV_PART_MAIN);
    const lv_font_t *unit_font = target_unit ? lv_obj_get_style_text_font(target_unit, LV_PART_MAIN) : NULL;
    if (!check(target_unit && readout_arc && unit_font == &lv_font_montserrat_32 &&
        number_area.y1 + number_font->line_height - number_font->base_line ==
            unit_area.y1 + unit_font->line_height - unit_font->base_line &&
        number_area.x2 < unit_area.x1 &&
        abs((readout_area.x1 + readout_area.x2) - (arc_area.x1 + arc_area.x2)) <= 2,
        "Bento retains the approved 96px number and 32px unit with a shared baseline and centered group")) { return false; }
    unsigned int actions = s_action_count;
    if (!click("智能自动") || !check(s_action_count == actions && s_state.climate.mode == APP_CLIMATE_MODE_AUTO,
        "tapping the already selected AUTO mode injects no toggle") ||
        !click("手动制冷") || !check(s_action_count == actions + 1U &&
            s_state.climate.mode == APP_CLIMATE_MODE_MANUAL_COOL &&
            !lv_obj_has_state(automatic, LV_STATE_CHECKED) && lv_obj_has_state(manual, LV_STATE_CHECKED),
            "the explicit MANUAL selector submits one original action and shows its acknowledged result") ||
        !click("手动制冷") || !check(s_action_count == actions + 1U &&
            s_state.climate.mode == APP_CLIMATE_MODE_MANUAL_COOL,
            "tapping the already selected MANUAL mode is also a no-op") ||
        !click("智能自动") || !check(s_action_count == actions + 2U &&
            s_state.climate.mode == APP_CLIMATE_MODE_AUTO && s_state.climate.target_temperature == 26U,
            "switching back to AUTO preserves the chosen temperature")) { return false; }

    lv_area_t automatic_area, manual_area, automatic_touch, manual_touch;
    lv_obj_get_coords(automatic, &automatic_area); lv_obj_get_coords(manual, &manual_area);
    lv_obj_get_click_area(automatic, &automatic_touch); lv_obj_get_click_area(manual, &manual_touch);
    if (!check(automatic_touch.y2 - automatic_touch.y1 + 1 >= 44 &&
        manual_touch.y2 - manual_touch.y1 + 1 >= 44 && automatic_area.x2 < manual_area.x1,
        "compact mode surfaces preserve separate at least 44px-high effective touch areas")) { return false; }

    bool lamp = s_state.climate.lamp_on != 0U;
    actions = s_action_count;
    if (!click("灯光") || !check((s_state.climate.lamp_on != 0U) != lamp && s_action_count == actions + 1U,
        "the direct HOME lamp icon and caption operate the original lamp preference once") ||
        !click("灯光") || !check((s_state.climate.lamp_on != 0U) == lamp && s_action_count == actions + 2U,
        "a second direct lamp click restores its preference without fabricating a connected load")) { return false; }

    const uint16_t *foreground = (const uint16_t *)app_lcd_get_fg_draw_buffer();
    static const uint16_t background_y[3] = {80U, 160U, 240U};
    uint16_t background[3];
    for (unsigned int i = 0U; i < 3U; i++) { background[i] = foreground[background_y[i] * LCD_FG_WIDTH + 410U]; }
    actions = s_action_count;
    int32_t target_rest_y = lv_obj_get_y(target);
    int32_t readout_rest_y = lv_obj_get_y(lv_obj_get_parent(target));
    if (!click("+")) { return false; }
    advance(1000U);
    target = find_visible_label(lv_screen_active(), "27");
    foreground = (const uint16_t *)app_lcd_get_fg_draw_buffer();
    bool unchanged_background = true;
    for (unsigned int i = 0U; i < 3U; i++) {
        unchanged_background = unchanged_background &&
            foreground[background_y[i] * LCD_FG_WIDTH + 410U] == background[i];
    }
    if (!check(s_state.climate.target_temperature == 27U && s_action_count == actions + 1U &&
        target && strcmp(lv_label_get_text(target), "27") == 0 &&
        lv_obj_get_y(target) == target_rest_y &&
        lv_obj_get_y(lv_obj_get_parent(target)) == readout_rest_y,
        "the short digit/ring feedback settles on the acknowledged target without extra actions") ||
        !check(unchanged_background,
        "real foreground pixels in the Bento card surfaces stay static during local setpoint feedback") ||
        !save_image(directory, "watch_home")) { return false; }

    actions = s_action_count;
    if (!click("详情") || !check(find_visible_label(lv_screen_active(), "策略与状态详情") &&
        find_visible_label(lv_screen_active(), "软件请求（未接负载）") &&
        find_visible_label(lv_screen_active(), "制冷 OFF  风机 OFF  摆风 OFF  灯光 OFF") &&
        find_visible_label(lv_screen_active(), "保护等待") && find_visible_label(lv_screen_active(), "自动睡眠") &&
        find_visible_label(lv_screen_active(), "PC 模拟") && find_visible_label(lv_screen_active(), "无人计时"),
        "actual strategy details retain output bits, limits, automatic sources, sensing origin and absence timing") ||
        !click("电源") || !click("+") ||
        !check(s_action_count == actions && !s_state.climate.power_on && s_state.climate.target_temperature == 27U,
        "the new details backdrop blocks real underlying POWER and PLUS clicks") ||
        !save_image(directory, "watch_strategy_details") || !click("完成")) { return false; }
    reset_simulation(); advance(1500U);
    return true;
}

static bool queue_ring_pointer(lv_obj_t *arc, double relative_angle, bool pressed)
{
    lv_area_t area;
    lv_obj_get_coords(arc, &area);
    double radians = (135.0 + relative_angle) * 3.14159265358979323846 / 180.0;
    double radius = (lv_obj_get_width(arc) < lv_obj_get_height(arc) ?
        lv_obj_get_width(arc) : lv_obj_get_height(arc)) / 2.0 - 6.0;
    int32_t x = (int32_t)lround((area.x1 + area.x2) / 2.0 + cos(radians) * radius);
    int32_t y = (int32_t)lround((area.y1 + area.y2) / 2.0 + sin(radians) * radius);
    return pc_port_push_pointer(x, y, pressed);
}

static bool queue_ring_sweep(lv_obj_t *arc, uint8_t from, uint8_t to)
{
    double start = ((double)from - 16.0) * 270.0 / 14.0;
    double end = ((double)to - 16.0) * 270.0 / 14.0;
    unsigned int steps = (unsigned int)ceil(fabs(end - start) / 6.0);
    if (steps == 0U) { steps = 1U; }
    for (unsigned int i = 1U; i <= steps; i++) {
        if (!queue_ring_pointer(arc, start + (end - start) * i / steps, true)) { return false; }
        /* Real touch samples have elapsed time. LVGL's stock arc limits its
           angular rate using that time; a whole sweep at one virtual tick
           would be an artificial zero-time gesture. No private arc helper
           or programmatic VALUE_CHANGED event drives this interaction. */
        process_frame();
        pc_port_advance_time(20U);
    }
    return true;
}

static bool queue_button_click(const char *text)
{
    lv_obj_t *button = find_button(lv_screen_active(), text);
    if (!button) { return false; }
    lv_area_t area;
    lv_obj_get_coords(button, &area);
    int32_t x = (area.x1 + area.x2) / 2, y = (area.y1 + area.y2) / 2;
    return pc_port_push_pointer(x, y, true) && pc_port_push_pointer(x, y, false);
}

static bool check_bento_ring(const char *directory)
{
    reset_simulation(); advance(1200U);
    lv_obj_t *arc = find_widget(&lv_arc_class, 0U);
    lv_obj_t *plus = find_button(lv_screen_active(), "+");
    lv_obj_t *minus = find_button(lv_screen_active(), "-");
    if (!check(arc && plus && minus && lv_arc_get_min_value(arc) == 0 &&
        lv_arc_get_max_value(arc) == 270 && lv_obj_has_flag(arc, LV_OBJ_FLAG_CLICKABLE) &&
        lv_obj_get_width(plus) >= 64 && lv_obj_get_height(plus) >= 64 &&
        lv_obj_get_width(minus) >= 64 && lv_obj_get_height(minus) >= 64,
        "Bento combines an interactive visual-angle ring with separate 64px plus/minus targets")) { return false; }
    unsigned int actions = s_action_count;
    lv_area_t ring_area; lv_obj_get_coords(arc, &ring_area);
    int32_t top_x = (ring_area.x1 + ring_area.x2) / 2, top_y = ring_area.y1 + 6;
    if (!pc_port_push_pointer(top_x, top_y, true) ||
        !pc_port_push_pointer(top_x + 2, top_y + 1, true) ||
        !pc_port_push_pointer(top_x + 2, top_y + 1, false)) { return false; }
    process_frame(); advance(500U);
    if (!check(s_state.climate.target_temperature == 26U && s_action_count == actions,
        "a real ring tap or sub-threshold finger jitter cannot jump the setpoint")) { return false; }

    /* Programmatic LVGL display events are not physical drag gestures. */
    lv_arc_set_value(arc, 0);
    lv_obj_send_event(arc, LV_EVENT_VALUE_CHANGED, NULL);
    advance(500U);
    if (!check(s_state.climate.target_temperature == 26U && s_action_count == actions &&
        lv_arc_get_value(arc) == (26 - 16) * 270 / 14,
        "programmatic ring events restore its authoritative display and submit no control action")) { return false; }

    if (!queue_ring_pointer(arc, (26.0 - 16.0) * 270.0 / 14.0, true) ||
        !queue_ring_sweep(arc, 26U, 28U)) { return false; }
    process_frame();
    if (!check(s_state.climate.target_temperature == 28U && s_action_count == actions + 2U,
        "a real SDL ring drag sends exactly one unified action for each of two temperature steps")) { return false; }
    if (!queue_ring_pointer(arc, (28.0 - 16.0) * 270.0 / 14.0, true) ||
        !queue_ring_pointer(arc, (28.0 - 16.0) * 270.0 / 14.0, true)) { return false; }
    process_frame();
    if (!check(s_state.climate.target_temperature == 28U && s_action_count == actions + 2U,
        "re-reading the stationary dragged angle cannot replay temperature actions")) { return false; }
    if (!queue_ring_sweep(arc, 28U, 24U) || !queue_ring_sweep(arc, 24U, 30U) ||
        !queue_ring_sweep(arc, 30U, 16U) || !queue_ring_sweep(arc, 16U, 27U)) { return false; }
    process_frame();
    if (!check(s_state.climate.target_temperature == 27U && s_action_count == actions + 37U,
        "one continuous back-and-forth SDL gesture accumulates every step and respects both setpoint limits")) { return false; }
    if (!queue_ring_pointer(arc, (27.0 - 16.0) * 270.0 / 14.0, false)) { return false; }
    process_frame(); advance(1000U);
    if (!check(s_state.climate.target_temperature == 27U && s_action_count == actions + 37U &&
        lv_arc_get_value(arc) == (27 - 16) * 270 / 14,
        "ring release and local animations settle on the acknowledged target without replaying actions") ||
        !save_image(directory, "bento_ring_drag")) { return false; }

    actions = s_action_count;
    if (!queue_button_click("+") || !queue_button_click("+") || !queue_button_click("-") ||
        !queue_button_click("+") || !queue_button_click("-")) { return false; }
    process_frame(); advance(500U);
    if (!check(s_state.climate.target_temperature == 28U && s_action_count == actions + 5U,
        "five real plus/minus clicks queued before one UI frame accumulate instead of reusing a stale snapshot")) { return false; }

    reset_simulation(); advance(500U); actions = s_action_count;
    if (!queue_ring_pointer(arc, (26.0 - 16.0) * 270.0 / 14.0, true) ||
        !queue_ring_sweep(arc, 26U, 28U)) { return false; }
    process_frame();
    if (!press_key(PC_KEY_DOWN, false) || !queue_ring_sweep(arc, 28U, 29U)) { return false; }
    process_frame();
    if (!check(s_state.climate.target_temperature == 29U && s_action_count == actions + 5U,
        "an authoritative board-key snapshot during dragging rebases the next absolute-angle action")) { return false; }
    if (!pc_port_push_pointer(-20, -20, false)) { return false; }
    process_frame(); advance(500U);
    actions = s_action_count;
    if (!click("-") || !check(s_state.climate.target_temperature == 28U && s_action_count == actions + 1U,
        "releasing a drag outside the LCD clears capture and the next minus click works exactly once")) { return false; }

    if (!queue_ring_pointer(arc, (28.0 - 16.0) * 270.0 / 14.0, true) ||
        !queue_ring_sweep(arc, 28U, 30U) || !queue_ring_pointer(arc, 270.0, false)) { return false; }
    process_frame(); advance(500U);
    if (!click("+") || !check(s_state.climate.target_temperature == 30U,
        "the enlarged plus button preserves the 30 degree upper bound after ring dragging")) { return false; }
    if (!queue_ring_pointer(arc, 270.0, true) || !queue_ring_sweep(arc, 30U, 16U) ||
        !queue_ring_pointer(arc, 0.0, false)) { return false; }
    process_frame(); advance(500U);
    if (!click("-") || !check(s_state.climate.target_temperature == 16U,
        "the enlarged minus button preserves the 16 degree lower bound after ring dragging")) { return false; }
    reset_simulation(); advance(500U);
    if (!click("+") || !click("+") || !press_key('v', false)) { return false; }
    advance(300U);
    actions = s_action_count;
    if (!queue_ring_pointer(arc, (26.0 - 16.0) * 270.0 / 14.0, true) ||
        !queue_ring_sweep(arc, 26U, 27U) ||
        !queue_ring_pointer(arc, (27.0 - 16.0) * 270.0 / 14.0, false)) { return false; }
    process_frame(); advance(500U);
    if (!check(!s_demo.active && !s_state.demo_active && s_state.climate.target_temperature == 27U &&
        s_action_count == actions + 3U,
        "a real ring gesture ending DEMO rebases from restored preferences to its absolute target without stale actions")) { return false; }
    reset_simulation(); advance(1200U);
    return true;
}

static unsigned int v95_event_count(uint8_t type)
{
    const app_event_journal_t *journal = app_ui_event_records();
    unsigned int count = 0U;
    for (uint32_t i = 0U; i < journal->count; ++i) {
        const app_event_record_t *record = app_event_journal_latest(journal, i);
        if (record && record->meta.type == type) { ++count; }
    }
    return count;
}

static bool v95_all_record_sources(uint8_t source)
{
    const app_event_journal_t *journal = app_ui_event_records();
    if (journal->source != source || journal->capture_busy_id) { return false; }
    for (uint32_t i = 0U; i < journal->count; ++i) {
        const app_event_record_t *record = app_event_journal_latest(journal, i);
        if (!record || record->meta.source != source) { return false; }
    }
    return true;
}

static bool v95_climate_unchanged(const app_climate_state_t *before, unsigned int actions)
{
    const app_climate_state_t *after = &s_state.climate;
    return s_action_count == actions && after->power_on == before->power_on &&
        after->target_temperature == before->target_temperature &&
        after->mode == before->mode && after->fan_speed == before->fan_speed &&
        after->manual_sleep == before->manual_sleep &&
        after->manual_energy_saving == before->manual_energy_saving &&
        after->swing_lr == before->swing_lr && after->swing_ud == before->swing_ud &&
        after->turbo == before->turbo && after->lamp_on == before->lamp_on &&
        app_climate_output_mask(after) == app_climate_output_mask(before);
}

static bool v95_select_record(uint32_t index)
{
    /* These are the actual visible journal list rows, not direct LVGL event
       injection. A stale/hidden row cannot be selected through this helper. */
    return index < APP_EVENT_JOURNAL_CAPACITY && click_point(168, 152 + (int32_t)index * 60);
}

static bool v95_modal_touch_targets(const char *const names[], unsigned int count)
{
    lv_obj_update_layout(lv_screen_active());
    lv_area_t screen = {0, 0, LCD_FG_WIDTH - 1, LCD_FG_HEIGHT - 1};
    for (unsigned int i = 0U; i < count; ++i) {
        lv_obj_t *button = find_button(lv_screen_active(), names[i]);
        if (!button) { return check(false, "V9.5 lifecycle modal exposes all intended real touch buttons"); }
        lv_area_t target, caption, panel;
        lv_obj_get_coords(button, &target);
        lv_obj_get_coords(lv_obj_get_child(button, 0), &caption);
        lv_obj_get_coords(lv_obj_get_parent(button), &panel);
        char message[160];
        (void)snprintf(message, sizeof(message),
            "V9.5 lifecycle touch target %s is at least 44 px high with an unclipped caption inside its panel", names[i]);
        if (!check(lv_obj_get_height(button) >= 44 && area_inside(&caption, &target) &&
            area_inside(&target, &panel) && area_inside(&panel, &screen), message)) { return false; }
    }
    return true;
}

static bool v95_thumbnail_rendered(void)
{
    const app_event_journal_t *journal = app_ui_event_records();
    const app_event_record_t *record = app_event_journal_latest(journal, 0U);
    const uint16_t *pixels = record ? app_event_journal_image(journal, record->id) : NULL;
    lv_obj_t *image = find_record_thumbnail();
    if (!record || !pixels || !image || lv_obj_has_flag(image, LV_OBJ_FLAG_HIDDEN)) {
        return check(false, "V9.5 a successful snapshot has a visible real LVGL image");
    }
    const lv_image_dsc_t *descriptor = (const lv_image_dsc_t *)lv_image_get_src(image);
    if (!check(descriptor && descriptor->header.cf == LV_COLOR_FORMAT_RGB565 &&
        descriptor->header.w == 128U && descriptor->header.h == 128U &&
        descriptor->data == (const uint8_t *)pixels &&
        descriptor->data_size == APP_EVENT_THUMB_BYTES,
        "V9.5 snapshot renders its own fixed 128 by 128 RGB565 record pixels")) { return false; }
    lv_obj_update_layout(lv_screen_active());
    lv_area_t area;
    lv_obj_get_coords(image, &area);
    const uint16_t *foreground = (const uint16_t *)app_lcd_get_fg_draw_buffer();
    unsigned int matches = 0U, colorful = 0U;
    for (uint32_t y = 8U; y < 128U; y += 12U) {
        for (uint32_t x = 8U; x < 128U; x += 12U) {
            uint16_t pixel = pixels[y * 128U + x];
            uint16_t converted = (uint16_t)(0xF000U | ((pixel >> 12U) << 8U) |
                (((pixel >> 7U) & 0xFU) << 4U) | ((pixel >> 1U) & 0xFU));
            if (pixel && pixel != 0xFFFFU) { ++colorful; }
            if (foreground[(uint32_t)(area.y1 + (int32_t)y) * LCD_FG_WIDTH +
                (uint32_t)(area.x1 + (int32_t)x)] == converted) { ++matches; }
        }
    }
    printf("V9.5 thumbnail: colorful=%u/100 matching_rendered_pixels=%u/100\n", colorful, matches);
    return check(colorful >= 90U && matches >= 90U,
        "V9.5 saved synthetic image is nonempty and its RGB565 colors appear in the composed LVGL foreground");
}

static bool check_v95_environment(const char *directory)
{
    reset_simulation(); advance(1500U);
    const app_environment_alert_t *alerts = app_ui_environment_alerts();
    const app_event_journal_t *journal = app_ui_event_records();
    app_climate_state_t before = s_state.climate;
    unsigned int actions = s_action_count;
    static const char *const environment_buttons[] = {
        "温限 -", "温限 +", "湿限 -", "湿限 +", "时长 -", "时长 +", "确认提醒", "稍后提醒", "关闭"
    };
    if (!check(alerts->config.temperature_high_x10 == 300 && alerts->config.humidity_high == 70U &&
        alerts->config.hold_ms == 120000U && alerts->config.snooze_ms == 300000U,
        "V9.5 personal reminder defaults are 30 C, 70 percent, 120 s continuous and 5 min snooze") ||
        !check(v95_event_count(APP_EVENT_FIRST_PRESENT) == 1U &&
            v95_all_record_sources(APP_EVENT_SOURCE_SIMULATED),
            "V9.5 initial confirmed in-view presence creates exactly one explicitly PC-sourced observation") ||
        !save_image(directory, "v9_5_home") || !click("环境") ||
        !v95_modal_touch_targets(environment_buttons, 9U)) { return false; }
    if (!check(app_ui_environment_active() && !app_ui_journal_active() && !app_ui_controls_active() &&
        find_visible_label(lv_screen_active(), "PC 模拟 / 个人阈值") &&
        find_visible_label(lv_screen_active(), "不改变空调控制"),
        "V9.5 environment modal identifies personal simulated thresholds and owns the sole modal backdrop") ||
        !click("电源") || !click("记录") || !click("更多") ||
        !check(app_ui_environment_active() && !app_ui_journal_active() && !app_ui_controls_active() &&
            v95_climate_unchanged(&before, actions),
            "V9.5 real background pointer clicks cannot operate POWER, RECORD or MORE through the reminder backdrop")) { return false; }
    uint32_t idle_next_id = journal->next_id;
    if (!check(lv_obj_has_state(find_button(lv_screen_active(), "确认提醒"), LV_STATE_DISABLED) &&
        lv_obj_has_state(find_button(lv_screen_active(), "稍后提醒"), LV_STATE_DISABLED),
        "V9.5 acknowledgement and snooze remain disabled when no real reminder exists") ||
        !click("确认提醒") || !click("稍后提醒") ||
        !check(journal->next_id == idle_next_id && !alerts->channels[0].acknowledged &&
            !alerts->channels[1].snoozed && v95_climate_unchanged(&before, actions),
            "V9.5 actual pointer clicks on disabled reminder controls cannot fabricate events or preferences")) { return false; }
    if (!click("温限 -") || !click("湿限 +") ||
        !check(alerts->config.temperature_high_x10 == 290 && alerts->config.humidity_high == 75U &&
            find_visible_label(lv_screen_active(), "温度高限 29 C") &&
            find_visible_label(lv_screen_active(), "湿度高限 75 %"),
            "V9.5 real reminder setting buttons change and immediately display the selected personal limits") ||
        !click("温限 +") || !click("湿限 -") || !click("时长 -") ||
        !click("时长 -") || !click("时长 -") || !click("时长 -") ||
        !check(alerts->config.hold_ms == 30000U &&
            find_visible_label(lv_screen_active(), "连续超限 30 s") && v95_climate_unchanged(&before, actions),
            "V9.5 three duration decrements reach 30 s and further clicks clamp without changing controls")) { return false; }
    s_state.temperature = 31.0f; s_state.humidity = 80.0f;
    refresh_climate(app_ui_tick_ms()); publish(); advance(600U);
    app_environment_view_t view;
    app_environment_get_view(alerts, app_ui_tick_ms(), &view);
    if (!check(view.channels[0].status == APP_ENV_PENDING && view.channels[1].status == APP_ENV_PENDING &&
        find_visible_label(lv_screen_active(), "持续确认"),
        "V9.5 fresh 31 C and 80 percent start two continuous confirmation timers") ||
        !save_image(directory, "v9_5_environment_pending")) { return false; }
    uint32_t elapsed = app_ui_tick_ms() - alerts->channels[0].pending_since_ms;
    if (elapsed < 29999U) { advance(29999U - elapsed); }
    if (!check(!alerts->channels[0].raised && !alerts->channels[1].raised,
        "V9.5 neither channel raises before the configured continuous 30 s boundary")) { return false; }
    advance(1U); advance(220U);
    if (!check(alerts->channels[0].raised && alerts->channels[1].raised &&
        v95_event_count(APP_EVENT_ENV_TEMP_RAISED) == 1U &&
        v95_event_count(APP_EVENT_ENV_HUM_RAISED) == 1U &&
        find_visible_label(lv_screen_active(), "持续超限"),
        "V9.5 both channels raise once at the hold boundary and create distinct observable event records") ||
        !save_image(directory, "v9_5_environment_active")) { return false; }
    uint32_t next_id = journal->next_id;
    if (!click("确认提醒")) { return false; }
    app_environment_get_view(alerts, app_ui_tick_ms(), &view);
    if (!check(view.channels[0].status == APP_ENV_ACKNOWLEDGED &&
        view.channels[1].status == APP_ENV_ACKNOWLEDGED &&
        find_visible_label(lv_screen_active(), "已确认，仍超限") && journal->next_id == next_id &&
        v95_climate_unchanged(&before, actions),
        "V9.5 real acknowledgement retains both raised conditions and submits no actuator action or duplicate raise") ||
        !save_image(directory, "v9_5_environment_ack") || !click("稍后提醒")) { return false; }
    app_environment_get_view(alerts, app_ui_tick_ms(), &view);
    if (!check(view.channels[0].status == APP_ENV_SNOOZED && view.channels[1].status == APP_ENV_SNOOZED &&
        view.channels[0].snooze_remaining_ms > 299000U && !view.channels[0].acknowledged,
        "V9.5 real snooze selects five minutes of silence while retaining the actual active condition")) { return false; }
    uint32_t remaining = view.channels[0].snooze_remaining_ms;
    advance_model_time(remaining - 1U); process_frame();
    app_environment_get_view(alerts, app_ui_tick_ms(), &view);
    if (!check(view.channels[0].status == APP_ENV_SNOOZED && view.channels[0].snooze_remaining_ms == 1U,
        "V9.5 the reminder remains silent immediately before the exact five-minute expiry")) { return false; }
    advance(1U);
    app_environment_get_view(alerts, app_ui_tick_ms(), &view);
    if (!check(view.channels[0].status == APP_ENV_ACTIVE && view.channels[1].status == APP_ENV_ACTIVE &&
        journal->next_id == next_id && v95_climate_unchanged(&before, actions),
        "V9.5 snooze expiry restores the same active reminders without creating new raised events or control actions")) { return false; }
    s_state.temperature = 26.0f; s_state.humidity = 55.0f;
    s_sensor_online = false; s_state.sensor_valid = s_state.humidity_valid = 0U;
    s_state.dht.has_sample = 1U; s_state.dht.last_read_ok = 0U;
    refresh_climate(app_ui_tick_ms()); publish(); advance(600U);
    app_environment_get_view(alerts, app_ui_tick_ms(), &view);
    if (!check(view.channels[0].status == APP_ENV_UNKNOWN && view.channels[1].status == APP_ENV_UNKNOWN &&
        view.channels[0].raised && view.channels[1].raised && journal->next_id == next_id &&
        find_visible_label(lv_screen_active(), "保留原提醒"),
        "V9.5 cached apparently normal readings with failed quality remain UNKNOWN and cannot falsely recover") ||
        !save_image(directory, "v9_5_environment_unknown")) { return false; }
    s_sensor_online = true; s_state.temperature = 29.5f; s_state.humidity = 68.0f;
    advance(600U);
    if (!check(alerts->channels[0].raised && alerts->channels[1].raised && journal->next_id == next_id,
        "V9.5 fresh readings just below the high limits retain reminders until their 1 C and 3 percent recovery hysteresis")) { return false; }
    s_state.temperature = 26.0f; s_state.humidity = 55.0f; advance(600U);
    app_environment_get_view(alerts, app_ui_tick_ms(), &view);
    if (!check(view.channels[0].status == APP_ENV_NORMAL && view.channels[1].status == APP_ENV_NORMAL &&
        !view.channels[0].raised && !view.channels[1].raised &&
        v95_event_count(APP_EVENT_ENV_RECOVERY) == 2U && v95_climate_unchanged(&before, actions),
        "V9.5 fresh 26 C and 55 percent recover both channels once with separate recovery records")) { return false; }
    if (!click("关闭") || !check(!app_ui_environment_active(),
        "V9.5 CLOSE releases the reminder backdrop") || !click("更多") ||
        !check(app_ui_controls_active() && !app_ui_environment_active() && !app_ui_journal_active(),
            "V9.5 existing MORE controls remain available after closing the reminder") ||
        !click("完成") || !press_key('r', false)) { return false; }
    advance(1500U);
    return check(alerts->config.hold_ms == 120000U && alerts->config.temperature_high_x10 == 300 &&
        alerts->config.humidity_high == 70U && !app_ui_environment_active() && !app_ui_journal_active(),
        "V9.5 real R reset restores local reminder defaults and closes lifecycle dialogs");
}

static bool check_v95_journal(const char *directory)
{
    reset_simulation(); advance(1500U);
    const app_event_journal_t *journal = app_ui_event_records();
    app_climate_state_t before = s_state.climate;
    unsigned int actions = s_action_count;
    static const char *const journal_buttons[] = {"拍一张", "关闭"};
    uint32_t confirmed_next_id = journal->next_id;
    s_ai_online = false; advance(600U);
    if (!check(s_state.climate.presence == APP_PRESENCE_UNKNOWN &&
        journal->next_id == confirmed_next_id && v95_event_count(APP_EVENT_ABSENT) == 0U,
        "V9.5 a brief AI outage is UNKNOWN and never fabricates an absence journal event")) { return false; }
    s_ai_online = true; advance(1500U);
    if (!check(s_state.climate.presence == APP_PRESENCE_PRESENT &&
        journal->next_id == confirmed_next_id && v95_event_count(APP_EVENT_FIRST_PRESENT) == 1U,
        "V9.5 reconfirming the same occupied state after a brief AI fault preserves the existing event and four-record history")) { return false; }
    if (!click("记录") || !v95_modal_touch_targets(journal_buttons, 2U) ||
        !check(app_ui_journal_active() && !app_ui_environment_active() &&
        !app_ui_controls_active() && find_visible_label(lv_screen_active(), "仅当前检测视野") &&
        find_visible_label(lv_screen_active(), "来源：PC 模拟") && find_visible_label(lv_screen_active(), "图时"),
        "V9.5 journal identifies only the current detection view, PC source and actual display-image uptime") ||
        !click("电源") || !click("环境") || !click("更多") ||
        !check(app_ui_journal_active() && !app_ui_environment_active() && !app_ui_controls_active() &&
            v95_climate_unchanged(&before, actions),
            "V9.5 journal backdrop blocks underlying controls and other modal entry buttons") ||
        !click("拍一张")) { return false; }
    const app_event_record_t *snapshot = app_event_journal_latest(journal, 0U);
    if (!check(snapshot && snapshot->meta.type == APP_EVENT_MANUAL_SNAPSHOT &&
        snapshot->meta.source == APP_EVENT_SOURCE_SIMULATED && snapshot->image_status == APP_EVENT_IMAGE_READY &&
        snapshot->meta.temperature_quality == APP_EVENT_QUALITY_VALID && snapshot->meta.temperature_x10 == 270 &&
        snapshot->meta.humidity_quality == APP_EVENT_QUALITY_VALID && snapshot->meta.humidity_x10 == 550U &&
        snapshot->display_frame_id && snapshot->image_captured_ms <= snapshot->meta.run_ms &&
        find_visible_label(lv_screen_active(), "模拟图片") && v95_climate_unchanged(&before, actions),
        "V9.5 a real SNAPSHOT click records valid readings and a fresh display frame as an explicitly synthetic image") ||
        !v95_thumbnail_rendered() || !save_image(directory, "v9_5_journal")) { return false; }
    const app_event_record_t *original = app_event_journal_latest(journal, 1U);
    uint32_t original_id = original ? original->id : 0U;
    if (!v95_select_record(1U) || !check(original_id && find_visible_label(lv_screen_active(), "确认有人\n运行"),
        "V9.5 real list-row pointer selection chooses the original observation by stable record ID")) { return false; }
    s_state.people_count = 0; advance(1500U);
    if (!check(app_event_journal_lookup(journal, original_id) &&
        app_event_journal_latest(journal, 0U)->meta.type == APP_EVENT_ABSENT &&
        find_visible_label(lv_screen_active(), "确认有人\n运行") &&
        !find_visible_label(lv_screen_active(), "所选记录已"),
        "V9.5 a later absence event shifts list rows without changing the selected record or its image")) { return false; }
    s_state.people_count = 1; advance(1500U);
    s_state.people_count = 0; advance(1500U);
    lv_obj_t *image = find_record_thumbnail();
    if (!check(journal->count == APP_EVENT_JOURNAL_CAPACITY && !app_event_journal_lookup(journal, original_id) &&
        find_visible_label(lv_screen_active(), "所选记录已被新事件替换") && image &&
        lv_obj_has_flag(image, LV_OBJ_FLAG_HIDDEN),
        "V9.5 the bounded four-record ring explicitly reports selected-ID eviction and removes the old displayed image") ||
        !save_image(directory, "v9_5_journal_evicted") || !v95_select_record(0U) ||
        !check(find_visible_label(lv_screen_active(), "确认无人\n运行") &&
            !find_visible_label(lv_screen_active(), "所选记录已"),
            "V9.5 selecting a surviving row after eviction restores an actual available record") ||
        !click("关闭")) { return false; }
    reset_simulation(); advance(1500U);
    if (!click("记录") || !click("拍一张")) { return false; }
    uint32_t pc_image_id = app_event_journal_latest(journal, 0U)->id;
    s_mock_vision = false;
    s_state.simulated = 0U; s_state.temperature_source = APP_TEMPERATURE_DHT11;
    s_state.camera_init_status = s_state.camera_start_status = 0;
    refresh_climate(app_ui_tick_ms()); publish(); advance(220U);
    if (!check(v95_all_record_sources(APP_EVENT_SOURCE_BOARD) &&
        !app_event_journal_lookup(journal, pc_image_id) &&
        find_visible_label(lv_screen_active(), "所选记录已被新事件替换") &&
        lv_obj_has_flag(image, LV_OBJ_FLAG_HIDDEN),
        "V9.5 switching to a board-like fixture clears every PC record and displayed-image cache instead of relabeling them") ||
        !v95_select_record(0U)) { return false; }
    const app_event_record_t *board = app_event_journal_latest(journal, 0U);
    if (!check(board && board->meta.source == APP_EVENT_SOURCE_BOARD &&
        board->image_status == APP_EVENT_IMAGE_CAMERA_UNAVAILABLE &&
        !app_event_journal_image(journal, board->id) &&
        find_visible_label(lv_screen_active(), "来源：开发板实测") &&
        find_visible_label(lv_screen_active(), "相机不可用") && lv_obj_has_flag(image, LV_OBJ_FLAG_HIDDEN),
        "V9.5 board-like data never inherits a PC synthetic thumbnail even when board camera status is nominal") ||
        !click("拍一张") || !check(app_event_journal_latest(journal, 0U)->image_status == APP_EVENT_IMAGE_CAMERA_UNAVAILABLE &&
            v95_climate_unchanged(&before, actions),
            "V9.5 a board-like manual snapshot stays explicitly image-unavailable and never changes control requests") ||
        !click("关闭")) { return false; }
    reset_simulation(); advance(1500U);
    if (!click("数据") || !click("演示") || !click("开始回放") || !click("主页")) { return false; }
    advance(1500U);
    if (!click("记录") ||
        !check(s_demo.active && v95_all_record_sources(APP_EVENT_SOURCE_DEMO) &&
            find_visible_label(lv_screen_active(), "来源：演示数据"),
            "V9.5 the existing real DEMO controls open a separate explicitly identified demonstration journal") ||
        !click("关闭") || !click("数据") || !click("演示") || !click("停止并恢复") || !click("主页")) { return false; }
    advance(1500U);
    if (!check(!s_demo.active && v95_all_record_sources(APP_EVENT_SOURCE_SIMULATED),
        "V9.5 stopping the demo clears scripted records rather than mixing them into the restored PC session")) { return false; }
    reset_simulation(); advance(1500U);
    return true;
}

static bool check_v95_camera_failures(const char *directory)
{
    reset_simulation(); advance(1500U);
    const app_event_journal_t *journal = app_ui_event_records();
    app_climate_state_t before = s_state.climate;
    unsigned int actions = s_action_count;
    if (!click("记录")) { return false; }
    s_mock_vision = false; s_skip_automatic_samples = true;
    pc_port_camera_sample(100000U, app_ui_tick_ms(), false, NULL, 0U);
    if (!click("拍一张")) { return false; }
    const app_event_record_t *record = app_event_journal_latest(journal, 0U);
    lv_obj_t *image = find_record_thumbnail();
    if (!check(record && record->image_status != APP_EVENT_IMAGE_READY &&
        record->image_status != APP_EVENT_IMAGE_PENDING && !app_event_journal_image(journal, record->id) &&
        image && lv_obj_has_flag(image, LV_OBJ_FLAG_HIDDEN) &&
        (find_visible_label(lv_screen_active(), "相机不可用") ||
            find_visible_label(lv_screen_active(), "未取得可用画面")),
        "V9.5 unavailable camera sampling produces an explicit no-image record and removes the preceding thumbnail") ||
        !save_image(directory, "v9_5_camera_unavailable")) { return false; }
    pc_camera_person_t person = {200U, 150U, 120U, 260U};
    pc_port_camera_sample(0U, app_ui_tick_ms(), true, &person, 1U);
    if (!click("拍一张") || !check(app_event_journal_latest(journal, 0U)->image_status == APP_EVENT_IMAGE_READY &&
        app_event_journal_latest(journal, 0U)->display_frame_id == 0U,
        "V9.5 the first fresh frame after camera recovery produces an actual saved thumbnail including valid frame ID zero") ||
        !v95_thumbnail_rendered() || !click("拍一张")) { return false; }
    record = app_event_journal_latest(journal, 0U);
    if (!check(record && record->image_status == APP_EVENT_IMAGE_SKIPPED &&
        !app_event_journal_image(journal, record->id) &&
        find_visible_label(lv_screen_active(), "未取得新帧") && lv_obj_has_flag(image, LV_OBJ_FLAG_HIDDEN),
        "V9.5 repeated manual clicks on the identical frozen frame do not save it as a new photo")) { return false; }
    pc_port_camera_sample(100002U, app_ui_tick_ms() - APP_CAMERA_THUMBNAIL_MAX_AGE_MS - 1U,
        true, &person, 1U);
    if (!click("拍一张") || !check(app_event_journal_latest(journal, 0U)->image_status != APP_EVENT_IMAGE_READY &&
        lv_obj_has_flag(image, LV_OBJ_FLAG_HIDDEN),
        "V9.5 an available but expired camera frame cannot replace an actual fresh event image")) { return false; }
    pc_port_camera_sample(100003U, app_ui_tick_ms(), true, &person, 1U);
    if (!click("拍一张") || !check(app_event_journal_latest(journal, 0U)->image_status == APP_EVENT_IMAGE_READY &&
        app_event_journal_latest(journal, 0U)->display_frame_id == 100003U &&
        v95_climate_unchanged(&before, actions),
        "V9.5 a new current frame restores snapshot availability without actuator actions") ||
        !click("关闭")) { return false; }
    reset_simulation(); advance(1500U);
    if (!click("视觉") || !check(LCD_CAMERA_VIEW_WIDTH == 480U && LCD_CAMERA_VIEW_HEIGHT == 480U &&
        LCD_CAMERA_VIEW_X == 160U && LCD_CAMERA_VIEW_Y == 0U,
        "V9.5 the PC camera occupies the same centered square 480 by 480 detection viewport")) { return false; }
    uint16_t camera_center = 0U;
    app_camera_thumbnail_info_t camera_info;
    if (!check(app_lcd_copy_camera_thumbnail(&camera_center, 1U, 1U, &camera_info) &&
        camera_center != 0U && camera_info.source_x == 160U && camera_info.source_y == 0U &&
        camera_info.source_width == 480U && camera_info.source_height == 480U &&
        camera_info.simulated && camera_info.frame_id == s_state.vision.frame_id &&
        camera_info.captured_ms == s_state.vision.captured_ms,
        "V9.5 actual camera thumbnail sampling reports the centered crop and real simulated display-frame provenance") ||
        !save_image(directory, "v9_5_vision_square") || !click("返回主页")) { return false; }
    reset_simulation(); advance(1500U);
    return true;
}

static bool check_v95_lifecycle(const char *directory)
{
    return check_v95_environment(directory) && check_v95_journal(directory) &&
        check_v95_camera_failures(directory);
}

static bool check_v10_model_view(const char *directory)
{
    reset_simulation(); advance(1500U);
    if (!click("视觉") ||
        !check(!find_visible_label(lv_screen_active(), APP_MODEL_NAME) &&
            !find_visible_label(lv_screen_active(), "256 x 256 INT8") &&
            !find_button(lv_screen_active(), "框：稳定") &&
            find_visible_label(lv_screen_active(), "FPS --") &&
            find_visible_label(lv_screen_active(), "AI -- Hz"),
            "V10 preview hides model labels and stable-mode controls while PC hardware rates remain unavailable") ||
        !save_image(directory, "v10_model")) { return false; }
    unsigned int actions = s_action_count;
    uint8_t mask = app_climate_output_mask(&s_state.climate);
    s_mock_vision = false;
    s_skip_automatic_samples = true;
    s_ai_online = false;
    s_state.people_valid = 0U;
    s_state.vision.valid = 0U;
    s_state.ai.stage = APP_AI_STAGE_ERROR;
    s_state.ai.error = APP_AI_ERROR_MODEL_WEIGHTS;
    s_state.camera_init_status = s_state.camera_start_status = -1001;
    refresh_climate(app_ui_tick_ms()); publish(); advance(220U);
    if (!check(find_visible_label(lv_screen_active(), "Camera MODEL") &&
            find_visible_label(lv_screen_active(), "AI -- Hz"),
            "V10 a simulated mismatching-weight status identifies MODEL rather than a camera initialization fault") ||
        !click("诊断") ||
        !check(find_visible_label(lv_screen_active(), "模型权重不匹配") &&
            find_visible_label(lv_screen_active(), APP_MODEL_DESCRIPTION),
            "V10 DIAG explains mismatching model weights with the shared configured model description") ||
        !save_image(directory, "v10_model_weights_mismatch")) { return false; }
    s_state.ai.error = -1202;
    publish(); advance(1250U);
    if (!check(find_visible_label(lv_screen_active(), "模型接口不匹配") &&
            find_visible_label(lv_screen_active(), "Camera MODEL"),
            "V10 a simulated tensor-contract failure has its own model-interface diagnostic") ||
        !save_image(directory, "v10_model_io_mismatch") ||
        !check(s_action_count == actions && app_climate_output_mask(&s_state.climate) == mask &&
            !s_state.climate.power_on && s_state.climate.target_temperature == 26U,
            "V10 model diagnostic navigation injects no control action or load request") ||
        !click("返回主页")) { return false; }
    reset_simulation(); advance(1500U);
    return check(s_mock_vision && s_ai_online && s_state.ai.error == 0,
        "V10 reset restores PC simulated observations after model-error presentation checks");
}

static bool self_test(const char *directory)
{
    pc_port_use_test_clock();
    if (!check_wall_scheduler()) { return false; }
    if (!check_demo_scene(directory)) { return false; }
    if (!check_v9_pages(directory)) { return false; }
    if (!check_dht_presentation(directory)) { return false; }
    if (!check_v93_controls(directory) || !check_v93_chart_details(directory) ||
        !check_v93_stable_overlay(directory)) { return false; }
    if (!check_v94_theme(directory)) { return false; }
    if (!check_watch_home(directory)) { return false; }
    if (!check_bento_ring(directory)) { return false; }
    if (!check_v95_lifecycle(directory)) { return false; }
    if (!check_v10_model_view(directory)) { return false; }
    reset_simulation();
    if (!check(!s_state.climate.power_on && s_state.climate.target_temperature == 26U &&
               s_state.temperature == 27.0f && s_state.humidity == 55.0f &&
               s_state.people_count == 1 && s_state.als == 250U,
               "default state OFF/26, room 27/55, occupied/bright")) { return false; }
    lv_display_t *display = lv_display_get_next(NULL);
    if (!check(display && !lv_display_get_next(display), "single shared LVGL display") ||
        !check(lv_indev_get_next(NULL) != NULL, "real LVGL pointer registered")) { return false; }
    advance(1500U);
    if (!check(s_state.climate.presence == APP_PRESENCE_PRESENT, "occupied input confirmed")) { return false; }
    unsigned int actions = s_action_count;
    if (!click("电源") || !check(s_state.climate.power_on && s_action_count == actions + 1U,
        "rapid mouse down/up invokes POWER once")) { return false; }
    if (!click("+") || !check(s_state.climate.target_temperature == 27U, "target increases")) { return false; }
    if (!click("-") || !check(s_state.climate.target_temperature == 26U, "target decreases")) { return false; }
    if (!click("风速") || !click("风档 2") ||
        !check(s_state.climate.fan_speed == 2U, "FAN dialog selects an explicit speed") ||
        !click("风档 5") || !click("风档 1") || !click("完成")) { return false; }
    if (!click("摆风") || !click("左右摆风") ||
        !check(s_state.climate.swing_lr, "SWING dialog changes the existing left/right preference") ||
        !click("完成")) { return false; }
    if (!click("节能") || !check(s_state.climate.effective_eco &&
        s_state.climate.effective_target_temperature == 28U, "ECO changes execution target")) { return false; }
    if (!click("睡眠") || !check(s_state.climate.effective_sleep &&
        s_state.climate.effective_fan_speed == 1U, "SLEEP changes effective state")) { return false; }
    if (!click("睡眠") || !click("节能")) { return false; }
    if (!click("强劲") || !check(s_state.climate.effective_fan_speed == 5U, "TURBO requests fifth speed")) { return false; }
    if (!click("强劲")) { return false; }
    if (!save_image(directory, "user")) { return false; }
    lv_obj_t *user_page = lv_screen_active();
    if (!click("视觉") || !check(lv_screen_active() != user_page, "DEBUG opens camera preview")) { return false; }
    lv_obj_t *diagnostic_title = find_label_containing(lv_screen_active(), "调试诊断");
    lv_obj_t *diagnostic_panel = diagnostic_title ? lv_obj_get_parent(diagnostic_title) : NULL;
    if (!check(diagnostic_panel && lv_obj_has_flag(diagnostic_panel, LV_OBJ_FLAG_HIDDEN),
        "DEBUG keeps full diagnostics hidden by default") ||
        !check(camera_preview_uncovered(), "default DEBUG leaves camera image area transparent") ||
        !check(find_label_containing(lv_screen_active(), "FPS --") != NULL,
        "PC preview does not invent a hardware camera frame rate") ||
        !check_detection_overlay(directory) ||
        !save_image(directory, "debug")) { return false; }
    if (!click("诊断") || !check(!lv_obj_has_flag(diagnostic_panel, LV_OBJ_FLAG_HIDDEN) &&
        find_label_containing(diagnostic_panel, "CAM CFG") != NULL,
        "DIAG opens all original hardware diagnostics") ||
        !check_debug_refresh_throttle(diagnostic_panel) ||
        !save_image(directory, "debug-details")) { return false; }
    if (!click("诊断") || !check(lv_obj_has_flag(diagnostic_panel, LV_OBJ_FLAG_HIDDEN) &&
        camera_preview_uncovered(),
        "DIAG closes diagnostics and restores camera preview") || !click("诊断") ||
        !click("返回主页") ||
        !check(lv_screen_active() == user_page, "USER MODE restores user page")) { return false; }
    if (!click("视觉") || !check(lv_obj_has_flag(diagnostic_panel, LV_OBJ_FLAG_HIDDEN),
        "re-entering DEBUG always starts with the full preview") ||
        !click("返回主页")) { return false; }
    if (!press_key('t', false) || !check(s_state.temperature == 28.0f, "SDL T raises actual temperature") ||
        !press_key('t', true) || !check(s_state.temperature == 27.0f, "SDL Shift+T lowers temperature") ||
        !press_key('h', false) || !check(s_state.humidity == 56.0f, "SDL H raises humidity") ||
        !press_key('h', true) || !check(s_state.humidity == 55.0f, "SDL Shift+H lowers humidity")) { return false; }
    s_state.humidity = NAN;
    advance(600U);
    if (!check(s_state.climate.sensor_fresh && !s_state.climate.humidity_fresh &&
               find_visible_label(user_page, "温度有效 / 湿度无效") != NULL &&
               sensor_reading_is(false, "27.0") &&
               sensor_reading_is(true, "--"),
               "invalid humidity remains separate from valid displayed control temperature") ||
        !inspect_home_details("温差 +1.0 C", "数值无效",
               "actual strategy details retain a valid temperature delta and independently invalid humidity")) { return false; }
    s_state.humidity = 55.0f;
    advance(600U);
    s_state.temperature = NAN;
    advance(600U);
    if (!check(!s_state.climate.sensor_fresh && !s_state.climate.cooling_request &&
               sensor_reading_is(false, "--"),
               "non-finite temperature cannot produce a numeric difference or cooling request") ||
        !inspect_home_details("温差 未知", "数值无效",
               "non-finite temperature remains explicitly unknown in the real strategy panel")) { return false; }
    s_state.temperature = 27.0f;
    advance(600U);
    if (!press_key('d', false)) { return false; } advance(1500U);
    if (!check(s_state.climate.auto_sleep && s_state.climate.presence == APP_PRESENCE_SLEEP,
        "dark occupied room activates automatic sleep")) { return false; }
    if (!press_key('a', false)) { return false; } advance(600U);
    if (!check(!s_state.climate.als_fresh && !s_state.climate.auto_sleep,
        "light failure cancels dark-light sleep inference")) { return false; }
    if (!press_key('a', false) || !press_key('b', false)) { return false; } advance(600U);
    if (!press_key('s', false)) { return false; } advance(600U);
    if (!check(!s_state.climate.sensor_fresh && !s_state.climate.cooling_request &&
        s_state.climate.reason == APP_REASON_SENSOR_INVALID, "temperature failure stops cooling request")) { return false; }
    if (!save_image(directory, "sensor-offline") || !press_key('s', false)) { return false; } advance(600U);
    if (!check(s_state.climate.sensor_fresh, "temperature recovers")) { return false; }
    if (!press_key('c', false)) { return false; } advance(600U);
    if (!check(s_state.climate.presence == APP_PRESENCE_UNKNOWN && !s_state.climate.absence_ms,
        "AI offline means unknown, not absent")) { return false; }
    if (!save_image(directory, "ai-offline") || !press_key('c', false)) { return false; } advance(1500U);
    s_skip_automatic_samples = true;
    advance(APP_AI_STALE_MS + 600U);
    if (!check(!s_state.climate.people_fresh && s_state.climate.presence == APP_PRESENCE_UNKNOWN,
        "valid but stale AI result becomes unknown")) { return false; }
    s_skip_automatic_samples = false; advance(1500U);
    if (!press_key('f', false) || !check(s_state.simulation_speed == 60U, "F selects 60x virtual time")) { return false; }
    uint32_t before_fast_tick = app_ui_tick_ms();
    apply_wall_delta(10U);
    if (!check(app_ui_tick_ms() - before_fast_tick == 600U, "60x advances 600 virtual ms per 10 wall ms") ||
        !press_key('f', false) || !check(s_state.simulation_speed == 1U, "F restores realtime")) { return false; }
    lv_obj_t *power_button = find_button(lv_screen_active(), "电源");
    lv_area_t power_area;
    lv_obj_get_coords(power_button, &power_area);
    int32_t px = (power_area.x1 + power_area.x2) / 2;
    int32_t py = (power_area.y1 + power_area.y2) / 2;
    unsigned int reset_actions = s_action_count;
    if (!pc_port_push_pointer(px, py, true)) { return false; } process_frame();
    if (!press_key('r', false) || !pc_port_push_pointer(px, py, false)) { return false; }
    process_frame();
    if (!check(!s_state.climate.power_on && s_action_count == reset_actions,
        "R cancels an in-flight mouse gesture without a late POWER click")) { return false; }
    advance(1500U);
    if (!check(!s_state.climate.power_on && s_state.climate.target_temperature == 26U,
        "R resets simulation preferences and faults")) { return false; }
    if (!click("电源")) { return false; }
    advance(APP_COOLING_MIN_OFF_MS + 600U);
    if (!check(s_state.climate.cooling_request, "temperature demand starts after software off protection")) { return false; }
    s_state.temperature = 25.0f; advance(600U);
    if (!check(!s_state.climate.cooling_request, "low temperature ends cooling request")) { return false; }
    s_state.temperature = 29.0f; advance(600U);
    if (!check(!s_state.climate.cooling_request && s_state.climate.cooling_wait_ms,
        "immediate restart is delayed by minimum off interval")) { return false; }
    if (!save_image(directory, "cooling-wait")) { return false; }
    if (!press_key('0', false)) { return false; } advance(1500U);
    if (!check(s_state.climate.presence == APP_PRESENCE_ABSENT, "zero people confirmed absent")) { return false; }
    advance(APP_ABSENCE_ECO_MS - s_state.climate.absence_ms - 1U -
            (app_ui_tick_ms() - s_last_climate_tick));
    if (!check(!s_state.climate.auto_energy_saving, "before ten-minute boundary remains normal")) { return false; }
    advance(500U);
    if (!check(s_state.climate.auto_energy_saving && s_state.climate.power_on,
        "ten minutes enables automatic ECO")) { return false; }
    advance(APP_ABSENCE_OFF_MS - s_state.climate.absence_ms - 1U -
            (app_ui_tick_ms() - s_last_climate_tick));
    if (!check(s_state.climate.power_on, "before thirty-minute boundary power stays on")) { return false; }
    advance(500U);
    if (!check(!s_state.climate.power_on && !app_climate_output_mask(&s_state.climate),
        "thirty minutes switches off all requests")) { return false; }
    if (!save_image(directory, "auto-off") || !click("电源")) { return false; } advance(600U);
    if (!check(s_state.climate.power_on && s_state.climate.absence_ms < 2000U,
        "manual restart does not inherit saturated absence timer")) { return false; }
    if (!click("手动制冷") || !check(s_state.climate.mode == APP_CLIMATE_MODE_MANUAL_COOL,
        "explicit MANUAL mode selects manual cooling")) { return false; }
    advance(APP_ABSENCE_OFF_MS + 600U);
    if (!check(s_state.climate.power_on && !s_state.climate.auto_energy_saving,
        "manual mode ignores automatic absence shutdown")) { return false; }
    if (!save_image(directory, "manual") || !press_key(PC_KEY_ESCAPE, false) ||
        !check(!s_running, "Escape requests normal exit")) { return false; }
    s_running = true;
    if (!pc_port_push_quit()) { return false; } process_frame();
    if (!check(!s_running, "SDL_QUIT requests normal exit")) { return false; }
    lv_mem_monitor_t heap;
    lv_mem_monitor(&heap);
    lv_obj_t *detail_text = find_label_containing(diagnostic_panel, "CAM CFG");
    size_t diagnostic_bytes = detail_text ? strlen(lv_label_get_text(detail_text)) + 1U : 0U;
    printf("LVGL heap: total=%zu peak=%zu free=%zu largest=%zu diagnostic_label=%zu\n",
        heap.total_size, heap.max_used, heap.free_size, heap.free_biggest_size, diagnostic_bytes);
    /* Reserve enough space to replace two full diagnostic strings; this is a
       workload-derived margin, rather than assuming a fixed spare-heap size. */
    return check(lv_mem_test() == LV_RESULT_OK, "LVGL heap integrity after complete SDL self-test") &&
        check(diagnostic_bytes != 0U && heap.free_size >= diagnostic_bytes * 2U &&
              heap.free_biggest_size >= diagnostic_bytes,
              "LVGL heap retains space for full diagnostic label replacements");
}

static void reset_simulation(void)
{
    app_demo_init(&s_demo);
    s_have_saved_demo = false;
    memset(&s_state, 0, sizeof(s_state));
    app_climate_init(&s_state.climate, 26U);
    s_state.temperature = 27.0f;
    s_state.humidity = 55.0f;
    s_state.people_count = 1;
    s_state.als = 250U;
    s_state.people_valid = s_state.als_valid = s_state.sensor_valid = 1U;
    s_state.humidity_valid = 1U;
    s_state.people_updated_ms = s_state.als_updated_ms = s_state.sensor_updated_ms = app_ui_tick_ms();
    s_state.humidity_updated_ms = app_ui_tick_ms();
    s_state.temperature_source = APP_TEMPERATURE_SIMULATED;
    s_state.simulated = 1U;
    s_state.simulation_speed = 1U;
    s_state.touch.ready = 1U;
    s_state.touch.last_success_ms = app_ui_tick_ms();
    s_state.camera_init_status = -1001;
    s_state.ai.stage = 9U; /* Hardware AI pipeline is disabled on the PC. */
    s_state.camera_start_status = -1001;
    s_state.camera.config_status = -1001;
    s_state.camera.sensor_test_pattern_mode = -1;
    s_state.camera.sensor_test_pattern_status = -1001;
    s_state.camera.isp_run_status = -1001;
    s_state.camera.isp_native_status = -1001;
    s_state.lcd.pitch_bytes = LCD_CAMERA_WIDTH * 2U;
    s_state.lcd.foreground_pitch_bytes = LCD_FG_WIDTH * 2U;
    s_ai_online = s_als_online = s_sensor_online = true;
    s_mock_vision = true;
    s_person_right = false;
    s_mock_jitter = false;
    s_skip_automatic_samples = false;
    s_last_sample_tick = s_last_climate_tick = app_ui_tick_ms();
    s_have_log = false;
    app_ui_reset_lifecycle();
    update_mock_vision(app_ui_tick_ms());
    refresh_climate(app_ui_tick_ms());
    publish();
}

static bool parse_duration(const char *text, uint32_t *duration)
{
    char *end;
    unsigned long value = strtoul(text, &end, 10);
    if (!*text || *end || value > 86400000UL) { return false; }
    *duration = (uint32_t)value;
    return true;
}

int main(int argc, char **argv)
{
    bool test = false;
    bool hidden = false;
    uint32_t duration = 0U;
    const char *output_dir = ".";
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--self-test") == 0) { test = true; }
        else if (strcmp(argv[i], "--hidden") == 0) { hidden = true; }
        else if (strcmp(argv[i], "--output-dir") == 0 && i + 1 < argc) { output_dir = argv[++i]; }
        else if (strcmp(argv[i], "--duration-ms") == 0 && i + 1 < argc &&
                 parse_duration(argv[i + 1], &duration)) { i++; }
        else {
            fprintf(stderr, "Usage: %s [--self-test] [--output-dir EXISTING_DIR] [--duration-ms N] [--hidden]\n", argv[0]);
            return 2;
        }
    }
    if (!pc_port_init(hidden)) {
        fprintf(stderr, "SDL startup failed: %s\n", pc_port_last_error());
        pc_port_shutdown();
        return 1;
    }
    reset_simulation();
    app_ui_init(apply_action);
    app_ui_set_demo_callback(handle_demo_command);
    /* The UI owns the only display and pointer. PC mouse events explicitly
       drive that pointer; no second SDL/LVGL display or input is created. */
    for (lv_indev_t *indev = lv_indev_get_next(NULL); indev;
         indev = lv_indev_get_next(indev)) {
        lv_indev_set_mode(indev, LV_INDEV_MODE_EVENT);
    }
    s_last_climate_tick = app_ui_tick_ms();
    process_frame();
    printf("PC simulation: C/A/S toggle AI/light/temperature faults; F toggles 1x/60x virtual time; R resets.\n");
    printf("PC simulation: 0/1/2 people; J/K move mock person left/right; D/B dark/bright; T/Shift+T temperature +/-1; "
           "H/Shift+H humidity +/-1; P/M/Down/Up board keys; V scene start/pause; "
           "X scene stop/restore; Escape closes. All scene data are simulated.\n");
    int result = 0;
    if (test) {
        result = self_test(output_dir) ? 0 : 1;
        printf("Self-test %s\n", result == 0 ? "PASS" : "FAIL");
    } else {
        uint32_t started = pc_port_wall_tick_ms();
        s_last_wall_tick = started;
        while (s_running && (!duration || pc_port_wall_tick_ms() - started < duration)) {
            uint32_t wall_tick = pc_port_wall_tick_ms();
            run_wall_frame(wall_tick - s_last_wall_tick);
            s_last_wall_tick = wall_tick;
            pc_port_sleep_ms(5U);
        }
    }
    lv_deinit();
    pc_port_shutdown();
    return result;
}
