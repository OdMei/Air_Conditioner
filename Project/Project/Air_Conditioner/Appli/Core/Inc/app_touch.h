#ifndef APP_TOUCH_H
#define APP_TOUCH_H

#include <stdint.h>

typedef struct
{
    uint8_t ready;
    uint8_t pressed;
    uint8_t ft_probe;
    uint8_t gt_probe;
    uint8_t ft_ack;
    uint8_t gt_ack;
    uint8_t scl_level;
    uint8_t sda_level;
    uint8_t reset_level;
    uint8_t interrupt_level;
    uint32_t scan_count;
    uint8_t finger_count;
    uint16_t x;
    uint16_t y;
    uint8_t controller;          /* 0 unknown, 1 FT5206, 2 GT9xxx; retained across failures. */
    uint32_t error_count;        /* Failed initialize attempts or real failed scans. */
    uint32_t recovery_count;     /* Successful automatic retry/reinitialize attempts. */
    uint32_t last_success_ms;    /* Last successful init or real successful scan. */
    uint32_t error_since_ms;     /* Start of the current communication failure. */
    uint8_t error_kind;          /* bsp_i2c2_error_t; retained last failure. */
    uint32_t last_hal_status;
    uint32_t last_hal_error;
    uint32_t last_hal_state;
    uint32_t last_isr;
    uint32_t last_cr2;
    uint32_t last_elapsed_ms;
    uint32_t bus_recovery_count;  /* Host repair attempts completed, not sensor resets. */
} app_touch_state_t;

/**
 * @brief Initialize the official capacitive touch controller driver.
 * @return 0 when a supported controller is available; 1 otherwise.
 */
uint8_t app_touch_init(void);

/**
 * @brief Poll the initialized touch controller for current contact data.
 * @note Call from the input thread. Initial failures retry every 5 seconds.
 *       Runtime failures release the pointer immediately. A failure lasting
 *       2 seconds permits a controller-only reinit, at most once per 5 seconds.
 *       Busy/timeout are communication failures, not proof of disconnection.
 */
void app_touch_poll(void);

/**
 * @brief Copy the latest touch-controller state.
 * @param state Destination for the current state snapshot.
 * @note Call after app_touch_poll from the input thread.
 */
void app_touch_get_state(app_touch_state_t *state);

#endif /* APP_TOUCH_H */
