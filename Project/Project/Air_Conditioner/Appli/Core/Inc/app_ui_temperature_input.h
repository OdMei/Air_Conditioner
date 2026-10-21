#ifndef APP_UI_TEMPERATURE_INPUT_H
#define APP_UI_TEMPERATURE_INPUT_H

#include "app_ui.h"
#include "lvgl.h"

typedef void (*app_ui_temperature_preview_cb_t)(uint8_t target);

/* Only the LVGL owner calls this module. The action callback applies a
 * TEMP_UP/DOWN synchronously; observe_target is called for every newly
 * published snapshot, including a callback that restores simulation state.
 * Preview changes the edited setpoint only, never execution state. */
void app_ui_temperature_input_init(lv_obj_t *arc,
                                   app_ui_action_cb_t action,
                                   app_ui_temperature_preview_cb_t preview);
void app_ui_temperature_input_observe_target(uint8_t target);
void app_ui_temperature_input_step(int8_t direction);
uint8_t app_ui_temperature_input_target(uint8_t fallback);
uint8_t app_ui_temperature_input_dragging(void);
void app_ui_temperature_input_cancel(void);

#endif
