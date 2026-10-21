#ifndef APP_CLIMATE_LOGIC_H
#define APP_CLIMATE_LOGIC_H

#include <stdint.h>

#define APP_ABSENCE_ECO_MS (600000UL)
#define APP_ABSENCE_OFF_MS (1800000UL)
#define APP_LIGHT_DARK_ENTER (100U)
#define APP_LIGHT_BRIGHT_EXIT (200U)
#define APP_CLIMATE_MODE_AUTO (0U)
#define APP_CLIMATE_MODE_MANUAL_COOL (1U)
#define APP_AI_STALE_MS (3000UL)
#define APP_ALS_STALE_MS (3000UL)
#define APP_SENSOR_STALE_MS (6000UL)
#define APP_PEOPLE_CONFIRM_MS (1000UL)
/* Software demonstration interval, not a real compressor specification. */
#ifndef APP_COOLING_MIN_OFF_MS
#define APP_COOLING_MIN_OFF_MS (30000UL)
#endif

#define APP_REMOTE_KEY_POWER (69U)
#define APP_REMOTE_KEY_TEMP_UP (9U)
#define APP_REMOTE_KEY_TEMP_DOWN (7U)
#define APP_REMOTE_KEY_FAN_1 (22U)
#define APP_REMOTE_KEY_FAN_2 (25U)
#define APP_REMOTE_KEY_FAN_3 (13U)
#define APP_REMOTE_KEY_FAN_4 (12U)
#define APP_REMOTE_KEY_FAN_5 (24U)
#define APP_REMOTE_KEY_TURBO (94U)
#define APP_REMOTE_KEY_SWING_LR (67U)
#define APP_REMOTE_KEY_SWING_UD (21U)
#define APP_REMOTE_KEY_LAMP (90U)
#define APP_REMOTE_KEY_SLEEP (66U)
#define APP_REMOTE_KEY_ECO (71U)
#define APP_REMOTE_KEY_MODE (64U)

typedef enum
{
    APP_LIGHT_DARK = 0,
    APP_LIGHT_BRIGHT = 1
} app_light_t;

typedef enum
{
    APP_PRESENCE_ABSENT = 0,
    APP_PRESENCE_SLEEP,
    APP_PRESENCE_PRESENT,
    APP_PRESENCE_UNKNOWN
} app_presence_t;

typedef enum
{
    APP_ACTION_NONE = 0,
    APP_ACTION_POWER,
    APP_ACTION_MODE_NEXT,
    APP_ACTION_TEMP_DOWN,
    APP_ACTION_TEMP_UP,
    APP_ACTION_FAN_1,
    APP_ACTION_FAN_2,
    APP_ACTION_FAN_3,
    APP_ACTION_FAN_4,
    APP_ACTION_FAN_5,
    APP_ACTION_TURBO,
    APP_ACTION_SWING_LR,
    APP_ACTION_SWING_UD,
    APP_ACTION_LAMP,
    APP_ACTION_SLEEP_TOGGLE,
    APP_ACTION_ECO_TOGGLE,
    APP_ACTION_FAN_NEXT
} app_climate_action_t;

typedef enum
{
    APP_REASON_POWER_OFF = 0,
    APP_REASON_AI_UNKNOWN,
    APP_REASON_CONFIRMING,
    APP_REASON_SENSOR_INVALID,
    APP_REASON_TEMP_SATISFIED,
    APP_REASON_COOLING_WAIT,
    APP_REASON_COOLING,
    APP_REASON_ABSENCE_OFF
} app_climate_reason_t;

typedef enum
{
    APP_DATA_MISSING = 0,
    APP_DATA_FRESH,
    APP_DATA_STALE,
    APP_DATA_INVALID
} app_data_quality_t;

typedef struct
{
    int32_t people_count;
    uint8_t people_valid;
    uint32_t people_updated_ms;
    uint16_t als;
    uint8_t als_valid;
    uint32_t als_updated_ms;
    float temperature;
    float humidity;
    uint8_t sensor_valid;
    uint32_t sensor_updated_ms;
    uint8_t humidity_valid;
    uint32_t humidity_updated_ms;
} app_climate_inputs_t;

typedef struct
{
    uint8_t power_on;
    uint8_t target_temperature;
    uint8_t mode;
    uint8_t fan_speed;
    uint8_t turbo;
    uint8_t swing_lr;
    uint8_t swing_ud;
    uint8_t lamp_on;
    uint8_t manual_sleep;
    uint8_t manual_energy_saving;
    uint8_t auto_sleep;
    uint8_t auto_energy_saving;
    uint8_t light;
    app_presence_t presence;
    uint32_t absence_ms;
    uint8_t people_fresh;
    uint8_t als_fresh;
    uint8_t sensor_fresh;
    uint8_t humidity_fresh;
    app_data_quality_t temperature_quality;
    app_data_quality_t humidity_quality;
    uint8_t people_confirming;
    uint8_t effective_sleep;
    uint8_t effective_eco;
    uint8_t effective_target_temperature;
    uint8_t effective_fan_speed;
    uint8_t cooling_request;
    uint32_t cooling_wait_ms;
    app_climate_reason_t reason;
    /* Internal timing state; timestamps of zero are valid. */
    uint8_t step_initialized;
    uint32_t step_updated_ms;
    uint8_t people_candidate_valid;
    uint8_t people_candidate_present;
    uint32_t people_candidate_since_ms;
    uint8_t cooling_timer_started;
    uint32_t cooling_off_since_ms;
    uint8_t cooling_stop_pending;
    uint8_t thermostat_demand;
    uint8_t automatic_off;
} app_climate_state_t;

void app_climate_init(app_climate_state_t *state, uint8_t target_temperature);
void app_climate_apply_action(app_climate_state_t *state,
                              app_climate_action_t action);
app_climate_action_t app_climate_remote_action(uint8_t key_code);
/* Validity is explicit. Unsigned age subtraction handles HAL tick wrap. */
void app_climate_step(app_climate_state_t *state,
                      const app_climate_inputs_t *inputs, uint32_t now_ms);
uint8_t app_climate_output_mask(const app_climate_state_t *state);
const char *app_climate_reason_name(app_climate_reason_t reason);

#endif
