#ifndef APP_SIMULATOR_LV_CONF_H
#define APP_SIMULATOR_LV_CONF_H

/* Share fonts, the 160 KiB allocator, color depth and rendering options with
 * the board. Only this desktop target includes this wrapper. */
#include "../Project/Project/Air_Conditioner/Appli/Core/Inc/lv_conf.h"

/* app_ui creates the sole LVGL display. pc_port presents its converted
 * ARGB4444 overlay using SDL, without an additional LVGL display driver. */
#undef LV_USE_OS
#define LV_USE_OS LV_OS_NONE
#undef LV_USE_SDL
#define LV_USE_SDL 0
#undef LV_USE_DRAW_SDL
#define LV_USE_DRAW_SDL 0
#undef LV_USE_WINDOWS
#define LV_USE_WINDOWS 0

#endif /* APP_SIMULATOR_LV_CONF_H */
