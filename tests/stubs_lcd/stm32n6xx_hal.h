#ifndef TEST_LCD_HAL_H
#define TEST_LCD_HAL_H
#include <stddef.h>
#include <stdint.h>
typedef enum { HAL_OK = 0, HAL_ERROR = 1 } HAL_StatusTypeDef;
typedef struct { volatile uint32_t RCR, CFBLR, CFBAR, CR; } LTDC_Layer_TypeDef;
typedef struct { uint32_t unused; } LTDC_HandleTypeDef;
extern LTDC_Layer_TypeDef test_lcd_layers[2];
#define LTDC_Layer1 (&test_lcd_layers[0])
#define LTDC_Layer2 (&test_lcd_layers[1])
#define LTDC_LxRCR_VBR 2U
#define LTDC_RELOAD_VERTICAL_BLANKING LTDC_LxRCR_VBR
#define LTDC_LxCFBLR_CFBP_Msk 0xFFFFU
#define LTDC_LxCFBLR_CFBP_Pos 0U
uint32_t HAL_GetTick(void);
uint32_t __get_PRIMASK(void);
void __disable_irq(void);
void __set_PRIMASK(uint32_t state);
void SCB_CleanInvalidateDCache_by_Addr(void *address, int32_t bytes);
void SCB_CleanDCache_by_Addr(void *address, int32_t bytes);
void SCB_InvalidateDCache_by_Addr(volatile void *address, int32_t bytes);
HAL_StatusTypeDef HAL_LTDC_SetAddress_NoReload(LTDC_HandleTypeDef *handle,
                                               uint32_t address, uint32_t layer);
HAL_StatusTypeDef HAL_LTDC_ReloadLayer(LTDC_HandleTypeDef *handle,
                                      uint32_t type, uint32_t layer);
#endif
