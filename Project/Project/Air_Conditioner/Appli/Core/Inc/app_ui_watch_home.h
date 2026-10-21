#ifndef APP_UI_WATCH_HOME_H
#define APP_UI_WATCH_HOME_H

#include "app_ui.h"
#include "app_ui_insights.h"
#include "lvgl.h"

#define APP_UI_WATCH_OPEN_ENVIRONMENT (1U)
#define APP_UI_WATCH_OPEN_JOURNAL     (2U)
#define APP_UI_WATCH_OPEN_MORE        (3U)
#define APP_UI_WATCH_OPEN_DETAILS     (4U)

typedef void (*app_ui_watch_command_cb_t)(uint8_t command);

/* The snapshot address remains owned by app_ui. Buttons consult its latest
 * state. Only the display owner may call this module. Ring and buttons edit
 * a setpoint cursor until the next controller snapshot confirms the value;
 * execution states remain authoritative. Both inputs use TEMP_UP / DOWN. */
lv_obj_t *app_ui_watch_home_create(const app_ui_snapshot_t *snapshot,
                                   app_ui_action_cb_t action,
                                   app_ui_navigation_cb_t navigate,
                                   app_ui_watch_command_cb_t command);
void app_ui_watch_home_update(const char *reason, const char *reminder,
                              uint8_t attention);
void app_ui_watch_home_enter(void);
void app_ui_watch_home_observe_target(uint8_t target);
void app_ui_watch_home_cancel_input(void);
void app_ui_watch_home_show_details(void);
void app_ui_watch_home_close_details(void);

#endif
