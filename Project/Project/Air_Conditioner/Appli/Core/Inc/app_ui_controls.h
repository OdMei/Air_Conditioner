#ifndef APP_UI_CONTROLS_H
#define APP_UI_CONTROLS_H

#include <stdint.h>
#include "app_ui.h"
#include "lvgl.h"

#define APP_UI_CONTROLS_FAN (1U)
#define APP_UI_CONTROLS_SWING (2U)
#define APP_UI_CONTROLS_MORE (3U)

/* Called only by the existing LVGL owner. The persistent modal belongs to
 * parent (HOME); presenting, updating, or closing it never changes policy.
 * Each control click calls the shared action callback exactly once. Changes
 * become visible only when a new control snapshot has been published. */
void app_ui_controls_init(lv_obj_t *parent, app_ui_action_cb_t action);
void app_ui_controls_show(uint8_t kind, const app_ui_snapshot_t *snapshot);
void app_ui_controls_update(const app_ui_snapshot_t *snapshot);
void app_ui_controls_close(void);
uint8_t app_ui_controls_active(void);

#endif
