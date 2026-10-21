#ifndef TOUCH_TEST_MAIN_H
#define TOUCH_TEST_MAIN_H

#include <stdint.h>

typedef struct { uint8_t unused; } GPIO_TypeDef;
typedef enum { GPIO_PIN_RESET = 0, GPIO_PIN_SET = 1 } GPIO_PinState;
extern GPIO_TypeDef touch_test_gpiob;
extern GPIO_TypeDef touch_test_gpiod;
#define GPIOB (&touch_test_gpiob)
#define GPIOD (&touch_test_gpiod)
#define GPIO_PIN_3  (1U << 3)
#define GPIO_PIN_4  (1U << 4)
#define GPIO_PIN_10 (1U << 10)
#define GPIO_PIN_14 (1U << 14)

uint32_t HAL_GetTick(void);
GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef *port, uint16_t pin);

#endif
