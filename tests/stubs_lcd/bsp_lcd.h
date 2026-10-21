#ifndef TEST_LCD_BSP_H
#define TEST_LCD_BSP_H
#include "stm32n6xx_hal.h"
#define BSP_LCD_WIDTH 800U
#define BSP_LCD_HEIGHT 480U
#define LCD_PIXEL_FORMAT_RGB565 1U
#define LCD_PIXEL_FORMAT_ARGB4444 2U
typedef struct {
    uint32_t x0, y0, x1, y1, pixel_format, address;
} bsp_lcd_layer_config_t;
extern const unsigned bsp_lcd_driver;
void bsp_lcd_init(void);
void bsp_lcd_set_brightness(uint8_t brightness);
void bsp_lcd_config_layer(unsigned layer, bsp_lcd_layer_config_t *config);
LTDC_HandleTypeDef *bsp_lcd_get_ltdc_handle(void);
#endif
