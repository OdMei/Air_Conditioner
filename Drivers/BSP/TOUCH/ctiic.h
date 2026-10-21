#ifndef __CT_IIC_H
#define __CT_IIC_H

#include "main.h"
#include "bsp_bus.h"

/* Read-only board diagnostics. PD4/PD14 stay in the camera's I2C2 AF4 mode. */
#define CT_IIC_SCL_GPIO_PORT GPIOD
#define CT_IIC_SCL_GPIO_PIN  GPIO_PIN_14
#define CT_IIC_SDA_GPIO_PORT GPIOD
#define CT_IIC_SDA_GPIO_PIN  GPIO_PIN_4
#define CT_READ_SCL HAL_GPIO_ReadPin(CT_IIC_SCL_GPIO_PORT, CT_IIC_SCL_GPIO_PIN)
#define CT_READ_SDA HAL_GPIO_ReadPin(CT_IIC_SDA_GPIO_PORT, CT_IIC_SDA_GPIO_PIN)

#define CT_IIC_REG8  (1U)
#define CT_IIC_REG16 (2U)

/* Address = (7-bit << 1). Return 0 on success, 1 on failure. Thread-only. */
uint8_t ct_iic_init(void);
uint8_t ct_iic_probe(uint8_t address);
uint8_t ct_iic_read_reg(uint8_t address, uint16_t reg, uint8_t reg_bytes,
                        uint8_t *data, uint16_t length);
uint8_t ct_iic_write_reg(uint8_t address, uint16_t reg, uint8_t reg_bytes,
                         uint8_t *data, uint16_t length);
uint8_t ct_iic_last_error(void);
/* Advances for each real probe/register transaction attempt, not init/scan skips. */
uint32_t ct_iic_transaction_sequence(void);
/* Snapshot for the last touch address attempted; no I2C transfer. */
uint8_t ct_iic_get_diag(bsp_i2c2_diag_t *diag);

#endif
