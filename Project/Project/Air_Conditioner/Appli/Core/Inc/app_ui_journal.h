#ifndef APP_UI_JOURNAL_H
#define APP_UI_JOURNAL_H

#include "app_event_journal.h"
#include "lvgl.h"

typedef void (*app_ui_journal_capture_cb_t)(void *context);

void app_ui_journal_init(app_ui_journal_capture_cb_t capture, void *context);
void app_ui_journal_open(lv_obj_t *parent, app_event_journal_t *journal);
void app_ui_journal_close(void);
uint8_t app_ui_journal_active(void);
void app_ui_journal_update(void);

#endif
