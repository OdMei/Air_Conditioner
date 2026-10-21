#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "ap3216c.h"

I2C_HandleTypeDef hi2c4;
static uint32_t s_tick;
static uint32_t s_read_cost;
static unsigned int s_reads;
static unsigned int s_writes;
static unsigned int s_delays;
static int s_fail_read;
static int s_fail_write;
static uint8_t s_mode_readback;
static uint8_t s_registers[6];
static uint32_t s_timeouts[6];

static void reset_hal(void)
{
    const uint8_t data[6] = {0x03U, 0x12U, 0x34U, 0x12U, 0x0FU, 0x05U};
    s_tick = 0U;
    s_read_cost = 0U;
    s_reads = s_writes = s_delays = 0U;
    s_fail_read = s_fail_write = -1;
    s_mode_readback = 3U;
    memcpy(s_registers, data, sizeof(data));
    memset(s_timeouts, 0, sizeof(s_timeouts));
}

uint32_t HAL_GetTick(void) { return s_tick; }
void HAL_Delay(uint32_t milliseconds)
{
    assert(milliseconds == 10U);
    s_tick += milliseconds;
    s_delays++;
}

HAL_StatusTypeDef HAL_I2C_Mem_Read(I2C_HandleTypeDef *handle, uint16_t address,
                                  uint16_t reg, uint16_t reg_size,
                                  uint8_t *data, uint16_t size, uint32_t timeout)
{
    unsigned int index = s_reads++;
    assert(handle == &hi2c4 && address == AP3216C_ADDR);
    assert(reg_size == I2C_MEMADD_SIZE_8BIT && size == 1U);
    assert(timeout > 0U && timeout <= 20U);
    if (index < 6U) { s_timeouts[index] = timeout; }
    /* Only returned status, elapsed ticks and bytes are simulated. This is
       deliberately not a simulation of I2C clocks or interrupt timing. */
    s_tick += s_read_cost;
    if ((int)index == s_fail_read) {
        *data = 0xEEU; /* A failed transfer may still alter its local buffer. */
        return HAL_ERROR;
    }
    if (reg == 0U) { *data = s_mode_readback; }
    else {
        assert(reg >= 0x0AU && reg <= 0x0FU);
        *data = s_registers[reg - 0x0AU];
    }
    return HAL_OK;
}

HAL_StatusTypeDef HAL_I2C_Mem_Write(I2C_HandleTypeDef *handle, uint16_t address,
                                   uint16_t reg, uint16_t reg_size,
                                   uint8_t *data, uint16_t size, uint32_t timeout)
{
    unsigned int index = s_writes++;
    assert(handle == &hi2c4 && address == AP3216C_ADDR && reg == 0U);
    assert(reg_size == I2C_MEMADD_SIZE_8BIT && size == 1U && timeout == 20U);
    assert(index < 2U && *data == (index == 0U ? 4U : 3U));
    return (int)index == s_fail_write ? HAL_ERROR : HAL_OK;
}

static void test_success_and_invalid_measurement_bits(void)
{
    uint16_t ir, ps, als;
    reset_hal();
    s_read_cost = 2U;
    assert(ap3216c_read_checked(&ir, &ps, &als) == 0U);
    assert(ir == 0x4BU && ps == 0x5FU && als == 0x1234U);
    assert(s_reads == 6U && s_tick == 12U);
    for (unsigned int i = 0U; i < 6U; i++) {
        assert(s_timeouts[i] == 20U - 2U * i);
    }
    reset_hal();
    s_registers[0] |= 0x80U;
    s_registers[4] |= 0x40U;
    assert(ap3216c_read_checked(&ir, &ps, &als) == 0U);
    assert(ir == 0U && ps == 0U && als == 0x1234U);
}

static void test_each_transfer_failure_keeps_outputs(void)
{
    for (int failure = 0; failure < 6; failure++) {
        uint16_t ir = 111U, ps = 222U, als = 333U;
        reset_hal();
        s_fail_read = failure;
        assert(ap3216c_read_checked(&ir, &ps, &als) == 1U);
        assert(ir == 111U && ps == 222U && als == 333U);
        assert(s_reads == (unsigned int)failure + 1U);
    }
    reset_hal();
    uint16_t value = 111U;
    assert(ap3216c_read_checked(NULL, &value, &value) == 1U);
    assert(s_reads == 0U && value == 111U);
}

static void test_shared_deadline_and_wrap(void)
{
    uint16_t ir = 111U, ps = 222U, als = 333U;
    reset_hal();
    s_read_cost = 5U;
    assert(ap3216c_read_checked(&ir, &ps, &als) == 1U);
    assert(s_reads == 4U && s_tick == 20U);
    assert(ir == 111U && ps == 222U && als == 333U);
    reset_hal();
    s_tick = UINT32_MAX - 4U;
    s_read_cost = 1U;
    assert(ap3216c_read_checked(&ir, &ps, &als) == 0U);
    assert(s_reads == 6U && s_tick == 1U && als == 0x1234U);
    /* HAL's fixed BUSY wait can exceed the supplied timeout. The driver must
       reject that result rather than publish a late partial sample. */
    reset_hal();
    ir = 111U; ps = 222U; als = 333U;
    s_read_cost = 21U;
    assert(ap3216c_read_checked(&ir, &ps, &als) == 1U);
    assert(s_reads == 1U && ir == 111U && ps == 222U && als == 333U);
}

static void test_initialization_error_propagation(void)
{
    reset_hal();
    assert(ap3216c_init() == 0U);
    assert(s_writes == 2U && s_reads == 1U && s_delays == 1U);
    for (int failure = 0; failure < 2; failure++) {
        reset_hal();
        s_fail_write = failure;
        assert(ap3216c_init() == 1U);
        assert(s_writes == (unsigned int)failure + 1U && s_reads == 0U);
    }
    reset_hal();
    s_fail_read = 0;
    assert(ap3216c_init() == 1U);
    reset_hal();
    s_mode_readback = 0U;
    assert(ap3216c_init() == 1U);
}

int main(void)
{
    test_success_and_invalid_measurement_bits();
    test_each_transfer_failure_keeps_outputs();
    test_shared_deadline_and_wrap();
    test_initialization_error_propagation();
    puts("AP3216C driver fault injection tests passed");
    return 0;
}
