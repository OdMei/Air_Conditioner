#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "app_touch.h"
#include "ctiic.h"
#include "ft5206.h"
#include "gt9xxx.h"
#include "touch.h"
#include "bsp_bus.h"

GPIO_TypeDef touch_test_gpiob;
GPIO_TypeDef touch_test_gpiod;

enum { DEVICE_FT = 1, DEVICE_GT = 2 };
enum { SCAN_SKIP, SCAN_DOWN, SCAN_UP, SCAN_FAIL };
static uint32_t mock_tick;
static uint8_t mock_devices;
static uint8_t mock_scan_mode;
static uint8_t mock_all_transfers_fail;
static uint8_t mock_probe_fails;
static uint32_t mock_ft_init_calls;
static uint32_t mock_gt_init_calls;
static uint32_t mock_bus_init_calls;
static uint32_t mock_bus_deinit_calls;
static uint32_t mock_gpio_read_calls;
static GPIO_PinState mock_gpio_level;

uint32_t HAL_GetTick(void) { return mock_tick; }

GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef *port, uint16_t pin)
{
    (void)port;
    (void)pin;
    mock_gpio_read_calls++;
    return mock_gpio_level;
}

static uint8_t device_for(uint16_t address)
{
    if (address == FT5206_CMD_WR) { return DEVICE_FT; }
    if (address == GT9XXX_CMD_WR) { return DEVICE_GT; }
    return 0U;
}

int32_t bsp_i2c2_init(void) { mock_bus_init_calls++; return 0; }
int32_t bsp_i2c2_deinit(void) { mock_bus_deinit_calls++; return 0; }
int32_t bsp_i2c2_get_diag(uint16_t address, bsp_i2c2_diag_t *diag)
{
    (void)address;
    memset(diag, 0, sizeof(*diag));
    return 0;
}

int32_t bsp_i2c2_probe(uint16_t address)
{
    return (mock_all_transfers_fail || mock_probe_fails ||
            !(mock_devices & device_for(address))) ? -1 : 0;
}

static int32_t mock_read(uint16_t address, uint16_t reg, uint8_t *data, uint16_t length)
{
    if (mock_all_transfers_fail || !(mock_devices & device_for(address)) ||
        ((mock_scan_mode == SCAN_FAIL) &&
         ((reg == FT5206_REG_NUM_FINGER) || (reg == GT9XXX_GSTID_REG))))
    {
        return -1;
    }
    memset(data, 0, length);
    return 0;
}

int32_t bsp_i2c2_read_reg8(uint16_t address, uint8_t reg, uint8_t *data, uint16_t length)
{ return mock_read(address, reg, data, length); }
int32_t bsp_i2c2_read_reg16(uint16_t address, uint16_t reg, uint8_t *data, uint16_t length)
{ return mock_read(address, reg, data, length); }
int32_t bsp_i2c2_write_reg8(uint16_t address, uint8_t reg, uint8_t *data, uint16_t length)
{ return mock_read(address, reg, data, length); }
int32_t bsp_i2c2_write_reg16(uint16_t address, uint16_t reg, uint8_t *data, uint16_t length)
{ return mock_read(address, reg, data, length); }

uint8_t ft5206_init(void)
{
    uint8_t data;
    mock_ft_init_calls++;
    if (ct_iic_init() != 0U) { return 1U; }
    return ct_iic_read_reg(FT5206_CMD_WR, FT5206_ID_G_LIB_VERSION, CT_IIC_REG8, &data, 1U);
}

uint8_t gt9xxx_init(void)
{
    uint8_t data;
    mock_gt_init_calls++;
    if (ct_iic_init() != 0U) { return 1U; }
    return ct_iic_read_reg(GT9XXX_CMD_WR, GT9XXX_PID_REG, CT_IIC_REG16, &data, 1U);
}

static uint8_t mock_scan(uint8_t address, uint16_t reg, uint8_t reg_bytes)
{
    uint8_t data;
    if (mock_scan_mode == SCAN_SKIP) { return 0U; }
    if (ct_iic_read_reg(address, reg, reg_bytes, &data, 1U) != 0U)
    {
        tp_dev.sta = 0U;
        return 0U;
    }
    if (mock_scan_mode == SCAN_DOWN)
    {
        tp_dev.sta = TP_PRES_DOWN | TP_CATH_PRES | 3U;
        tp_dev.x[0] = 320U;
        tp_dev.y[0] = 220U;
        return 1U;
    }
    tp_dev.sta = 0U;
    return 0U;
}

uint8_t ft5206_scan(uint8_t mode)
{ (void)mode; return mock_scan(FT5206_CMD_WR, FT5206_REG_NUM_FINGER, CT_IIC_REG8); }
uint8_t gt9xxx_scan(uint8_t mode)
{ (void)mode; return mock_scan(GT9XXX_CMD_WR, GT9XXX_GSTID_REG, CT_IIC_REG16); }

static void reset_mock(uint32_t tick, uint8_t devices)
{
    mock_tick = tick;
    mock_devices = devices;
    mock_scan_mode = SCAN_SKIP;
    mock_all_transfers_fail = 0U;
    mock_probe_fails = 0U;
    mock_ft_init_calls = mock_gt_init_calls = 0U;
    mock_bus_init_calls = mock_bus_deinit_calls = 0U;
    mock_gpio_read_calls = 0U;
    mock_gpio_level = GPIO_PIN_SET;
}

static app_touch_state_t state(void)
{
    app_touch_state_t result;
    app_touch_get_state(&result);
    return result;
}

static void poll(uint32_t tick, uint8_t mode)
{
    mock_tick = tick;
    mock_scan_mode = mode;
    app_touch_poll();
    assert(mock_bus_deinit_calls == 0U);
}

static void test_initial_failure_retry(void)
{
    reset_mock(1000U, 0U);
    assert(app_touch_init() == 1U);
    assert(state().ready == 0U && state().controller == 0U);
    assert(state().error_count == 1U && state().recovery_count == 0U);
    poll(5999U, SCAN_SKIP);
    assert(mock_ft_init_calls == 1U && mock_gt_init_calls == 1U);
    mock_devices = DEVICE_FT;
    poll(6000U, SCAN_SKIP);
    assert(state().ready == 1U && state().controller == DEVICE_FT);
    assert(state().last_success_ms == 6000U && state().recovery_count == 1U);
    assert(mock_ft_init_calls == 2U && mock_gt_init_calls == 1U);
}

static void test_persistent_failure_and_known_controller(void)
{
    reset_mock(1000U, DEVICE_GT);
    assert(app_touch_init() == 0U);
    assert(state().controller == DEVICE_GT);
    poll(6900U, SCAN_DOWN);
    assert(state().pressed == 1U && state().finger_count == 2U);
    mock_all_transfers_fail = 1U;
    poll(7000U, SCAN_FAIL);
    assert(state().ready == 0U && state().pressed == 0U && state().finger_count == 0U);
    assert(state().error_since_ms == 7000U && state().error_count == 1U);
    poll(7001U, SCAN_SKIP);
    assert(state().error_count == 1U && state().last_success_ms == 6900U);
    poll(8999U, SCAN_SKIP);
    assert(mock_gt_init_calls == 1U);
    poll(9000U, SCAN_SKIP);
    assert(mock_gt_init_calls == 2U && mock_ft_init_calls == 1U);
    assert(state().controller == DEVICE_GT && state().ready == 0U);
    assert(state().recovery_count == 0U);
    mock_all_transfers_fail = 0U;
    poll(13999U, SCAN_SKIP);
    assert(mock_gt_init_calls == 2U);
    poll(14000U, SCAN_SKIP);
    assert(mock_gt_init_calls == 3U && mock_ft_init_calls == 1U);
    assert(state().ready == 1U && state().recovery_count == 1U);
    assert(state().last_success_ms == 14000U && state().error_since_ms == 0U);
}

static void test_short_failure_success_and_skip(void)
{
    reset_mock(1000U, DEVICE_FT);
    assert(app_touch_init() == 0U);
    poll(7000U, SCAN_DOWN);
    assert(state().x == 320U && state().y == 220U && state().pressed == 1U);
    assert(mock_gpio_read_calls == 4U);
    mock_gpio_level = GPIO_PIN_RESET;
    poll(7010U, SCAN_FAIL);
    assert(state().ready == 0U && state().pressed == 0U);
    assert(mock_gpio_read_calls == 8U);
    assert(state().scl_level == 0U && state().sda_level == 0U);
    assert(state().reset_level == 0U && state().interrupt_level == 0U);
    poll(7011U, SCAN_SKIP);
    assert(state().error_count == 1U && state().last_success_ms == 7000U);
    assert(mock_gpio_read_calls == 8U);
    poll(8900U, SCAN_UP);
    assert(state().ready == 1U && state().error_since_ms == 0U);
    assert(state().last_success_ms == 8900U && state().recovery_count == 0U);
    assert(mock_gpio_read_calls == 8U);
    poll(9999U, SCAN_SKIP);
    assert(mock_ft_init_calls == 1U && mock_gt_init_calls == 0U);
    assert(state().last_success_ms == 8900U && state().error_count == 1U);
    poll(13000U, SCAN_DOWN);
    poll(13050U, SCAN_SKIP);
    assert(state().pressed == 1U && state().finger_count == 2U);
    assert(state().last_success_ms == 13000U);
}

static void test_retry_tick_wraparound(void)
{
    uint32_t start = UINT32_MAX - 2000U;
    reset_mock(start, 0U);
    assert(app_touch_init() == 1U);
    mock_devices = DEVICE_FT;
    poll(start + 4999U, SCAN_SKIP);
    assert(mock_ft_init_calls == 1U);
    poll(start + 5000U, SCAN_SKIP);
    assert(mock_ft_init_calls == 2U && state().ready == 1U);
    assert(state().last_success_ms == start + 5000U);

    reset_mock(UINT32_MAX - 6000U, DEVICE_FT);
    assert(app_touch_init() == 0U);
    start = UINT32_MAX - 1000U;
    poll(start, SCAN_FAIL);
    poll(start + 1999U, SCAN_SKIP);
    assert(mock_ft_init_calls == 1U);
    poll(start + 2000U, SCAN_SKIP);
    assert(mock_ft_init_calls == 2U && state().ready == 1U);
    assert(state().recovery_count == 1U);
}

static void test_retry_spacing_after_recent_init(void)
{
    reset_mock(1000U, DEVICE_FT);
    assert(app_touch_init() == 0U);
    poll(1100U, SCAN_FAIL);
    poll(3100U, SCAN_SKIP);
    assert(mock_ft_init_calls == 1U && state().ready == 0U);
    poll(5999U, SCAN_SKIP);
    assert(mock_ft_init_calls == 1U);
    poll(6000U, SCAN_SKIP);
    assert(mock_ft_init_calls == 2U && state().recovery_count == 1U);
}

static void test_probe_failure_keeps_identified_controller(void)
{
    reset_mock(1000U, DEVICE_FT);
    mock_probe_fails = 1U;
    assert(app_touch_init() == 1U);
    assert(state().controller == DEVICE_FT && mock_gt_init_calls == 0U);
    mock_probe_fails = 0U;
    poll(6000U, SCAN_SKIP);
    assert(mock_ft_init_calls == 2U && mock_gt_init_calls == 0U);
    assert(state().ready == 1U && state().recovery_count == 1U);
}

static void test_zero_failure_timestamp(void)
{
    reset_mock(UINT32_MAX - 5000U, DEVICE_FT);
    assert(app_touch_init() == 0U);
    poll(0U, SCAN_FAIL);
    assert(state().error_count == 1U && state().error_since_ms == 0U);
    poll(1999U, SCAN_SKIP);
    assert(mock_ft_init_calls == 1U);
    poll(2000U, SCAN_SKIP);
    assert(mock_ft_init_calls == 2U && state().recovery_count == 1U);
}

static void test_real_ctiic_sequence(void)
{
    uint8_t data = 0U;
    uint32_t before;
    reset_mock(1000U, DEVICE_FT | DEVICE_GT);
    before = ct_iic_transaction_sequence();
    assert(ct_iic_init() == 0U);
    assert(ct_iic_transaction_sequence() == before);
    assert(ct_iic_probe(FT5206_CMD_WR) == 0U);
    assert(ct_iic_transaction_sequence() == before + 1U);
    assert(ct_iic_read_reg(FT5206_CMD_WR, 2U, CT_IIC_REG8, &data, 1U) == 0U);
    assert(ct_iic_transaction_sequence() == before + 2U);
    assert(ct_iic_write_reg(GT9XXX_CMD_WR, 0x814EU, CT_IIC_REG16, &data, 1U) == 0U);
    assert(ct_iic_transaction_sequence() == before + 3U);
    assert(ct_iic_read_reg(FT5206_CMD_WR, 0x123U, CT_IIC_REG8, &data, 1U) == 1U);
    assert(ct_iic_transaction_sequence() == before + 3U);
}

int main(void)
{
    test_initial_failure_retry();
    test_persistent_failure_and_known_controller();
    test_short_failure_success_and_skip();
    test_retry_tick_wraparound();
    test_retry_spacing_after_recent_init();
    test_probe_failure_keeps_identified_controller();
    test_zero_failure_timestamp();
    test_real_ctiic_sequence();
    puts("PASS: touch init retry, persistent/short failures, release, skips, controller retention, and tick wraparound.");
    return 0;
}
