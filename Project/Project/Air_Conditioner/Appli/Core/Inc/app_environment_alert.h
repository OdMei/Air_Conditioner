#ifndef APP_ENVIRONMENT_ALERT_H
#define APP_ENVIRONMENT_ALERT_H

#include <stdint.h>

#define APP_ENV_CHANNEL_TEMPERATURE (0U)
#define APP_ENV_CHANNEL_HUMIDITY (1U)
#define APP_ENV_CHANNEL_COUNT (2U)
#define APP_ENV_CHANNEL_MASK_TEMPERATURE (0x01U)
#define APP_ENV_CHANNEL_MASK_HUMIDITY (0x02U)
#define APP_ENV_CHANNEL_MASK_ALL (0x03U)

#define APP_ENV_SOURCE_BOARD (0U)
#define APP_ENV_SOURCE_SIMULATED (1U)
#define APP_ENV_SOURCE_DEMO (2U)

#ifndef APP_ENV_HOLD_MIN_MS
#define APP_ENV_HOLD_MIN_MS (30000UL)
#endif
#define APP_ENV_HOLD_MAX_MS (1800000UL)
#define APP_ENV_SNOOZE_MIN_MS (60000UL)
#define APP_ENV_SNOOZE_MAX_MS (1800000UL)
#define APP_ENV_DEFAULT_HOLD_MS (120000UL)
#define APP_ENV_DEFAULT_SNOOZE_MS (300000UL)

/* Events occupy one byte per channel. Use the extraction macro, not UI state
 * transitions, to create event records; UNKNOWN never implies RECOVERED. */
#define APP_ENV_EVENT_RAISED (0x01U)
#define APP_ENV_EVENT_RECOVERED (0x02U)
#define APP_ENV_EVENT_ACK (0x04U)
#define APP_ENV_EVENT_SNOOZE (0x08U)
#define APP_ENV_EVENT_UNKNOWN (0x10U)
#define APP_ENV_EVENT_CONFIG_RESET (0x20U)
#define APP_ENV_EVENT_SOURCE_RESET (0x40U)
#define APP_ENV_EVENTS_FOR_CHANNEL(events, channel) \
    ((uint8_t)(((uint16_t)(events) >> ((uint8_t)(channel) * 8U)) & 0xffU))

typedef uint16_t app_environment_events_t;

typedef enum
{
    APP_ENV_NORMAL = 0,
    APP_ENV_PENDING,
    APP_ENV_ACTIVE,
    APP_ENV_ACKNOWLEDGED,
    APP_ENV_SNOOZED,
    APP_ENV_UNKNOWN
} app_environment_status_t;

typedef struct
{
    int16_t temperature_high_x10; /* Personal threshold: 160..400. */
    uint8_t humidity_high;        /* Personal threshold: 30..95 percent. */
    uint32_t hold_ms;             /* Continuous fresh readings only. */
    uint32_t snooze_ms;
} app_environment_config_t;

typedef struct
{
    int16_t temperature_x10;
    uint16_t humidity_x10;
    /* Caller sets each flag only for a valid, fresh reading. A cached value
     * after read failure, missing sample or stale sample must use zero. */
    uint8_t temperature_valid;
    uint8_t humidity_valid;
    uint8_t source;
} app_environment_input_t;

typedef struct
{
    uint32_t pending_since_ms;
    uint32_t raised_since_ms;
    uint32_t snooze_since_ms;
    int16_t value_x10; /* Last good value; never present it as fresh if !fresh. */
    uint8_t fresh;
    uint8_t pending;
    uint8_t raised;
    uint8_t acknowledged;
    uint8_t snoozed;
} app_environment_channel_state_t;

typedef struct
{
    app_environment_config_t config;
    app_environment_channel_state_t channels[APP_ENV_CHANNEL_COUNT];
    uint32_t last_updated_ms;
    uint8_t source;
    uint8_t clock_initialized;
} app_environment_alert_t;

typedef struct
{
    app_environment_status_t status;
    int16_t value_x10;
    int16_t threshold_x10;
    uint32_t pending_ms;
    uint32_t active_ms;
    uint32_t snooze_remaining_ms;
    uint8_t fresh;
    uint8_t raised;
    uint8_t acknowledged;
} app_environment_channel_view_t;

typedef struct
{
    app_environment_config_t config;
    app_environment_channel_view_t channels[APP_ENV_CHANNEL_COUNT];
    uint8_t source;
} app_environment_view_t;

void app_environment_config_defaults(app_environment_config_t *config);
void app_environment_init(app_environment_alert_t *state,
                          const app_environment_config_t *config,
                          uint8_t source);
/* Out-of-range configuration is clamped. A changed configuration opens a new
 * confirmation session; it never emits a false recovery event. */
app_environment_events_t app_environment_set_config(
    app_environment_alert_t *state, const app_environment_config_t *config,
    uint32_t now_ms);
/* Call from one owner, with a monotonic uint32 millisecond clock. Unsigned
 * subtraction permits tick wrap; backwards jumps open a clean session. */
app_environment_events_t app_environment_update(
    app_environment_alert_t *state, const app_environment_input_t *input,
    uint32_t now_ms);
app_environment_events_t app_environment_ack(app_environment_alert_t *state,
                                             uint8_t channel_mask,
                                             uint32_t now_ms);
app_environment_events_t app_environment_snooze(app_environment_alert_t *state,
                                                uint8_t channel_mask,
                                                uint32_t now_ms);
void app_environment_get_view(const app_environment_alert_t *state,
                              uint32_t now_ms, app_environment_view_t *view);

#endif
