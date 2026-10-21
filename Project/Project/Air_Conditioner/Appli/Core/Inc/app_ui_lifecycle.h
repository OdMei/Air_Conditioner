#ifndef APP_UI_LIFECYCLE_H
#define APP_UI_LIFECYCLE_H

#include "app_environment_alert.h"
#include "app_event_journal.h"

/* Owner-thread read-only views for validation and future presentation.
 * Reset is a local preview/session reset, never an actuator command. */
const app_environment_alert_t *app_ui_environment_alerts(void);
const app_event_journal_t *app_ui_event_records(void);
void app_ui_reset_lifecycle(void);

#endif
