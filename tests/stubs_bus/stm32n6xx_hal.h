#ifndef BUS_TEST_HAL_H
#define BUS_TEST_HAL_H
#include <stdint.h>
#include <stddef.h>
typedef enum { HAL_OK, HAL_ERROR, HAL_BUSY, HAL_TIMEOUT } HAL_StatusTypeDef;
typedef enum { RESET, SET } FlagStatus;
typedef enum { GPIO_PIN_RESET, GPIO_PIN_SET } GPIO_PinState;
typedef struct { uint32_t CR1, CR2, ISR, ICR, TIMINGR; } I2C_TypeDef;
typedef struct { uint32_t Timing; } I2C_InitTypeDef;
typedef struct { I2C_TypeDef *Instance; I2C_InitTypeDef Init; uint32_t State,Mode,Lock,ErrorCode; } I2C_HandleTypeDef;
extern I2C_TypeDef mock_i2c2;
extern I2C_HandleTypeDef hi2c2;
#define I2C2 (&mock_i2c2)
#define GPIOD ((void *)1)
#define GPIO_PIN_4 (1U << 4)
#define GPIO_PIN_14 (1U << 14)
#define HAL_I2C_STATE_RESET 0U
#define HAL_I2C_STATE_READY 0x20U
#define HAL_I2C_STATE_BUSY_RX 0x22U
#define HAL_I2C_MODE_NONE 0U
#define HAL_UNLOCKED 0U
#define HAL_LOCKED 1U
#define HAL_I2C_ERROR_NONE 0U
#define HAL_I2C_ERROR_AF 4U
#define HAL_I2C_ERROR_TIMEOUT 0x20U
#define I2C_MEMADD_SIZE_8BIT 1U
#define I2C_MEMADD_SIZE_16BIT 2U
#define I2C_CR1_PE 1U
#define I2C_CR1_DNF 0xF00U
#define I2C_CR1_ANFOFF 0x1000U
#define I2C_CR2_STOP 0x4000U
#define I2C_FLAG_BUSY 0x8000U
#define I2C_FLAG_STOPF 0x20U
#define I2C_FLAG_AF 0x10U
#define I2C_FLAG_BERR 0x100U
#define I2C_FLAG_ARLO 0x200U
#define I2C_FLAG_OVR 0x400U
void mock_set_bit(uint32_t *reg, uint32_t bits);
void mock_i2c_disable(I2C_HandleTypeDef *handle);
void mock_i2c_clear(I2C_HandleTypeDef *handle, uint32_t bits);
void mock_rcc_reset(void);
#define SET_BIT(reg,bits) mock_set_bit(&(reg),(bits))
#define __HAL_I2C_GET_FLAG(h,f) (((h)->Instance->ISR & (f)) ? SET : RESET)
#define __HAL_I2C_DISABLE(h) mock_i2c_disable(h)
#define __HAL_I2C_CLEAR_FLAG(h,f) mock_i2c_clear((h),(f))
#define I2C_RESET_CR2(h) ((h)->Instance->CR2 = 0U)
#define __HAL_RCC_I2C2_FORCE_RESET() mock_rcc_reset()
#define __HAL_RCC_I2C2_RELEASE_RESET() ((void)0)
#define __NOP() ((void)0)
uint32_t HAL_GetTick(void);
uint32_t HAL_I2C_GetState(I2C_HandleTypeDef *handle);
GPIO_PinState HAL_GPIO_ReadPin(void *port, uint16_t pin);
HAL_StatusTypeDef HAL_I2C_Init(I2C_HandleTypeDef *handle);
HAL_StatusTypeDef HAL_I2CEx_ConfigAnalogFilter(I2C_HandleTypeDef *handle,uint32_t filter);
HAL_StatusTypeDef HAL_I2CEx_ConfigDigitalFilter(I2C_HandleTypeDef *handle,uint32_t filter);
HAL_StatusTypeDef HAL_I2C_Mem_Read(I2C_HandleTypeDef *,uint16_t,uint16_t,uint16_t,uint8_t *,uint16_t,uint32_t);
HAL_StatusTypeDef HAL_I2C_Mem_Write(I2C_HandleTypeDef *,uint16_t,uint16_t,uint16_t,uint8_t *,uint16_t,uint32_t);
HAL_StatusTypeDef HAL_I2C_IsDeviceReady(I2C_HandleTypeDef *,uint16_t,uint32_t,uint32_t);
#endif
