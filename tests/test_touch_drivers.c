#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "touch.h"
#include "ctiic.h"

/* Only the hardware boundary is mocked. The two scan/init implementations
 * and their actual register constants are linked from Drivers/BSP/TOUCH. */
GPIO_TypeDef touch_drivers_gpiob;
GPIO_TypeDef touch_drivers_gpiod;
_m_tp_dev tp_dev;
extern uint8_t g_gt_tnum;

typedef struct {
    uint8_t write;
    uint8_t address;
    uint8_t reg_bytes;
    uint16_t reg;
    uint16_t length;
    uint8_t value;
} transaction_t;

enum { FT = 1, GT = 2, LOG_CAPACITY = 64 };
static transaction_t transactions[LOG_CAPACITY];
static unsigned transaction_count;
static unsigned protocol_errors;
static unsigned failures;
static unsigned cases;
static unsigned fail_transfer;
static uint8_t fail_init;
static uint8_t status_byte;
static uint8_t ft_points[5][4];
static uint8_t gt_points[76];
static uint8_t ft_version[2];
static uint8_t gt_product[4];
static uint32_t clock_ms;
static unsigned gpio_inits;
static unsigned reset_writes;
static uint8_t reset_levels[4];
static uint8_t device;

#define CHECK(expression) do { \
    if (!(expression)) { \
        fprintf(stderr, "FAIL %s:%u: %s\n", __func__, (unsigned)__LINE__, #expression); \
        failures++; return; \
    } \
} while (0)

uint32_t HAL_GetTick(void) { return clock_ms; }
void HAL_Delay(uint32_t milliseconds) { clock_ms += milliseconds; }
void HAL_GPIO_Init(GPIO_TypeDef *port, GPIO_InitTypeDef *config)
{
    if ((port != GPIOB && port != GPIOD) || config == NULL) { protocol_errors++; }
    gpio_inits++;
}
void HAL_GPIO_WritePin(GPIO_TypeDef *port, uint16_t pin, GPIO_PinState state)
{
    if (port != GPIOD || pin != GPIO_PIN_10) { protocol_errors++; }
    if (reset_writes < sizeof(reset_levels)) { reset_levels[reset_writes] = (uint8_t)state; }
    reset_writes++;
}
GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef *port, uint16_t pin)
{ (void)port; (void)pin; return GPIO_PIN_SET; }

uint8_t ct_iic_init(void) { return fail_init; }

static uint8_t log_transfer(uint8_t write, uint8_t address, uint16_t reg,
                            uint8_t reg_bytes, uint8_t *data, uint16_t length)
{
    transaction_t *transaction;
    uint8_t expected_address = device == FT ? FT5206_CMD_WR : GT9XXX_CMD_WR;
    uint8_t expected_reg_bytes = device == FT ? CT_IIC_REG8 : CT_IIC_REG16;
    if (transaction_count >= LOG_CAPACITY) { protocol_errors++; return 1U; }
    transaction = &transactions[transaction_count++];
    transaction->write = write;
    transaction->address = address;
    transaction->reg_bytes = reg_bytes;
    transaction->reg = reg;
    transaction->length = length;
    transaction->value = write && data != NULL && length != 0U ? data[0] : 0U;
    if (address != expected_address || reg_bytes != expected_reg_bytes ||
        data == NULL || length == 0U) { protocol_errors++; return 1U; }
    if (fail_transfer != 0U && transaction_count == fail_transfer) { return 1U; }
    return 0U;
}

uint8_t ct_iic_write_reg(uint8_t address, uint16_t reg, uint8_t reg_bytes,
                        uint8_t *data, uint16_t length)
{
    if (log_transfer(1U, address, reg, reg_bytes, data, length)) { return 1U; }
    if (device == GT && reg == GT9XXX_GSTID_REG &&
        (length != 1U || data[0] != 0U)) { protocol_errors++; return 1U; }
    return 0U;
}

uint8_t ct_iic_read_reg(uint8_t address, uint16_t reg, uint8_t reg_bytes,
                       uint8_t *data, uint16_t length)
{
    unsigned point;
    if (log_transfer(0U, address, reg, reg_bytes, data, length)) { return 1U; }
    if (device == FT) {
        if (reg == FT5206_ID_G_LIB_VERSION && length == 2U) {
            memcpy(data, ft_version, 2U); return 0U;
        }
        if (reg == FT5206_REG_NUM_FINGER && length == 1U) {
            data[0] = status_byte; return 0U;
        }
        for (point = 0U; point < 5U; point++) {
            if (reg == FT5206_TP1_REG + point * 6U && length == 4U) {
                memcpy(data, ft_points[point], 4U); return 0U;
            }
        }
    } else {
        if (reg == GT9XXX_PID_REG && length == 4U) {
            memcpy(data, gt_product, 4U); return 0U;
        }
        if (reg == GT9XXX_GSTID_REG && length == 1U) {
            data[0] = status_byte; return 0U;
        }
        if (reg == GT9XXX_TP1_REG && length >= 4U && length <= sizeof(gt_points) &&
            (length - 4U) % 8U == 0U) {
            memcpy(data, gt_points, length); return 0U;
        }
    }
    protocol_errors++;
    return 1U;
}

static void reset_mock(uint8_t selected_device)
{
    device = selected_device;
    memset(&tp_dev, 0, sizeof(tp_dev));
    tp_dev.touchtype = 0x81U;
    memset(transactions, 0, sizeof(transactions));
    memset(ft_points, 0, sizeof(ft_points));
    memset(gt_points, 0, sizeof(gt_points));
    memset(reset_levels, 0, sizeof(reset_levels));
    transaction_count = protocol_errors = fail_transfer = 0U;
    fail_init = status_byte = 0U;
    clock_ms = gpio_inits = reset_writes = 0U;
    ft_version[0] = 0x30U; ft_version[1] = 0x03U;
    memcpy(gt_product, "911", 4U);
    g_gt_tnum = 5U;
}

/* Drivers deliberately throttle idle polling. Wait for one actual transaction
 * instead of replacing their implementation or assuming internal static state. */
static uint8_t scan_report(void)
{
    unsigned attempt;
    uint8_t result = 0U;
    transaction_count = 0U;
    for (attempt = 0U; attempt < 12U; attempt++) {
        result = device == FT ? ft5206_scan(0U) : gt9xxx_scan(0U);
        if (transaction_count != 0U) { return result; }
    }
    protocol_errors++;
    return result;
}

static void ft_point(unsigned point, uint16_t raw_x, uint16_t raw_y, uint8_t event)
{
    ft_points[point][0] = (uint8_t)((raw_x >> 8U) & 0x0FU) | event;
    ft_points[point][1] = (uint8_t)raw_x;
    /* High Y nibble is a touch ID, not coordinate bits. */
    ft_points[point][2] = (uint8_t)((raw_y >> 8U) & 0x0FU) | (uint8_t)(point << 4U);
    ft_points[point][3] = (uint8_t)raw_y;
}

static void gt_point(unsigned point, uint16_t raw_x, uint16_t raw_y)
{
    uint8_t *record = &gt_points[point * 8U];
    record[0] = (uint8_t)raw_x; record[1] = (uint8_t)(raw_x >> 8U);
    record[2] = (uint8_t)raw_y; record[3] = (uint8_t)(raw_y >> 8U);
}

static void test_register_wrappers(void)
{
    uint8_t data[4] = {1U, 2U, 3U, 4U};
    reset_mock(FT);
    CHECK(ft5206_wr_reg(0x80U, data, 1U) == 0U);
    CHECK(transactions[0].address == 0x70U && transactions[0].reg_bytes == 1U);
    CHECK(ft5206_rd_reg(FT5206_ID_G_LIB_VERSION, data, 2U) == 0U);
    CHECK(transactions[1].reg == 0xA1U && transactions[1].length == 2U);
    reset_mock(GT);
    CHECK(gt9xxx_wr_reg(GT9XXX_CTRL_REG, data, 1U) == 0U);
    CHECK(transactions[0].address == 0x28U && transactions[0].reg_bytes == 2U);
    CHECK(gt9xxx_rd_reg(GT9XXX_PID_REG, data, 4U) == 0U);
    CHECK(transactions[1].reg == 0x8140U && transactions[1].length == 4U);
    CHECK(protocol_errors == 0U);
}

static void test_ft_init_contract(void)
{
    const uint16_t registers[] = {0x00U, 0xA4U, 0x80U, 0x88U};
    const uint8_t values[] = {0U, 0U, 22U, 12U};
    unsigned i;
    reset_mock(FT);
    CHECK(ft5206_init() == 0U);
    CHECK(transaction_count == 5U && gpio_inits == 2U);
    CHECK(clock_ms == 70U && reset_writes == 2U);
    CHECK(reset_levels[0] == 0U && reset_levels[1] == 1U);
    for (i = 0U; i < 4U; i++) {
        CHECK(transactions[i].write && transactions[i].reg == registers[i]);
        CHECK(transactions[i].length == 1U && transactions[i].value == values[i]);
    }
    CHECK(transactions[4].write == 0U && transactions[4].reg == 0xA1U);
    CHECK(protocol_errors == 0U);
}

static void test_ft_init_versions_and_faults(void)
{
    const uint8_t versions[][2] = {{0x30U, 3U}, {0U, 1U}, {0U, 2U}, {0U, 0U}};
    unsigned i;
    for (i = 0U; i < sizeof(versions) / sizeof(versions[0]); i++) {
        reset_mock(FT); memcpy(ft_version, versions[i], 2U);
        CHECK(ft5206_init() == 0U);
    }
    reset_mock(FT); ft_version[0] = 0xFFU; ft_version[1] = 0xFFU;
    CHECK(ft5206_init() != 0U);
    reset_mock(FT); fail_init = 1U;
    CHECK(ft5206_init() != 0U && transaction_count == 0U);
    for (i = 1U; i <= 5U; i++) {
        reset_mock(FT); fail_transfer = i;
        CHECK(ft5206_init() != 0U && transaction_count == i);
        CHECK(protocol_errors == 0U);
    }
}

static void test_gt_init_contract(void)
{
    const char *products[] = {"911", "9147", "1158", "9271"};
    unsigned i;
    for (i = 0U; i < sizeof(products) / sizeof(products[0]); i++) {
        reset_mock(GT); memcpy(gt_product, products[i], 4U);
        CHECK(gt9xxx_init() == 0U);
        CHECK(g_gt_tnum == (i == 3U ? 10U : 5U));
        CHECK(transaction_count == 3U && gpio_inits == 3U);
        CHECK(transactions[0].reg == 0x8140U && transactions[0].length == 4U);
        CHECK(transactions[1].write && transactions[1].reg == 0x8040U);
        CHECK(transactions[1].value == 2U && transactions[2].value == 0U);
        CHECK(clock_ms == 130U && reset_writes == 2U);
        CHECK(protocol_errors == 0U);
    }
    /* A previous ten-point product must not leak its limit into a new device. */
    reset_mock(GT); memcpy(gt_product, "9271", 4U); CHECK(gt9xxx_init() == 0U);
    memcpy(gt_product, "911", 4U); CHECK(gt9xxx_init() == 0U && g_gt_tnum == 5U);
}

static void test_gt_init_faults(void)
{
    unsigned i;
    reset_mock(GT); memcpy(gt_product, "9999", 4U);
    CHECK(gt9xxx_init() != 0U && transaction_count == 1U);
    reset_mock(GT); fail_init = 1U;
    CHECK(gt9xxx_init() != 0U && transaction_count == 0U);
    for (i = 1U; i <= 3U; i++) {
        reset_mock(GT); fail_transfer = i;
        CHECK(gt9xxx_init() != 0U && transaction_count == i);
        CHECK(protocol_errors == 0U);
    }
}

static void test_ft_one_and_five_points(void)
{
    unsigned i;
    reset_mock(FT); status_byte = 1U;
    ft_point(0U, 100U, 300U, 0x80U);
    CHECK(scan_report() == 1U);
    CHECK(tp_dev.x[0] == 300U && tp_dev.y[0] == 100U);
    CHECK(tp_dev.sta == (TP_PRES_DOWN | TP_CATH_PRES | 1U));
    CHECK(transaction_count == 2U && transactions[1].reg == 0x03U && transactions[1].length == 4U);
    status_byte = 5U;
    for (i = 0U; i < 5U; i++) { ft_point(i, (uint16_t)(100U + i), (uint16_t)(300U + i), 0x80U); }
    CHECK(scan_report() == 1U);
    CHECK((tp_dev.sta & 0x03FFU) == 0x1FU && transaction_count == 6U);
    for (i = 0U; i < 5U; i++) {
        CHECK(transactions[i + 1U].reg == 0x03U + i * 6U && transactions[i + 1U].length == 4U);
        CHECK(tp_dev.x[i] == 300U + i && tp_dev.y[i] == 100U + i);
    }
    CHECK(protocol_errors == 0U);
}

static void test_ft_zero_and_release(void)
{
    reset_mock(FT); status_byte = 0U;
    CHECK(scan_report() == 0U && !(tp_dev.sta & TP_PRES_DOWN));
    status_byte = 1U; ft_point(0U, 80U, 90U, 0x80U);
    CHECK(scan_report() == 1U && (tp_dev.sta & TP_PRES_DOWN));
    status_byte = 0U;
    CHECK(scan_report() == 0U && !(tp_dev.sta & TP_PRES_DOWN));
    CHECK(transaction_count == 1U && protocol_errors == 0U);
}

static void test_ft_illegal_count_releases(void)
{
    unsigned count;
    for (count = 6U; count <= 15U; count++) {
        reset_mock(FT); status_byte = 1U; ft_point(0U, 80U, 90U, 0x80U);
        CHECK(scan_report() == 1U && (tp_dev.sta & TP_PRES_DOWN));
        status_byte = (uint8_t)count;
        CHECK(scan_report() == 0U);
        CHECK(!(tp_dev.sta & TP_PRES_DOWN));
        CHECK(transaction_count == 1U && protocol_errors == 0U);
    }
    reset_mock(FT); status_byte = 1U; ft_point(0U, 80U, 90U, 0x80U);
    CHECK(scan_report() == 1U);
    status_byte = 0x10U; /* Finger count is the low nibble, not bit 4. */
    CHECK(scan_report() == 0U && !(tp_dev.sta & TP_PRES_DOWN));
}

static void test_ft_origin_and_last_pixel(void)
{
    reset_mock(FT); status_byte = 1U; ft_point(0U, 0U, 0U, 0x80U);
    CHECK(scan_report() == 1U && (tp_dev.sta & TP_PRES_DOWN));
    CHECK(tp_dev.x[0] == 0U && tp_dev.y[0] == 0U);
    ft_point(0U, 479U, 799U, 0x80U);
    CHECK(scan_report() == 1U && tp_dev.x[0] == 799U && tp_dev.y[0] == 479U);
    CHECK(protocol_errors == 0U);
}

static void test_ft_outside_and_other_valid_point(void)
{
    reset_mock(FT); status_byte = 1U; ft_point(0U, 480U, 300U, 0x80U);
    CHECK(scan_report() == 0U && !(tp_dev.sta & TP_PRES_DOWN));
    ft_point(0U, 100U, 800U, 0x80U);
    CHECK(scan_report() == 0U && !(tp_dev.sta & TP_PRES_DOWN));
    status_byte = 2U; ft_point(0U, 480U, 300U, 0x80U); ft_point(1U, 150U, 350U, 0x80U);
    CHECK(scan_report() == 1U && (tp_dev.sta & TP_PRES_DOWN));
    CHECK(tp_dev.x[0] == 350U && tp_dev.y[0] == 150U);
    CHECK((tp_dev.sta & 0x03FFU) == 2U && protocol_errors == 0U);
}

static void test_ft_lift_and_reserved_events(void)
{
    reset_mock(FT); status_byte = 1U; ft_point(0U, 150U, 350U, 0x40U);
    CHECK(scan_report() == 0U && !(tp_dev.sta & TP_PRES_DOWN));
    ft_point(0U, 150U, 350U, 0xC0U);
    CHECK(scan_report() == 0U && !(tp_dev.sta & TP_PRES_DOWN));
    CHECK(protocol_errors == 0U);
}

static void test_ft_down_then_contact_event(void)
{
    reset_mock(FT); status_byte = 1U; ft_point(0U, 150U, 350U, 0x00U);
    CHECK(scan_report() == 1U && (tp_dev.sta & TP_PRES_DOWN));
    CHECK(tp_dev.x[0] == 350U && tp_dev.y[0] == 150U);
    ft_point(0U, 151U, 351U, 0x80U);
    CHECK(scan_report() == 1U && tp_dev.x[0] == 351U && tp_dev.y[0] == 151U);
    ft_point(0U, 151U, 351U, 0x40U);
    CHECK(scan_report() == 0U && !(tp_dev.sta & TP_PRES_DOWN));
    CHECK(protocol_errors == 0U);
}

static void test_ft_transaction_faults(void)
{
    unsigned failure, i;
    for (failure = 1U; failure <= 6U; failure++) {
        reset_mock(FT); status_byte = 5U; fail_transfer = failure;
        tp_dev.sta = TP_PRES_DOWN | 1U;
        for (i = 0U; i < 5U; i++) { ft_point(i, (uint16_t)(100U + i), (uint16_t)(300U + i), 0x80U); }
        CHECK(scan_report() == 0U && !(tp_dev.sta & TP_PRES_DOWN));
        CHECK(transaction_count == failure && protocol_errors == 0U);
        fail_transfer = 0U;
        CHECK(scan_report() == 1U && (tp_dev.sta & TP_PRES_DOWN));
    }
}

static void test_ft_failed_report_does_not_publish_partial_coordinates(void)
{
    unsigned i;
    reset_mock(FT); status_byte = 5U; fail_transfer = 6U;
    for (i = 0U; i < CT_MAX_TOUCH; i++) { tp_dev.x[i] = 123U; tp_dev.y[i] = 234U; }
    for (i = 0U; i < 5U; i++) { ft_point(i, (uint16_t)(100U + i), (uint16_t)(300U + i), 0x80U); }
    CHECK(scan_report() == 0U && tp_dev.sta == 0U);
    for (i = 0U; i < CT_MAX_TOUCH; i++) { CHECK(tp_dev.x[i] == 123U && tp_dev.y[i] == 234U); }
    fail_transfer = 0U;
    CHECK(scan_report() == 1U);
    for (i = 5U; i < CT_MAX_TOUCH; i++) { CHECK(tp_dev.x[i] == 123U && tp_dev.y[i] == 234U); }
    CHECK(protocol_errors == 0U);
}

static void test_ft_reserved_status_bits_do_not_change_count(void)
{
    reset_mock(FT); status_byte = 0xF1U; ft_point(0U, 100U, 300U, 0x80U);
    CHECK(scan_report() == 1U && tp_dev.x[0] == 300U && tp_dev.y[0] == 100U);
    CHECK(transaction_count == 2U && (tp_dev.sta & 0x03FFU) == 1U);
    status_byte = 0xF0U;
    CHECK(scan_report() == 0U && tp_dev.sta == 0U && transaction_count == 1U);
    CHECK(protocol_errors == 0U);
}

static void test_ft_portrait_boundaries(void)
{
    reset_mock(FT); tp_dev.touchtype = 0x80U; status_byte = 1U;
    ft_point(0U, 0U, 200U, 0x80U);
    CHECK(scan_report() == 1U && tp_dev.x[0] == 799U && tp_dev.y[0] == 200U);
    ft_point(0U, 799U, 200U, 0x80U);
    CHECK(scan_report() == 1U && tp_dev.x[0] == 0U && tp_dev.y[0] == 200U);
    ft_point(0U, 800U, 200U, 0x80U);
    CHECK(scan_report() == 0U && !(tp_dev.sta & TP_PRES_DOWN));
    CHECK(protocol_errors == 0U);
}

static void test_idle_throttle_is_not_a_release_transaction(void)
{
    unsigned selected_device;
    for (selected_device = FT; selected_device <= GT; selected_device++) {
        unsigned scan;
        uint8_t saw_skip = 0U;
        reset_mock((uint8_t)selected_device);
        /* Establish enough idle scans to reach each driver's polling gate. */
        for (scan = 0U; scan < 25U; scan++) {
            uint16_t old_state = TP_PRES_DOWN | TP_CATH_PRES | 1U;
            tp_dev.sta = old_state;
            transaction_count = 0U;
            if (device == FT) { (void)ft5206_scan(0U); }
            else { (void)gt9xxx_scan(0U); }
            if (transaction_count == 0U) {
                CHECK(tp_dev.sta == old_state);
                saw_skip = 1U;
            }
        }
        CHECK(saw_skip && protocol_errors == 0U);
    }
}

static void test_gt_one_five_and_ten_points(void)
{
    const unsigned counts[] = {1U, 5U, 10U};
    unsigned c, i, count;
    for (c = 0U; c < sizeof(counts) / sizeof(counts[0]); c++) {
        reset_mock(GT); count = counts[c]; g_gt_tnum = count == 10U ? 10U : 5U;
        status_byte = (uint8_t)(0x80U | count);
        for (i = 0U; i < count; i++) { gt_point(i, (uint16_t)(300U + i), (uint16_t)(100U + i)); }
        CHECK(scan_report() == 1U);
        CHECK(transaction_count == 3U);
        CHECK(transactions[0].reg == 0x814EU && transactions[0].length == 1U);
        CHECK(transactions[1].reg == 0x8150U && transactions[1].length == (count - 1U) * 8U + 4U);
        CHECK(transactions[2].write && transactions[2].reg == 0x814EU && transactions[2].value == 0U);
        CHECK((tp_dev.sta & 0x03FFU) == (1U << count) - 1U);
        for (i = 0U; i < count; i++) { CHECK(tp_dev.x[i] == 300U + i && tp_dev.y[i] == 100U + i); }
        CHECK(protocol_errors == 0U);
    }
}

static void test_gt_zero_and_release(void)
{
    reset_mock(GT); status_byte = 0x80U;
    CHECK(scan_report() == 0U && tp_dev.sta == 0U && transaction_count == 2U);
    status_byte = 0x81U; gt_point(0U, 300U, 100U);
    CHECK(scan_report() == 1U && (tp_dev.sta & TP_PRES_DOWN));
    status_byte = 0x80U;
    CHECK(scan_report() == 0U && tp_dev.sta == 0U && transaction_count == 2U);
    CHECK(transactions[1].write && protocol_errors == 0U);
}

static void test_gt_not_ready_preserves_state(void)
{
    reset_mock(GT); status_byte = 0x81U; gt_point(0U, 300U, 100U);
    CHECK(scan_report() == 1U);
    status_byte = 1U;
    CHECK(scan_report() == 0U && (tp_dev.sta & TP_PRES_DOWN));
    CHECK(tp_dev.x[0] == 300U && tp_dev.y[0] == 100U);
    CHECK(transaction_count == 1U && !transactions[0].write && protocol_errors == 0U);
}

static void test_gt_illegal_counts(void)
{
    unsigned count, limit;
    for (limit = 5U; limit <= 10U; limit += 5U) {
        for (count = limit + 1U; count <= 15U; count++) {
            reset_mock(GT); g_gt_tnum = (uint8_t)limit;
            tp_dev.sta = TP_PRES_DOWN | 1U; status_byte = (uint8_t)(0x80U | count);
            CHECK(scan_report() == 0U && tp_dev.sta == 0U);
            CHECK(transaction_count == 2U && transactions[1].write && protocol_errors == 0U);
        }
    }
}

static void test_gt_origin_bounds_and_valid_fallback(void)
{
    reset_mock(GT); status_byte = 0x81U; gt_point(0U, 0U, 0U);
    CHECK(scan_report() == 1U && tp_dev.x[0] == 0U && tp_dev.y[0] == 0U);
    gt_point(0U, 799U, 479U);
    CHECK(scan_report() == 1U && tp_dev.x[0] == 799U && tp_dev.y[0] == 479U);
    gt_point(0U, 800U, 100U);
    CHECK(scan_report() == 0U && tp_dev.sta == 0U);
    gt_point(0U, 300U, 480U);
    CHECK(scan_report() == 0U && tp_dev.sta == 0U);
    status_byte = 0x82U; gt_point(0U, 800U, 100U); gt_point(1U, 320U, 120U);
    CHECK(scan_report() == 1U && tp_dev.x[0] == 320U && tp_dev.y[0] == 120U);
    CHECK((tp_dev.sta & 0x03FFU) == 2U && protocol_errors == 0U);
}

static void test_gt_transaction_faults(void)
{
    unsigned failure;
    for (failure = 1U; failure <= 3U; failure++) {
        reset_mock(GT); status_byte = 0x81U; gt_point(0U, 300U, 100U);
        tp_dev.sta = TP_PRES_DOWN | 1U; fail_transfer = failure;
        CHECK(scan_report() == 0U && tp_dev.sta == 0U);
        CHECK(transaction_count == failure && protocol_errors == 0U);
        if (failure == 2U) { CHECK(!transactions[transaction_count - 1U].write); }
        fail_transfer = 0U;
        CHECK(scan_report() == 1U && (tp_dev.sta & TP_PRES_DOWN));
    }
}

static void test_gt_failed_read_does_not_acknowledge_or_publish(void)
{
    unsigned i;
    reset_mock(GT); status_byte = 0x8AU; g_gt_tnum = 10U; fail_transfer = 2U;
    for (i = 0U; i < CT_MAX_TOUCH; i++) {
        gt_point(i, (uint16_t)(300U + i), (uint16_t)(100U + i));
        tp_dev.x[i] = 123U; tp_dev.y[i] = 234U;
    }
    CHECK(scan_report() == 0U && tp_dev.sta == 0U && transaction_count == 2U);
    CHECK(!transactions[1].write && transactions[1].length == 76U);
    for (i = 0U; i < CT_MAX_TOUCH; i++) { CHECK(tp_dev.x[i] == 123U && tp_dev.y[i] == 234U); }
    fail_transfer = 0U;
    CHECK(scan_report() == 1U && transaction_count == 3U);
    CHECK(transactions[2].write && protocol_errors == 0U);
}

static void test_gt_portrait_boundaries(void)
{
    reset_mock(GT); tp_dev.touchtype = 0x80U; status_byte = 0x81U;
    gt_point(0U, 200U, 0U);
    CHECK(scan_report() == 1U && tp_dev.x[0] == 799U && tp_dev.y[0] == 200U);
    gt_point(0U, 200U, 799U);
    CHECK(scan_report() == 1U && tp_dev.x[0] == 0U && tp_dev.y[0] == 200U);
    gt_point(0U, 200U, 800U);
    CHECK(scan_report() == 0U && tp_dev.sta == 0U && protocol_errors == 0U);
}

static void run_case(const char *name, void (*test)(void))
{
    unsigned before = failures;
    cases++;
    test();
    if (before == failures) { printf("PASS %s\n", name); }
}

int main(void)
{
    run_case("register address and size wrappers", test_register_wrappers);
    run_case("FT init register/reset contract", test_ft_init_contract);
    run_case("FT versions and every init transfer failure", test_ft_init_versions_and_faults);
    run_case("GT product point limits and reset contract", test_gt_init_contract);
    run_case("GT unsupported product and every init failure", test_gt_init_faults);
    run_case("FT one/five reports and four-byte register reads", test_ft_one_and_five_points);
    run_case("FT zero count and release", test_ft_zero_and_release);
    run_case("FT malformed count releases old press", test_ft_illegal_count_releases);
    run_case("FT screen origin and last pixel", test_ft_origin_and_last_pixel);
    run_case("FT out-of-screen and valid secondary contact", test_ft_outside_and_other_valid_point);
    run_case("FT lift/reserved events", test_ft_lift_and_reserved_events);
    run_case("FT down/contact/up event sequence", test_ft_down_then_contact_event);
    run_case("FT every report transfer failure and recovery", test_ft_transaction_faults);
    run_case("FT partial transfer failure keeps old coordinates unpublished", test_ft_failed_report_does_not_publish_partial_coordinates);
    run_case("FT reserved status bits use low-nibble count", test_ft_reserved_status_bits_do_not_change_count);
    run_case("FT mirrored screen boundaries", test_ft_portrait_boundaries);
    run_case("GT one/five/ten records and acknowledge order", test_gt_one_five_and_ten_points);
    run_case("GT zero count and release", test_gt_zero_and_release);
    run_case("GT report-not-ready retains previous complete state", test_gt_not_ready_preserves_state);
    run_case("GT malformed five/ten point counts", test_gt_illegal_counts);
    run_case("GT coordinate edges and secondary contact", test_gt_origin_bounds_and_valid_fallback);
    run_case("GT every report transfer failure and recovery", test_gt_transaction_faults);
    run_case("GT failed ten-point read does not acknowledge or publish", test_gt_failed_read_does_not_acknowledge_or_publish);
    run_case("GT mirrored screen boundaries", test_gt_portrait_boundaries);
    run_case("FT/GT idle poll skips do not manufacture release reports", test_idle_throttle_is_not_a_release_transaction);
    printf("Raw FT/GT drivers: %u cases, %u failures\n", cases, failures);
    return failures != 0U ? 1 : 0;
}
