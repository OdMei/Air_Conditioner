#include "ctiic.h"
#include "bsp_bus.h"

/* Camera uses bsp_bus directly and cannot overwrite this touch-local status. */
static uint8_t s_ct_iic_error;
static uint32_t s_ct_iic_transaction_sequence;
static uint16_t s_ct_iic_last_address = 0xFFFFU;

static uint8_t ct_iic_result(int32_t status)
{
    s_ct_iic_error = (status == 0) ? 0U : 1U;
    return s_ct_iic_error;
}

uint8_t ct_iic_init(void)
{
    s_ct_iic_last_address = 0xFFFFU;
    return ct_iic_result(bsp_i2c2_init());
}

uint8_t ct_iic_probe(uint8_t address)
{
    s_ct_iic_last_address = (uint16_t)(address & 0xFEU);
    s_ct_iic_transaction_sequence++;
    return ct_iic_result(bsp_i2c2_probe((uint16_t)(address & 0xFEU)));
}

uint8_t ct_iic_read_reg(uint8_t address, uint16_t reg, uint8_t reg_bytes,
                        uint8_t *data, uint16_t length)
{
    /* FT/GT constants already contain the shifted address. HAL supplies the
     * R/W bit and the repeated START; never shift these addresses again. */
    address &= 0xFEU;
    s_ct_iic_last_address = address;
    if ((reg_bytes == CT_IIC_REG8) && (reg <= 0xFFU))
    {
        s_ct_iic_transaction_sequence++;
        return ct_iic_result(bsp_i2c2_read_reg8(address, (uint8_t)reg, data, length));
    }
    if (reg_bytes == CT_IIC_REG16)
    {
        s_ct_iic_transaction_sequence++;
        return ct_iic_result(bsp_i2c2_read_reg16(address, reg, data, length));
    }
    return ct_iic_result(-1);
}

uint8_t ct_iic_write_reg(uint8_t address, uint16_t reg, uint8_t reg_bytes,
                         uint8_t *data, uint16_t length)
{
    address &= 0xFEU;
    s_ct_iic_last_address = address;
    if ((reg_bytes == CT_IIC_REG8) && (reg <= 0xFFU))
    {
        s_ct_iic_transaction_sequence++;
        return ct_iic_result(bsp_i2c2_write_reg8(address, (uint8_t)reg, data, length));
    }
    if (reg_bytes == CT_IIC_REG16)
    {
        s_ct_iic_transaction_sequence++;
        return ct_iic_result(bsp_i2c2_write_reg16(address, reg, data, length));
    }
    return ct_iic_result(-1);
}

uint8_t ct_iic_last_error(void)
{
    return s_ct_iic_error;
}

uint32_t ct_iic_transaction_sequence(void)
{
    return s_ct_iic_transaction_sequence;
}

uint8_t ct_iic_get_diag(bsp_i2c2_diag_t *diag)
{
    return (bsp_i2c2_get_diag(s_ct_iic_last_address, diag) == 0) ? 0U : 1U;
}

