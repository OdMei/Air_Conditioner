#ifndef APP_UI_THEME_H
#define APP_UI_THEME_H

#include <stdbool.h>
#include "lvgl.h"

#define APP_UI_COLOR_BACKGROUND (0x0B1420U)
#define APP_UI_COLOR_CARD       (0x152333U)
#define APP_UI_COLOR_BORDER     (0x2A4056U)
#define APP_UI_COLOR_TEXT       (0xEAF3F8U)
#define APP_UI_COLOR_MUTED      (0x93A9BAU)
#define APP_UI_COLOR_ACCENT     (0x64DFC0U)
#define APP_UI_COLOR_CAUTION    (0xFFC46BU)
#define APP_UI_COLOR_BUTTON     (0x263D52U)
#define APP_UI_COLOR_BUTTON_ON  (0x276B64U)
#define APP_UI_COLOR_PRESSED    (0x37627AU)

/* All styles have module-static lifetime and are initialized once after
 * lv_init(), from the existing LVGL owner. Factories reuse those styles and
 * remove the default theme's size-changing pressed transitions. */
void app_ui_theme_init(void);
lv_obj_t *app_ui_theme_screen(bool transparent);
lv_obj_t *app_ui_theme_card(lv_obj_t *parent, int32_t x, int32_t y,
                            int32_t width, int32_t height);
/* Factory captions use lv_label_set_text_static: text must be a literal or
 * storage that stays valid for the object's entire lifetime. Runtime text
 * from temporary buffers must instead use app_ui_theme_text(). */
lv_obj_t *app_ui_theme_label(lv_obj_t *parent, const char *lifetime_static_text,
                             int32_t x, int32_t y, int32_t width);
lv_obj_t *app_ui_theme_button(lv_obj_t *parent, const char *lifetime_static_text,
                              int32_t x, int32_t y, int32_t width, int32_t height);
/* CHECKED reflects an authoritative snapshot. Buttons are deliberately not
 * CHECKABLE, so a click cannot optimistically change a control preference. */
void app_ui_theme_checked(lv_obj_t *button, bool checked);
/* Copies changed text into LVGL-owned storage, safe for stack buffers. */
void app_ui_theme_text(lv_obj_t *label, const char *text);
lv_obj_t *app_ui_theme_row(lv_obj_t *parent, int32_t x, int32_t y,
                           int32_t width, int32_t height, int32_t gap);
void app_ui_theme_text_muted(lv_obj_t *object);
void app_ui_theme_text_accent(lv_obj_t *object);
void app_ui_theme_text_caution(lv_obj_t *object);

#endif
