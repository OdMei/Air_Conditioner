#include "app_touch.h"
#include "ctiic.h"
#include "ft5206.h"
#include "gt9xxx.h"
#include "touch.h"
#include <string.h>

_m_tp_dev tp_dev;

#define APP_TOUCH_RETRY_MS          (5000U)
#define APP_TOUCH_FAILURE_REINIT_MS (2000U)
#define APP_TOUCH_CONTROLLER_FT     (1U)
#define APP_TOUCH_CONTROLLER_GT     (2U)

static uint8_t s_touch_initialized;
static uint8_t s_touch_failure_active;
static uint32_t s_touch_last_init_attempt_ms;
static app_touch_state_t s_touch_state;

static void app_touch_capture_bus_diag(void)
{
    bsp_i2c2_diag_t diag;
    if (ct_iic_get_diag(&diag) == 0U)
    {
        s_touch_state.error_kind = diag.error_kind;
        s_touch_state.last_hal_status = diag.last_hal_status;
        s_touch_state.last_hal_error = diag.last_hal_error;
        s_touch_state.last_hal_state = diag.last_hal_state;
        s_touch_state.last_isr = diag.last_isr;
        s_touch_state.last_cr2 = diag.last_cr2;
        s_touch_state.last_elapsed_ms = diag.last_elapsed_ms;
        s_touch_state.bus_recovery_count = diag.bus_recovery_count;
    }
}

static void app_touch_capture_levels(void)
{
    s_touch_state.scl_level = (uint8_t)HAL_GPIO_ReadPin(GPIOD, GPIO_PIN_14);
    s_touch_state.sda_level = (uint8_t)HAL_GPIO_ReadPin(GPIOD, GPIO_PIN_4);
    s_touch_state.reset_level =
        (uint8_t)HAL_GPIO_ReadPin(GPIOD, GPIO_PIN_10);
    s_touch_state.interrupt_level =
        (uint8_t)HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_3);
}

static void app_touch_release_pointer(void)
{
    s_touch_state.ready = 0U;
    s_touch_state.pressed = 0U;
    s_touch_state.finger_count = 0U;
    tp_dev.sta = 0U;
}

static void app_touch_record_failure(uint32_t now)
{
    if (s_touch_failure_active == 0U)
    {
        s_touch_failure_active = 1U;
        s_touch_state.error_since_ms = now;
    }
    s_touch_state.error_count++;
    app_touch_release_pointer();
    app_touch_capture_levels();
    app_touch_capture_bus_diag();
}

static void app_touch_record_success(uint32_t now)
{
    s_touch_failure_active = 0U;
    s_touch_state.error_since_ms = 0U;
    s_touch_state.last_success_ms = now;
    s_touch_state.ready = 1U;
    app_touch_capture_bus_diag();
}

static uint8_t app_touch_try_initialize(uint8_t is_recovery)
{
    uint8_t success = 0U;

    s_touch_initialized = 0U;
    app_touch_release_pointer();
    tp_dev.touchtype = 0x81U;

    /* Once identified, retry only that controller. The shared I2C2 init is
       idempotent; neither the bus nor the camera is reset here. */
    if ((s_touch_state.controller == 0U) ||
        (s_touch_state.controller == APP_TOUCH_CONTROLLER_FT))
    {
        s_touch_state.ft_probe = ft5206_init();
        s_touch_state.ft_ack = ct_iic_probe(FT5206_CMD_WR);
        if (s_touch_state.ft_probe == 0U)
        {
            s_touch_state.controller = APP_TOUCH_CONTROLLER_FT;
            tp_dev.scan = ft5206_scan;
            success = (s_touch_state.ft_ack == 0U) ? 1U : 0U;
        }
    }

    if ((s_touch_state.controller == 0U) ||
        (s_touch_state.controller == APP_TOUCH_CONTROLLER_GT))
    {
        s_touch_state.gt_probe = gt9xxx_init();
        s_touch_state.gt_ack = ct_iic_probe(GT9XXX_CMD_WR);
        if (s_touch_state.gt_probe == 0U)
        {
            s_touch_state.controller = APP_TOUCH_CONTROLLER_GT;
            tp_dev.scan = gt9xxx_scan;
            success = (s_touch_state.gt_ack == 0U) ? 1U : 0U;
        }
    }

    s_touch_last_init_attempt_ms = HAL_GetTick();
    app_touch_capture_levels();
    if (success != 0U)
    {
        s_touch_initialized = 1U;
        app_touch_record_success(s_touch_last_init_attempt_ms);
        if (is_recovery != 0U) { s_touch_state.recovery_count++; }
        return 0U;
    }
    app_touch_record_failure(s_touch_last_init_attempt_ms);
    return 1U;
}

uint8_t app_touch_init(void)
{
    memset(&s_touch_state, 0, sizeof(s_touch_state));
    memset(&tp_dev, 0, sizeof(tp_dev));
    s_touch_initialized = 0U;
    s_touch_failure_active = 0U;
    s_touch_state.ft_probe = 1U;
    s_touch_state.gt_probe = 1U;
    s_touch_state.ft_ack = 1U;
    s_touch_state.gt_ack = 1U;
    /* The LCD is 800x480 landscape.  FT5206 uses bit 0 to select the
       landscape axis order; bit 7 marks a capacitive controller. */
    return app_touch_try_initialize(0U);
}

void app_touch_poll(void)
{
    uint32_t now = HAL_GetTick();
    uint32_t before;
    uint16_t points;

    if (s_touch_initialized == 0U)
    {
        if ((uint32_t)(now - s_touch_last_init_attempt_ms) >= APP_TOUCH_RETRY_MS)
        {
            (void)app_touch_try_initialize(1U);
        }
        return;
    }
    if (tp_dev.scan == 0)
    {
        s_touch_initialized = 0U;
        app_touch_record_failure(now);
        return;
    }

    before = ct_iic_transaction_sequence();
    s_touch_state.scan_count++;
    (void)tp_dev.scan(0U);
    now = HAL_GetTick();
    if (ct_iic_transaction_sequence() != before)
    {
        if (ct_iic_last_error() != 0U)
        {
            /* One short mutex wait/transfer failure is a communication
               error, not evidence of a disconnected controller. */
            app_touch_record_failure(now);
        }
        else
        {
            app_touch_record_success(now);
            points = tp_dev.sta & 0x03FFU;
            s_touch_state.finger_count = 0U;
            while (points != 0U)
            {
                s_touch_state.finger_count += (uint8_t)(points & 1U);
                points >>= 1U;
            }
            s_touch_state.pressed =
                ((tp_dev.sta & TP_PRES_DOWN) != 0U) ? 1U : 0U;
            if (s_touch_state.pressed != 0U)
            {
                s_touch_state.x = tp_dev.x[0];
                s_touch_state.y = tp_dev.y[0];
            }
        }
    }
    /* A throttled scan made no transaction: keep its previous health and
       pointer state, do not count another error or manufacture a success. */
    if ((s_touch_failure_active != 0U) &&
        ((uint32_t)(now - s_touch_state.error_since_ms) >= APP_TOUCH_FAILURE_REINIT_MS) &&
        ((uint32_t)(now - s_touch_last_init_attempt_ms) >= APP_TOUCH_RETRY_MS))
    {
        (void)app_touch_try_initialize(1U);
    }
}

void app_touch_get_state(app_touch_state_t *state)
{
    if (state != 0)
    {
        *state = s_touch_state;
    }
}
