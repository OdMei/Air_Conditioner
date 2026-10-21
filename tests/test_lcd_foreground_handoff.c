#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "app_lcd.h"
#include "bsp_lcd.h"

LTDC_Layer_TypeDef test_lcd_layers[2];
const unsigned Font20 = 0U;
const unsigned bsp_lcd_driver = 0U;
static LTDC_HandleTypeDef handle;
static uint32_t test_tick;
static uint32_t irq_mask;
static uint32_t clean_calls;
static uint32_t clean_bytes;
static uint32_t sleeps;
static uint32_t complete_after;
static uint8_t address_fails;
static uint8_t dma_visible[2][LCD_FG_WIDTH * LCD_FG_HEIGHT * 2U];

uint32_t HAL_GetTick(void) { return test_tick; }
uint32_t __get_PRIMASK(void) { return irq_mask; }
void __disable_irq(void) { irq_mask = 1U; }
void __set_PRIMASK(uint32_t state) { irq_mask = state; }
void SCB_CleanInvalidateDCache_by_Addr(void *a, int32_t n)
{ (void)a; (void)n; assert(irq_mask == 0U); }
void SCB_CleanDCache_by_Addr(void *a, int32_t n)
{
    uintptr_t address = (uintptr_t)a;
    assert(irq_mask == 0U);
    clean_calls++;
    clean_bytes += n;
    for (uint8_t i = 0U; i < 2U; i++)
    {
        uintptr_t base = (uintptr_t)app_lcd_get_fg_buffer(i);
        if (address >= base && address + (uint32_t)n <= base + sizeof(dma_visible[i]))
            memcpy(dma_visible[i] + (address - base), a, (size_t)n);
    }
}
void SCB_InvalidateDCache_by_Addr(volatile void *a, int32_t n)
{ (void)a; (void)n; assert(irq_mask == 0U); }
HAL_StatusTypeDef HAL_LTDC_SetAddress_NoReload(LTDC_HandleTypeDef *h,
                                               uint32_t a, uint32_t layer)
{
    (void)h;
    if (address_fails) return HAL_ERROR;
    test_lcd_layers[layer].CFBAR = a;
    return HAL_OK;
}
HAL_StatusTypeDef HAL_LTDC_ReloadLayer(LTDC_HandleTypeDef *h,
                                      uint32_t type, uint32_t layer)
{ (void)h; test_lcd_layers[layer].RCR = type; return HAL_OK; }
void bsp_lcd_init(void) {}
void bsp_lcd_set_brightness(uint8_t b) { (void)b; }
void bsp_lcd_config_layer(unsigned layer, bsp_lcd_layer_config_t *c)
{ test_lcd_layers[layer].CFBAR = c->address; }
LTDC_HandleTypeDef *bsp_lcd_get_ltdc_handle(void) { return &handle; }
void UTIL_LCD_SetFuncDriver(const void *p) { (void)p; }
void UTIL_LCD_SetLayer(unsigned n) { (void)n; }
void UTIL_LCD_Clear(unsigned n) { (void)n; }
void UTIL_LCD_SetFont(const void *p) { (void)p; }
void UTIL_LCD_SetTextColor(unsigned n) { (void)n; }
void tx_thread_sleep(unsigned long ticks)
{
    assert(irq_mask == 0U);
    test_tick += (uint32_t)ticks;
    sleeps++;
    if (complete_after != 0U && sleeps >= complete_after)
        test_lcd_layers[1].RCR = 0U;
}

int main(void)
{
    uint16_t *draw;
    uint16_t *old_front;
    const uint32_t dirty = 7U * LCD_FG_WIDTH + 5U;

    app_lcd_init();
    draw = (uint16_t *)app_lcd_get_fg_draw_buffer();
    old_front = (uint16_t *)app_lcd_get_fg_buffer(1U);
    assert(draw != old_front);
    draw[dirty] = 0xF123U;
    draw[dirty + 1U] = 0x4567U;
    draw[dirty + LCD_FG_WIDTH] = 0x8123U;
    app_lcd_mark_fg_dirty(5, 7, 2U, 2U);
    app_lcd_present_fg_buffer((uint8_t *)draw);
    assert(clean_calls == 2U && clean_bytes == 8U);
    assert(old_front[dirty] == 0U); /* Still scanning: not copied yet. */

    complete_after = 0U;
    assert(app_lcd_get_fg_draw_buffer() == NULL);
    assert(sleeps == 50U && old_front[dirty] == 0U);
    /* A background-layer reload is not foreground completion. */
    test_lcd_layers[0].RCR = 0U;
    assert(app_lcd_get_fg_draw_buffer() == NULL);
    assert(old_front[dirty] == 0U);

    test_lcd_layers[1].RCR = 0U;
    assert(app_lcd_get_fg_draw_buffer() == (uint8_t *)old_front);
    assert(old_front[dirty] == 0xF123U);
    assert(old_front[dirty + 1U] == 0x4567U);
    assert(old_front[dirty + LCD_FG_WIDTH] == 0x8123U);
    assert(old_front[dirty - 1U] == 0U);
    assert(old_front[dirty + 2U] == 0U);
    assert(clean_calls == 4U && clean_bytes == 16U);
    assert(((uint16_t *)dma_visible[1])[dirty] == 0xF123U);

    old_front[dirty + 3U] = 0xABCDU;
    app_lcd_mark_fg_dirty(8, 7, 1U, 1U);
    app_lcd_present_fg_buffer((uint8_t *)old_front);
    assert(((uint16_t *)dma_visible[1])[dirty] == 0xF123U);
    assert(((uint16_t *)dma_visible[1])[dirty + 3U] == 0xABCDU);
    sleeps = 0U;
    complete_after = 3U;
    assert(app_lcd_get_fg_draw_buffer() == (uint8_t *)draw);
    assert(sleeps == 3U);
    assert(draw[dirty + 3U] == 0xABCDU);
    assert(draw[dirty] == 0xF123U); /* Previous partial area preserved. */

    /* Invalid pointers cannot change ownership or request another reload. */
    app_lcd_present_fg_buffer((uint8_t *)old_front);
    assert(test_lcd_layers[1].RCR == 0U);
    assert(app_lcd_get_fg_draw_buffer() == (uint8_t *)draw);

    /* A HAL submission failure keeps the editable back buffer and dirty area. */
    address_fails = 1U;
    draw[dirty] = 0xFFFFU;
    app_lcd_mark_fg_dirty(5, 7, 1U, 1U);
    app_lcd_present_fg_buffer((uint8_t *)draw);
    assert(test_lcd_layers[1].RCR == 0U);
    assert(app_lcd_get_fg_draw_buffer() == (uint8_t *)draw);
    address_fails = 0U;
    app_lcd_present_fg_buffer((uint8_t *)draw);
    test_lcd_layers[1].RCR = 0U;
    assert(app_lcd_get_fg_draw_buffer() == (uint8_t *)old_front);
    assert(old_front[dirty] == 0xFFFFU);
    puts("PASS: foreground VBlank ownership, bounded waits and partial baseline.");
    return 0;
}
