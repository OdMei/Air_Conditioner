#ifndef APP_UI_WATCH_ASSETS_H
#define APP_UI_WATCH_ASSETS_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

#define APP_UI_WATCH_BACKGROUND_WIDTH 200
#define APP_UI_WATCH_BACKGROUND_HEIGHT 120
#define APP_UI_WATCH_BACKGROUND_SCALE 1024
#define APP_UI_WATCH_ICON_SIZE 28

/* Digits 0..9 only. 96px line height, 12px baseline; units use existing fonts. */
extern const lv_font_t app_font_watch_96;

/* Native little-endian RGB565. Scale 4x with pivot (0,0) for the 800x480 panel. */
extern const lv_image_dsc_t app_ui_watch_background;

typedef enum {
    APP_UI_WATCH_ICON_HOME = 0,
    APP_UI_WATCH_ICON_DATA,
    APP_UI_WATCH_ICON_HISTORY,
    APP_UI_WATCH_ICON_ALERT,
    APP_UI_WATCH_ICON_VISION,
    APP_UI_WATCH_ICON_FAN,
    APP_UI_WATCH_ICON_SWING,
    APP_UI_WATCH_ICON_MOON,
    APP_UI_WATCH_ICON_ECO,
    APP_UI_WATCH_ICON_TURBO,
    APP_UI_WATCH_ICON_LIGHT,
    APP_UI_WATCH_ICON_POWER,
    APP_UI_WATCH_ICON_SNOW,
    APP_UI_WATCH_ICON_MORE,
    APP_UI_WATCH_ICON_COUNT
} app_ui_watch_icon_t;

/* 28x28 alpha masks with 2px strokes; display at 1:1, without image scaling.
 * Set image_recolor and LV_OPA_COVER for the desired color.
 * Returns NULL for invalid IDs. No filesystem, decoder or temporary canvas. */
const lv_image_dsc_t *app_ui_watch_icon(app_ui_watch_icon_t icon);

#ifdef __cplusplus
}
#endif

#endif
