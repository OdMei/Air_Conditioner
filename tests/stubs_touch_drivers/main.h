#ifndef TOUCH_DRIVERS_TEST_MAIN_H
#define TOUCH_DRIVERS_TEST_MAIN_H

#include <stdint.h>

typedef struct { uint8_t unused; } GPIO_TypeDef;
typedef struct {
    uint32_t Pin;
    uint32_t Mode;
    uint32_t Pull;
    uint32_t Speed;
} GPIO_InitTypeDef;
typedef enum { GPIO_PIN_RESET = 0, GPIO_PIN_SET = 1 } GPIO_PinState;
extern GPIO_TypeDef touch_drivers_gpiob;
extern GPIO_TypeDef touch_drivers_gpiod;
#define GPIOB (&touch_drivers_gpiob)
#define GPIOD (&touch_drivers_gpiod)
#define GPIO_PIN_3  (1U << 3)
#define GPIO_PIN_4  (1U << 4)
#define GPIO_PIN_8  (1U << 8)
#define GPIO_PIN_10 (1U << 10)
#define GPIO_PIN_14 (1U << 14)
#define GPIO_MODE_INPUT 0U
#define GPIO_MODE_OUTPUT_PP 1U
#define GPIO_PULLUP 1U
#define GPIO_NOPULL 0U
#define GPIO_SPEED_FREQ_VERY_HIGH 3U
#define __HAL_RCC_GPIOD_CLK_ENABLE() ((void)0)
#define __HAL_RCC_GPIOB_CLK_ENABLE() ((void)0)

uint32_t HAL_GetTick(void);
void HAL_Delay(uint32_t milliseconds);
void HAL_GPIO_Init(GPIO_TypeDef *port, GPIO_InitTypeDef *config);
void HAL_GPIO_WritePin(GPIO_TypeDef *port, uint16_t pin, GPIO_PinState state);
GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef *port, uint16_t pin);

#endif
