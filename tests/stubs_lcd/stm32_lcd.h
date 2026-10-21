#ifndef TEST_LCD_UTILS_H
#define TEST_LCD_UTILS_H
#define UTIL_LCD_COLOR_WHITE 0xFFFFFFFFU
typedef enum { LEFT_MODE = 0 } Text_AlignModeTypdef;
extern const unsigned Font20;
void UTIL_LCD_SetFuncDriver(const void *driver);
void UTIL_LCD_SetLayer(unsigned layer);
void UTIL_LCD_Clear(unsigned color);
void UTIL_LCD_SetFont(const void *font);
void UTIL_LCD_SetTextColor(unsigned color);
#endif
