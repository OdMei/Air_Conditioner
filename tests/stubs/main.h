#ifndef AP3216C_HOST_MAIN_H
#define AP3216C_HOST_MAIN_H

/* Minimal HAL surface used only when the AP3216C host test selects this
   directory first. Firmware builds continue using their real main.h. */
#include <stddef.h>
#include <stdint.h>

typedef struct { uint32_t unused; } I2C_HandleTypeDef;
typedef enum { HAL_OK = 0, HAL_ERROR, HAL_BUSY, HAL_TIMEOUT } HAL_StatusTypeDef;
#define I2C_MEMADD_SIZE_8BIT (1U)

uint32_t HAL_GetTick(void);
void HAL_Delay(uint32_t milliseconds);
HAL_StatusTypeDef HAL_I2C_Mem_Read(I2C_HandleTypeDef *handle, uint16_t address,
                                  uint16_t reg, uint16_t reg_size,
                                  uint8_t *data, uint16_t size, uint32_t timeout);
HAL_StatusTypeDef HAL_I2C_Mem_Write(I2C_HandleTypeDef *handle, uint16_t address,
                                   uint16_t reg, uint16_t reg_size,
                                   uint8_t *data, uint16_t size, uint32_t timeout);

#endif
