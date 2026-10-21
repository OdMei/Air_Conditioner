#ifndef APP_UI_INSIGHTS_H
#define APP_UI_INSIGHTS_H

#include <stddef.h>
#include <stdint.h>
#include "app_ui.h"

#define APP_UI_PAGE_HOME (0U)
#define APP_UI_PAGE_VISION (1U)
#define APP_UI_PAGE_DATA (2U)
#define APP_UI_PAGE_DEMO (3U)

typedef void (*app_ui_navigation_cb_t)(uint8_t page);
/* All methods are called by the sole LVGL/display owner. No ISR or worker
 * may update this reducer or create page objects. */
void app_ui_insights_init(app_ui_navigation_cb_t navigate, app_ui_demo_cb_t demo);
void app_ui_insights_observe(const app_ui_snapshot_t *snapshot, uint32_t now_ms);
/* Current temperature error and detector age; unavailable inputs remain unknown. */
void app_ui_insights_sensing_text(const app_ui_snapshot_t *snapshot,
                                  char *text, size_t capacity);
/* DATA charts offer a read-only detail view. Selection is by copied elapsed
 * timestamp/session, never a retained pointer into the history ring. */
void app_ui_insights_show(uint8_t page);
uint8_t app_ui_insights_active(void);
void app_ui_insights_update(const app_ui_snapshot_t *snapshot, const char *reason);

#endif
