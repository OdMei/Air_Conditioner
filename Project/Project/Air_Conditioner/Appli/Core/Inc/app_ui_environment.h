#ifndef APP_UI_ENVIRONMENT_H
#define APP_UI_ENVIRONMENT_H

#include <stddef.h>
#include "app_environment_alert.h"
#include "lvgl.h"

typedef void (*app_ui_environment_event_cb_t)(app_environment_events_t events);
void app_ui_environment_init(app_environment_alert_t *state,
                              app_ui_environment_event_cb_t event_cb);
void app_ui_environment_open(lv_obj_t *parent);
void app_ui_environment_close(void);
uint8_t app_ui_environment_active(void);
void app_ui_environment_update(void);
/* Returns whether a raised or confirming personal-threshold condition is
 * displayed. UNKNOWN is never described as normal or recovered. */
uint8_t app_ui_environment_summary(char *text, size_t capacity);

#endif
