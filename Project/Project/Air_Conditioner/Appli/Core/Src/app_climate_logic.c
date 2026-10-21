#include "app_climate_logic.h"
#include <math.h>
#include <string.h>

#define APP_MIN_TEMP (16U)
#define APP_MAX_TEMP (30U)

static void derive_preferences(app_climate_state_t *state)
{
    uint8_t automatic = state->mode == APP_CLIMATE_MODE_AUTO;
    uint8_t target = state->target_temperature;
    uint8_t fan = state->turbo ? 5U : state->fan_speed;
    state->effective_sleep = (uint8_t)(state->manual_sleep ||
                                       (automatic && state->auto_sleep));
    state->effective_eco = (uint8_t)(state->manual_energy_saving ||
                                     (automatic && state->auto_energy_saving));
    if (target < APP_MIN_TEMP) { target = APP_MIN_TEMP; }
    if (target > APP_MAX_TEMP) { target = APP_MAX_TEMP; }
    if (state->effective_sleep || state->effective_eco) {
        target = (uint8_t)(target > APP_MAX_TEMP - 2U ? APP_MAX_TEMP : target + 2U);
    }
    if (fan < 1U) { fan = 1U; }
    if (fan > 5U) { fan = 5U; }
    /* Sleep/Eco limits take precedence over Turbo, so simultaneous settings
       cannot defeat the selected quiet/energy-saving demonstration policy. */
    if (state->effective_eco && fan > 2U) { fan = 2U; }
    if (state->effective_sleep) { fan = 1U; }
    state->effective_target_temperature = target;
    state->effective_fan_speed = fan;
}

static void add_absence(app_climate_state_t *state, uint32_t elapsed_ms)
{
    if (state->absence_ms >= APP_ABSENCE_OFF_MS) {
        state->absence_ms = APP_ABSENCE_OFF_MS;
        return;
    }
    uint32_t remaining = APP_ABSENCE_OFF_MS - state->absence_ms;
    state->absence_ms += elapsed_ms < remaining ? elapsed_ms : remaining;
}

void app_climate_init(app_climate_state_t *state, uint8_t target_temperature)
{
    if (state == 0) {
        return;
    }
    memset(state, 0, sizeof(*state));
    state->power_on = 0U;
    state->target_temperature = target_temperature;
    if (state->target_temperature < APP_MIN_TEMP) {
        state->target_temperature = APP_MIN_TEMP;
    }
    if (state->target_temperature > APP_MAX_TEMP) {
        state->target_temperature = APP_MAX_TEMP;
    }
    state->mode = APP_CLIMATE_MODE_AUTO;
    state->fan_speed = 1U;
    state->turbo = 0U;
    state->swing_lr = 0U;
    state->swing_ud = 0U;
    state->lamp_on = 1U;
    state->manual_sleep = 0U;
    state->manual_energy_saving = 0U;
    state->auto_sleep = 0U;
    state->auto_energy_saving = 0U;
    state->light = APP_LIGHT_BRIGHT;
    state->presence = APP_PRESENCE_ABSENT;
    state->absence_ms = 0U;
    state->reason = APP_REASON_POWER_OFF;
    derive_preferences(state);
}

static void update_light(app_climate_state_t *state, uint16_t als)
{
    if (als <= APP_LIGHT_DARK_ENTER) {
        state->light = APP_LIGHT_DARK;
    } else if (als >= APP_LIGHT_BRIGHT_EXIT) {
        state->light = APP_LIGHT_BRIGHT;
    }
}

static uint8_t fresh(uint8_t valid, uint32_t updated_ms,
                     uint32_t now_ms, uint32_t lifetime_ms)
{
    /* A successful sample at tick zero is valid. Unsigned subtraction also
       handles a single 32-bit tick wrap; validity is never encoded in time. */
    return (uint8_t)(valid && (uint32_t)(now_ms - updated_ms) < lifetime_ms);
}

static void update_presence(app_climate_state_t *state,
                            const app_climate_inputs_t *inputs,
                            uint32_t now_ms, uint32_t elapsed_ms)
{
    uint8_t present;
    uint8_t known;
    uint8_t confirmed_present;
    uint8_t just_confirmed = 0U;
    state->auto_sleep = 0U;
    state->auto_energy_saving = 0U;
    state->people_confirming = 0U;
    if (!state->people_fresh) {
        state->presence = APP_PRESENCE_UNKNOWN;
        state->people_candidate_valid = 0U;
        state->absence_ms = 0U;
        return;
    }
    present = inputs->people_count > 0;
    known = state->presence != APP_PRESENCE_UNKNOWN;
    confirmed_present = state->presence != APP_PRESENCE_ABSENT;
    if (!known || present != confirmed_present) {
        if (!state->people_candidate_valid ||
            state->people_candidate_present != present) {
            state->people_candidate_valid = 1U;
            state->people_candidate_present = present;
            state->people_candidate_since_ms = now_ms;
        }
        if ((uint32_t)(now_ms - state->people_candidate_since_ms) >= APP_PEOPLE_CONFIRM_MS) {
            state->presence = present ? APP_PRESENCE_PRESENT : APP_PRESENCE_ABSENT;
            state->people_candidate_valid = 0U;
            just_confirmed = 1U;
        } else {
            state->people_confirming = 1U;
        }
    } else {
        state->people_candidate_valid = 0U;
    }
    /* Only confirmed changes affect the policy. A pending positive result
       pauses absence, so a single noisy frame neither clears Eco nor shuts
       down the system while a returning person is being confirmed. */
    if (state->presence != APP_PRESENCE_ABSENT ||
        state->mode == APP_CLIMATE_MODE_MANUAL_COOL) {
        state->absence_ms = 0U;
    } else if (state->presence == APP_PRESENCE_ABSENT) {
        if (!just_confirmed && !(state->people_confirming && present)) {
            add_absence(state, elapsed_ms);
        }
        state->auto_energy_saving = state->absence_ms >= APP_ABSENCE_ECO_MS;
        if (state->absence_ms >= APP_ABSENCE_OFF_MS && state->power_on) {
            state->power_on = 0U;
            state->automatic_off = 1U;
        }
    }
    if (state->presence != APP_PRESENCE_ABSENT &&
        state->presence != APP_PRESENCE_UNKNOWN) {
        state->auto_sleep = (uint8_t)(state->mode == APP_CLIMATE_MODE_AUTO &&
                                      state->als_fresh && state->light == APP_LIGHT_DARK);
        state->presence = state->auto_sleep ? APP_PRESENCE_SLEEP : APP_PRESENCE_PRESENT;
    }
}

static void update_cooling(app_climate_state_t *state,
                           const app_climate_inputs_t *inputs, uint32_t now_ms)
{
    uint8_t was_cooling = state->cooling_request;
    uint32_t off_elapsed;
    if (!state->cooling_timer_started || state->cooling_stop_pending) {
        state->cooling_off_since_ms = now_ms;
        state->cooling_timer_started = 1U;
        state->cooling_stop_pending = 0U;
    }
    state->cooling_wait_ms = 0U;
    if (!state->power_on || !state->sensor_fresh) {
        state->thermostat_demand = 0U;
        state->cooling_request = 0U;
    } else {
        if (inputs->temperature >= (float)state->effective_target_temperature + 1.0f) {
            state->thermostat_demand = 1U;
        } else if (inputs->temperature <= (float)state->effective_target_temperature - 1.0f) {
            state->thermostat_demand = 0U;
        }
        if (!state->thermostat_demand) {
            state->cooling_request = 0U;
        } else if (!was_cooling) {
            off_elapsed = now_ms - state->cooling_off_since_ms;
            if (off_elapsed < APP_COOLING_MIN_OFF_MS) {
                state->cooling_request = 0U;
                state->cooling_wait_ms = APP_COOLING_MIN_OFF_MS - off_elapsed;
            } else {
                state->cooling_request = 1U;
            }
        }
    }
    /* Only the requested-output closing edge starts another off interval.
       Repeated OFF/invalid steps must not restart the countdown forever. */
    if (was_cooling && !state->cooling_request) {
        state->cooling_off_since_ms = now_ms;
    }
    if (!state->power_on) {
        state->reason = state->automatic_off ? APP_REASON_ABSENCE_OFF : APP_REASON_POWER_OFF;
    } else if (!state->sensor_fresh) {
        state->reason = APP_REASON_SENSOR_INVALID;
    } else if (state->cooling_wait_ms) {
        state->reason = APP_REASON_COOLING_WAIT;
    } else if (state->cooling_request) {
        state->reason = APP_REASON_COOLING;
    } else if (state->mode == APP_CLIMATE_MODE_AUTO && !state->people_fresh) {
        state->reason = APP_REASON_AI_UNKNOWN;
    } else if (state->mode == APP_CLIMATE_MODE_AUTO && state->people_confirming) {
        state->reason = APP_REASON_CONFIRMING;
    } else {
        state->reason = APP_REASON_TEMP_SATISFIED;
    }
}

void app_climate_step(app_climate_state_t *state,
                      const app_climate_inputs_t *inputs, uint32_t now_ms)
{
    const app_climate_inputs_t unknown = {0};
    uint32_t elapsed_ms = 0U;
    if (state == 0) { return; }
    if (inputs == 0) { inputs = &unknown; }
    if (state->step_initialized) {
        elapsed_ms = now_ms - state->step_updated_ms;
        /* A new fresh report after a control gap does not prove occupancy
           throughout that gap. Restart confirmation and continuous absence. */
        if (elapsed_ms >= APP_AI_STALE_MS) {
            state->presence = APP_PRESENCE_UNKNOWN;
            state->people_candidate_valid = 0U;
            state->absence_ms = 0U;
            elapsed_ms = 0U;
        }
    } else {
        state->step_initialized = 1U;
        state->presence = APP_PRESENCE_UNKNOWN;
    }
    state->step_updated_ms = now_ms;
    state->people_fresh = fresh(inputs->people_valid, inputs->people_updated_ms,
                                now_ms, APP_AI_STALE_MS) && inputs->people_count >= 0;
    state->als_fresh = fresh(inputs->als_valid, inputs->als_updated_ms,
                             now_ms, APP_ALS_STALE_MS);
    state->temperature_quality = !inputs->sensor_valid ? APP_DATA_MISSING :
        !isfinite(inputs->temperature) ? APP_DATA_INVALID :
        !fresh(inputs->sensor_valid, inputs->sensor_updated_ms, now_ms,
               APP_SENSOR_STALE_MS) ? APP_DATA_STALE : APP_DATA_FRESH;
    state->humidity_quality = !inputs->humidity_valid ? APP_DATA_MISSING :
        (!isfinite(inputs->humidity) || inputs->humidity < 0.0f ||
         inputs->humidity > 100.0f) ? APP_DATA_INVALID :
        !fresh(inputs->humidity_valid, inputs->humidity_updated_ms, now_ms,
               APP_SENSOR_STALE_MS) ? APP_DATA_STALE : APP_DATA_FRESH;
    state->sensor_fresh = state->temperature_quality == APP_DATA_FRESH;
    state->humidity_fresh = state->humidity_quality == APP_DATA_FRESH;
    if (state->als_fresh) { update_light(state, inputs->als); }
    update_presence(state, inputs, now_ms, elapsed_ms);
    derive_preferences(state);
    update_cooling(state, inputs, now_ms);
}

void app_climate_apply_action(app_climate_state_t *state,
                              app_climate_action_t action)
{
    if (state == 0) {
        return;
    }
    switch (action) {
    case APP_ACTION_POWER:
        state->power_on = (uint8_t)!state->power_on;
        state->absence_ms = 0U;
        state->auto_energy_saving = 0U;
        state->automatic_off = 0U;
        if (!state->power_on) {
            if (state->cooling_request) { state->cooling_stop_pending = 1U; }
            state->cooling_request = 0U;
            state->thermostat_demand = 0U;
            state->cooling_wait_ms = 0U;
            state->reason = APP_REASON_POWER_OFF;
        }
        break;
    case APP_ACTION_MODE_NEXT:
        state->mode = (uint8_t)((state->mode + 1U) % 2U);
        state->absence_ms = 0U;
        state->auto_energy_saving = 0U;
        state->auto_sleep = 0U;
        break;
    case APP_ACTION_TEMP_DOWN:
        if (state->target_temperature > APP_MIN_TEMP) { state->target_temperature--; }
        break;
    case APP_ACTION_TEMP_UP:
        if (state->target_temperature < APP_MAX_TEMP) { state->target_temperature++; }
        break;
    case APP_ACTION_FAN_1: case APP_ACTION_FAN_2: case APP_ACTION_FAN_3:
    case APP_ACTION_FAN_4: case APP_ACTION_FAN_5:
        state->fan_speed = (uint8_t)(action - APP_ACTION_FAN_1 + 1U);
        state->turbo = 0U;
        break;
    case APP_ACTION_TURBO: state->turbo = (uint8_t)!state->turbo; break;
    case APP_ACTION_SWING_LR: state->swing_lr = (uint8_t)!state->swing_lr; break;
    case APP_ACTION_SWING_UD: state->swing_ud = (uint8_t)!state->swing_ud; break;
    case APP_ACTION_LAMP: state->lamp_on = (uint8_t)!state->lamp_on; break;
    case APP_ACTION_SLEEP_TOGGLE: state->manual_sleep = (uint8_t)!state->manual_sleep; break;
    case APP_ACTION_ECO_TOGGLE: state->manual_energy_saving = (uint8_t)!state->manual_energy_saving; break;
    case APP_ACTION_FAN_NEXT:
        state->fan_speed = (uint8_t)(state->fan_speed % 5U + 1U);
        state->turbo = 0U;
        break;
    default: break;
    }
    derive_preferences(state);
}

uint8_t app_climate_output_mask(const app_climate_state_t *state)
{
    uint8_t mask = 0U;
    if (state == 0 || !state->power_on) { return 0U; }
    if (state->sensor_fresh && state->cooling_request) { mask |= 0x01U; }
    if (state->effective_fan_speed) { mask |= 0x02U; }
    if (state->swing_lr || state->swing_ud) { mask |= 0x04U; }
    if (state->lamp_on) { mask |= 0x08U; }
    return mask;
}

const char *app_climate_reason_name(app_climate_reason_t reason)
{
    switch (reason) {
    case APP_REASON_POWER_OFF: return "Power off";
    case APP_REASON_AI_UNKNOWN: return "AI data unknown";
    case APP_REASON_CONFIRMING: return "Confirming presence";
    case APP_REASON_SENSOR_INVALID: return "Temperature data invalid";
    case APP_REASON_TEMP_SATISFIED: return "Temperature satisfied";
    case APP_REASON_COOLING_WAIT: return "Cooling restart wait";
    case APP_REASON_COOLING: return "Cooling requested";
    case APP_REASON_ABSENCE_OFF: return "Absence shutdown";
    default: return "Unknown reason";
    }
}

app_climate_action_t app_climate_remote_action(uint8_t key_code)
{
    switch (key_code) {
    case APP_REMOTE_KEY_POWER: return APP_ACTION_POWER;
    case APP_REMOTE_KEY_TEMP_UP: return APP_ACTION_TEMP_UP;
    case APP_REMOTE_KEY_TEMP_DOWN: return APP_ACTION_TEMP_DOWN;
    case APP_REMOTE_KEY_FAN_1: return APP_ACTION_FAN_1;
    case APP_REMOTE_KEY_FAN_2: return APP_ACTION_FAN_2;
    case APP_REMOTE_KEY_FAN_3: return APP_ACTION_FAN_3;
    case APP_REMOTE_KEY_FAN_4: return APP_ACTION_FAN_4;
    case APP_REMOTE_KEY_FAN_5: return APP_ACTION_FAN_5;
    case APP_REMOTE_KEY_TURBO: return APP_ACTION_TURBO;
    case APP_REMOTE_KEY_SWING_LR: return APP_ACTION_SWING_LR;
    case APP_REMOTE_KEY_SWING_UD: return APP_ACTION_SWING_UD;
    case APP_REMOTE_KEY_LAMP: return APP_ACTION_LAMP;
    case APP_REMOTE_KEY_SLEEP: return APP_ACTION_SLEEP_TOGGLE;
    case APP_REMOTE_KEY_ECO: return APP_ACTION_ECO_TOGGLE;
    case APP_REMOTE_KEY_MODE: return APP_ACTION_MODE_NEXT;
    default: return APP_ACTION_NONE;
    }
}
