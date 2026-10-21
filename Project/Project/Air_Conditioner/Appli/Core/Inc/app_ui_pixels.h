#ifndef APP_UI_PIXELS_H
#define APP_UI_PIXELS_H

#include <stdint.h>

/* Convert tightly packed LVGL BGRA bytes to the LCD's ARGB4444 pixels.
 * Each channel keeps its high four bits, including alpha. Destination rows
 * have dst_stride_pixels pixels; padding is preserved. Buffers must not
 * overlap and destination must be uint16_t aligned. A null pointer, empty
 * image, short destination stride or overflowing span leaves buffers intact.
 * APP_UI_PIXELS_DISABLE_HELIUM selects the portable implementation. */
void app_ui_pixels_convert(uint16_t *dst, uint32_t dst_stride_pixels,
                           const uint8_t *src_bgra,
                           uint32_t width, uint32_t height);

/* Compile-time backend selection; this does not claim measured speedup. */
uint8_t app_ui_pixels_uses_helium(void);

#endif /* APP_UI_PIXELS_H */
