#ifndef BSP_RUNTIME_HOST_MAIN_H
#define BSP_RUNTIME_HOST_MAIN_H

#include <stddef.h>
#include <stdint.h>

typedef struct { uint32_t unused; } GPIO_TypeDef;
typedef struct { void *Instance; } UART_HandleTypeDef;
typedef struct { void *Instance; } TIM_HandleTypeDef;
typedef enum { GPIO_PIN_RESET = 0, GPIO_PIN_SET = 1 } GPIO_PinState;
typedef enum { HAL_OK = 0, HAL_ERROR, HAL_BUSY, HAL_TIMEOUT } HAL_StatusTypeDef;
extern GPIO_TypeDef mock_gpioe, mock_gpiog;
#define GPIOE (&mock_gpioe)
#define GPIOG (&mock_gpiog)
#define GPIO_PIN_3 (1U << 3U)
#define GPIO_PIN_9 (1U << 9U)
#define USART1 ((void *)(uintptr_t)0x1000U)
#define TIM1 ((void *)(uintptr_t)0x2000U)
#define TIM_CHANNEL_1 (0U)
#define TIM_IT_UPDATE (1U)
#define TIM_ICPOLARITY_FALLING (0U)
#define TIM_ICPOLARITY_RISING (1U)
#define UNUSED(value) ((void)(value))
#define TIM_RESET_CAPTUREPOLARITY(handle, channel) ((void)(handle), (void)(channel))
#define TIM_SET_CAPTUREPOLARITY(handle, channel, polarity) \
    ((void)(handle), (void)(channel), (void)(polarity))
#define __HAL_TIM_SET_COUNTER(handle, value) ((void)(handle), (void)(value))
#define __HAL_TIM_ENABLE_IT(handle, value) ((void)(handle), (void)(value))

uint32_t __get_PRIMASK(void);
void __disable_irq(void);
void __set_PRIMASK(uint32_t value);
uint32_t __get_IPSR(void);
uint32_t __get_BASEPRI(void);
void __set_BASEPRI(uint32_t value);
void __DSB(void);
void __ISB(void);
#define __NVIC_PRIO_BITS (4U)
uint32_t HAL_GetTick(void);
void HAL_Delay(uint32_t milliseconds);
GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef *port, uint16_t pin);
void HAL_GPIO_WritePin(GPIO_TypeDef *port, uint16_t pin, GPIO_PinState state);
HAL_StatusTypeDef HAL_UART_Transmit(UART_HandleTypeDef *handle,
                                  const uint8_t *data, uint16_t size,
                                  uint32_t timeout);
HAL_StatusTypeDef HAL_TIM_IC_Start_IT(TIM_HandleTypeDef *handle, uint32_t channel);
uint32_t HAL_TIM_ReadCapturedValue(TIM_HandleTypeDef *handle, uint32_t channel);
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *handle);
void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *handle);

#endif
