#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "app_climate_logic.h"
#include "app_ui.h"

_Static_assert(APP_ACTION_FAN_1 == 5 && APP_ACTION_ECO_TOGGLE == 15,
               "Existing remote action values must remain unchanged");

static app_climate_inputs_t sample(uint32_t now, int32_t people);
static void advance_fresh(app_climate_state_t *state, app_climate_inputs_t *inputs,
                           uint32_t *now, uint32_t duration);

static void test_light_hysteresis(void)
{
    app_climate_state_t state;
    app_climate_inputs_t inputs = sample(0U, 1);
    app_climate_init(&state, 26U);
    inputs.als = 50U;
    app_climate_step(&state, &inputs, 0U);
    assert(state.light == APP_LIGHT_DARK);
    inputs.als = 250U;
    app_climate_step(&state, &inputs, 500U);
    assert(state.light == APP_LIGHT_BRIGHT);
    inputs.als = 150U;
    app_climate_step(&state, &inputs, 1000U);
    assert(state.light == APP_LIGHT_BRIGHT);
}

static void test_occupancy_policy_and_shutdown(void)
{
    app_climate_state_t state;
    app_climate_inputs_t inputs = sample(0U, 0);
    uint32_t now = 0U;
    app_climate_init(&state, 26U);
    app_climate_apply_action(&state, APP_ACTION_POWER);
    assert(state.power_on != 0U);
    app_climate_step(&state, &inputs, now);
    advance_fresh(&state, &inputs, &now, APP_PEOPLE_CONFIRM_MS);
    assert(state.presence == APP_PRESENCE_ABSENT);
    assert(state.auto_energy_saving == 0U);
    advance_fresh(&state, &inputs, &now, APP_ABSENCE_ECO_MS - 1U);
    assert(state.auto_energy_saving == 0U);
    advance_fresh(&state, &inputs, &now, 1U);
    assert(state.auto_energy_saving != 0U);
    advance_fresh(&state, &inputs, &now,
                  APP_ABSENCE_OFF_MS - APP_ABSENCE_ECO_MS - 1U);
    assert(state.power_on != 0U);
    advance_fresh(&state, &inputs, &now, 1U);
    assert(state.power_on == 0U);
    inputs.people_count = 1;
    advance_fresh(&state, &inputs, &now, APP_PEOPLE_CONFIRM_MS + 500U);
    assert(state.presence == APP_PRESENCE_PRESENT);
    assert(state.auto_energy_saving == 0U);
}

static void test_dark_person_sleep(void)
{
    app_climate_state_t state;
    app_climate_inputs_t inputs = sample(0U, 1);
    uint32_t now = 0U;
    app_climate_init(&state, 26U);
    inputs.als = 50U;
    app_climate_step(&state, &inputs, now);
    advance_fresh(&state, &inputs, &now, APP_PEOPLE_CONFIRM_MS);
    assert(state.presence == APP_PRESENCE_SLEEP);
    assert(state.auto_sleep != 0U);
    inputs.als = 250U;
    advance_fresh(&state, &inputs, &now, 500U);
    assert(state.presence == APP_PRESENCE_PRESENT);
    assert(state.auto_sleep == 0U);
}

static void test_remote_actions(void)
{
    app_climate_state_t state;
    app_climate_init(&state, 26U);
    app_climate_apply_action(&state, APP_ACTION_SLEEP_TOGGLE);
    assert(state.manual_sleep != 0U);
    app_climate_apply_action(&state, APP_ACTION_ECO_TOGGLE);
    assert(state.manual_energy_saving != 0U);
    assert(app_climate_remote_action(69U) == APP_ACTION_POWER);
    assert(app_climate_remote_action(9U) == APP_ACTION_TEMP_UP);
    assert(app_climate_remote_action(7U) == APP_ACTION_TEMP_DOWN);
    assert(app_climate_remote_action(22U) == APP_ACTION_FAN_1);
    assert(app_climate_remote_action(94U) == APP_ACTION_TURBO);
    assert(app_climate_remote_action(67U) == APP_ACTION_SWING_LR);
    assert(app_climate_remote_action(21U) == APP_ACTION_SWING_UD);
    assert(app_climate_remote_action(90U) == APP_ACTION_LAMP);
    assert(app_climate_remote_action(66U) == APP_ACTION_SLEEP_TOGGLE);
    assert(app_climate_remote_action(71U) == APP_ACTION_ECO_TOGGLE);
    assert(app_climate_remote_action(64U) == APP_ACTION_MODE_NEXT);
}

static app_climate_inputs_t sample(uint32_t now, int32_t people)
{
    app_climate_inputs_t inputs = {0};
    inputs.people_count = people;
    inputs.people_valid = 1U;
    inputs.people_updated_ms = now;
    inputs.als = 250U;
    inputs.als_valid = 1U;
    inputs.als_updated_ms = now;
    inputs.temperature = 27.0f;
    inputs.humidity = 55.0f;
    inputs.sensor_valid = 1U;
    inputs.sensor_updated_ms = now;
    inputs.humidity_valid = 1U;
    inputs.humidity_updated_ms = now;
    return inputs;
}

static void refresh(app_climate_inputs_t *inputs, uint32_t now)
{
    inputs->people_updated_ms = now;
    inputs->als_updated_ms = now;
    inputs->sensor_updated_ms = now;
    inputs->humidity_updated_ms = now;
}

static void advance_fresh(app_climate_state_t *state, app_climate_inputs_t *inputs,
                           uint32_t *now, uint32_t duration)
{
    /* Model regular incoming observations, not one new report after a gap. */
    while (duration != 0U) {
        uint32_t step = duration > 500U ? 500U : duration;
        *now += step;
        refresh(inputs, *now);
        app_climate_step(state, inputs, *now);
        duration -= step;
    }
}

static void test_freshness_and_zero_timestamp(void)
{
    app_climate_state_t state;
    app_climate_inputs_t inputs = sample(0U, 1);
    inputs.temperature = 26.0f;
    app_climate_init(&state, 26U);
    app_climate_apply_action(&state, APP_ACTION_POWER);
    app_climate_step(&state, &inputs, 0U);
    assert(state.people_fresh && state.als_fresh && state.sensor_fresh);
    assert(state.presence == APP_PRESENCE_UNKNOWN && state.people_confirming);
    assert(state.reason == APP_REASON_CONFIRMING);
    app_climate_step(&state, &inputs, 1000U);
    assert(state.presence == APP_PRESENCE_PRESENT);
    app_climate_step(&state, &inputs, APP_AI_STALE_MS - 1U);
    assert(state.people_fresh && state.als_fresh);
    app_climate_step(&state, &inputs, APP_AI_STALE_MS);
    assert(!state.people_fresh && !state.als_fresh && state.sensor_fresh);
    assert(state.presence == APP_PRESENCE_UNKNOWN && state.absence_ms == 0U);
    assert(state.reason == APP_REASON_AI_UNKNOWN && state.power_on);
    app_climate_step(&state, &inputs, APP_SENSOR_STALE_MS);
    assert(!state.sensor_fresh && state.reason == APP_REASON_SENSOR_INVALID);
    inputs = sample(APP_SENSOR_STALE_MS + 500U, -1);
    app_climate_step(&state, &inputs, APP_SENSOR_STALE_MS + 500U);
    assert(!state.people_fresh && state.presence == APP_PRESENCE_UNKNOWN);
    app_climate_step(&state, NULL, APP_SENSOR_STALE_MS + 1000U);
    assert(!state.people_fresh && !state.als_fresh && !state.sensor_fresh);
    assert(!state.cooling_request && state.power_on);
}

static void test_presence_confirmation_and_light_failure(void)
{
    app_climate_state_t state;
    app_climate_inputs_t inputs = sample(0U, 1);
    uint32_t now = 0U;
    app_climate_init(&state, 26U);
    app_climate_step(&state, &inputs, now);
    advance_fresh(&state, &inputs, &now, APP_PEOPLE_CONFIRM_MS - 1U);
    assert(state.presence == APP_PRESENCE_UNKNOWN);
    advance_fresh(&state, &inputs, &now, 1U);
    assert(state.presence == APP_PRESENCE_PRESENT && !state.people_confirming);
    inputs.people_count = 0;
    advance_fresh(&state, &inputs, &now, 500U);
    assert(state.presence == APP_PRESENCE_PRESENT && state.people_confirming);
    inputs.people_count = 1;
    advance_fresh(&state, &inputs, &now, 500U);
    assert(state.presence == APP_PRESENCE_PRESENT && !state.people_confirming);
    assert(state.absence_ms == 0U);
    inputs.als = 50U;
    advance_fresh(&state, &inputs, &now, 500U);
    assert(state.presence == APP_PRESENCE_SLEEP && state.effective_sleep);
    inputs.als_valid = 0U;
    advance_fresh(&state, &inputs, &now, 500U);
    assert(!state.als_fresh && !state.auto_sleep && !state.effective_sleep);
    assert(state.presence == APP_PRESENCE_PRESENT);
    inputs.als_valid = 1U;
    inputs.als_updated_ms = now - APP_ALS_STALE_MS;
    app_climate_step(&state, &inputs, now);
    assert(!state.als_fresh && !state.auto_sleep);
}

static void test_new_absence_policy_and_manual_restart(void)
{
    app_climate_state_t state;
    app_climate_inputs_t inputs = sample(0U, 0);
    uint32_t now = 0U;
    app_climate_init(&state, 26U);
    app_climate_apply_action(&state, APP_ACTION_POWER);
    app_climate_step(&state, &inputs, now);
    advance_fresh(&state, &inputs, &now, APP_PEOPLE_CONFIRM_MS);
    assert(state.presence == APP_PRESENCE_ABSENT && state.absence_ms == 0U);
    advance_fresh(&state, &inputs, &now, APP_ABSENCE_ECO_MS - 1U);
    assert(!state.auto_energy_saving && state.power_on);
    advance_fresh(&state, &inputs, &now, 1U);
    assert(state.auto_energy_saving && state.effective_eco);
    assert(state.effective_target_temperature == 28U);
    /* One positive frame must not flicker the confirmed presence or Eco. */
    inputs.people_count = 1;
    advance_fresh(&state, &inputs, &now, 500U);
    assert(state.people_confirming && state.presence == APP_PRESENCE_ABSENT);
    assert(state.absence_ms == APP_ABSENCE_ECO_MS && state.effective_eco);
    inputs.people_count = 0;
    advance_fresh(&state, &inputs, &now, 500U);
    advance_fresh(&state, &inputs, &now, APP_ABSENCE_OFF_MS - state.absence_ms - 1U);
    assert(state.power_on);
    advance_fresh(&state, &inputs, &now, 1U);
    assert(!state.power_on && state.reason == APP_REASON_ABSENCE_OFF);
    assert(app_climate_output_mask(&state) == 0U);
    app_climate_apply_action(&state, APP_ACTION_POWER);
    assert(state.power_on && state.absence_ms == 0U && !state.automatic_off);
    advance_fresh(&state, &inputs, &now, 500U);
    assert(state.power_on && state.absence_ms == 500U);
    inputs.people_valid = 0U;
    advance_fresh(&state, &inputs, &now, 500U);
    assert(state.presence == APP_PRESENCE_UNKNOWN && state.absence_ms == 0U);
    assert(!state.auto_energy_saving && state.power_on);
}

static void test_long_gap_restarts_confirmation(void)
{
    app_climate_state_t state;
    app_climate_inputs_t inputs = sample(0U, 0);
    uint32_t now = 0U;
    app_climate_init(&state, 26U);
    app_climate_apply_action(&state, APP_ACTION_POWER);
    app_climate_step(&state, &inputs, now);
    advance_fresh(&state, &inputs, &now, APP_PEOPLE_CONFIRM_MS + 5000U);
    assert(state.absence_ms == 5000U);
    now += APP_ABSENCE_OFF_MS;
    refresh(&inputs, now); /* Fresh now does not establish earlier occupancy. */
    app_climate_step(&state, &inputs, now);
    assert(state.people_fresh && state.people_confirming);
    assert(state.presence == APP_PRESENCE_UNKNOWN && state.absence_ms == 0U);
    assert(state.power_on && !state.auto_energy_saving);
    advance_fresh(&state, &inputs, &now, APP_PEOPLE_CONFIRM_MS);
    assert(state.presence == APP_PRESENCE_ABSENT && state.absence_ms == 0U);
    advance_fresh(&state, &inputs, &now, 500U);
    assert(state.absence_ms == 500U);
    now += APP_AI_STALE_MS; /* Exact timeout boundary also breaks the chain. */
    refresh(&inputs, now);
    app_climate_step(&state, &inputs, now);
    assert(state.people_confirming && state.absence_ms == 0U);
}

static void test_manual_mode_and_effective_preferences(void)
{
    app_climate_state_t state;
    app_climate_inputs_t inputs = sample(0U, 0);
    uint32_t now = 0U;
    app_climate_init(&state, 26U);
    app_climate_apply_action(&state, APP_ACTION_POWER);
    app_climate_apply_action(&state, APP_ACTION_MODE_NEXT);
    assert(state.mode == APP_CLIMATE_MODE_MANUAL_COOL);
    app_climate_step(&state, &inputs, now);
    advance_fresh(&state, &inputs, &now, APP_ABSENCE_OFF_MS + APP_PEOPLE_CONFIRM_MS);
    assert(state.power_on && !state.auto_energy_saving && state.absence_ms == 0U);
    inputs.people_count = 1;
    inputs.als = 50U;
    advance_fresh(&state, &inputs, &now, APP_PEOPLE_CONFIRM_MS + 500U);
    assert(state.presence == APP_PRESENCE_PRESENT && !state.auto_sleep);
    app_climate_apply_action(&state, APP_ACTION_FAN_5);
    app_climate_apply_action(&state, APP_ACTION_TURBO);
    assert(state.effective_fan_speed == 5U);
    app_climate_apply_action(&state, APP_ACTION_ECO_TOGGLE);
    assert(state.effective_eco && state.effective_target_temperature == 28U);
    assert(state.effective_fan_speed == 2U);
    app_climate_apply_action(&state, APP_ACTION_SLEEP_TOGGLE);
    assert(state.effective_sleep && state.effective_target_temperature == 28U);
    assert(state.effective_fan_speed == 1U);
    app_climate_apply_action(&state, APP_ACTION_SLEEP_TOGGLE);
    app_climate_apply_action(&state, APP_ACTION_ECO_TOGGLE);
    assert(state.effective_fan_speed == 5U && state.effective_target_temperature == 26U);
    app_climate_apply_action(&state, APP_ACTION_MODE_NEXT);
    assert(state.mode == APP_CLIMATE_MODE_AUTO && state.absence_ms == 0U);
    advance_fresh(&state, &inputs, &now, 500U);
    assert(state.auto_sleep && state.effective_sleep);
}

static void test_fan_cycle_and_temperature_limits(void)
{
    app_climate_state_t state;
    app_climate_init(&state, 0U);
    assert(state.target_temperature == 16U);
    app_climate_apply_action(&state, APP_ACTION_TEMP_DOWN);
    assert(state.target_temperature == 16U);
    for (unsigned int i = 0U; i < 20U; i++) {
        app_climate_apply_action(&state, APP_ACTION_TEMP_UP);
    }
    assert(state.target_temperature == 30U);
    app_climate_apply_action(&state, APP_ACTION_ECO_TOGGLE);
    assert(state.effective_target_temperature == 30U);
    app_climate_apply_action(&state, APP_ACTION_ECO_TOGGLE);
    app_climate_apply_action(&state, APP_ACTION_TURBO);
    assert(state.effective_fan_speed == 5U);
    for (uint8_t expected = 2U; expected <= 5U; expected++) {
        app_climate_apply_action(&state, APP_ACTION_FAN_NEXT);
        assert(state.fan_speed == expected && state.effective_fan_speed == expected);
        assert(!state.turbo);
    }
    app_climate_apply_action(&state, APP_ACTION_FAN_NEXT);
    assert(state.fan_speed == 1U && state.effective_fan_speed == 1U);
}

static void test_hysteresis_and_restart_protection(void)
{
    app_climate_state_t state;
    app_climate_inputs_t inputs = sample(0U, 1);
    uint32_t now = 0U;
    app_climate_init(&state, 26U);
    app_climate_apply_action(&state, APP_ACTION_POWER);
    app_climate_step(&state, &inputs, now);
    assert(!state.cooling_request && state.cooling_wait_ms == APP_COOLING_MIN_OFF_MS);
    assert(state.reason == APP_REASON_COOLING_WAIT);
    advance_fresh(&state, &inputs, &now, APP_COOLING_MIN_OFF_MS - 1U);
    assert(!state.cooling_request && state.cooling_wait_ms == 1U);
    advance_fresh(&state, &inputs, &now, 1U);
    assert(state.cooling_request && state.cooling_wait_ms == 0U);
    assert(state.reason == APP_REASON_COOLING);
    inputs.temperature = 26.0f;
    advance_fresh(&state, &inputs, &now, 500U);
    assert(state.cooling_request); /* Hold in the +/- 1 C hysteresis band. */
    inputs.temperature = 25.0f;
    advance_fresh(&state, &inputs, &now, 500U);
    uint32_t stopped = now;
    assert(!state.cooling_request && state.reason == APP_REASON_TEMP_SATISFIED);
    assert(state.cooling_off_since_ms == stopped);
    inputs.temperature = 27.0f;
    advance_fresh(&state, &inputs, &now, 500U);
    assert(!state.cooling_request && state.cooling_wait_ms == APP_COOLING_MIN_OFF_MS - 500U);
    advance_fresh(&state, &inputs, &now, 500U);
    assert(state.cooling_off_since_ms == stopped);
    assert(state.cooling_wait_ms == APP_COOLING_MIN_OFF_MS - 1000U);
    advance_fresh(&state, &inputs, &now, APP_COOLING_MIN_OFF_MS - 1000U);
    assert(state.cooling_request);
    inputs.sensor_valid = 0U;
    advance_fresh(&state, &inputs, &now, 500U);
    stopped = now;
    assert(!state.cooling_request && !state.cooling_wait_ms);
    assert(state.reason == APP_REASON_SENSOR_INVALID);
    assert((app_climate_output_mask(&state) & 1U) == 0U);
    advance_fresh(&state, &inputs, &now, 500U);
    assert(state.cooling_off_since_ms == stopped);
    inputs.sensor_valid = 1U;
    advance_fresh(&state, &inputs, &now, 500U);
    assert(state.cooling_wait_ms == APP_COOLING_MIN_OFF_MS - 1000U);
    advance_fresh(&state, &inputs, &now, APP_COOLING_MIN_OFF_MS - 1000U);
    assert(state.cooling_request);
    app_climate_apply_action(&state, APP_ACTION_POWER);
    assert(!state.cooling_request && app_climate_output_mask(&state) == 0U);
    advance_fresh(&state, &inputs, &now, 500U);
    stopped = now;
    advance_fresh(&state, &inputs, &now, 500U);
    assert(state.cooling_off_since_ms == stopped);
    app_climate_apply_action(&state, APP_ACTION_POWER);
    advance_fresh(&state, &inputs, &now, 500U);
    assert(!state.cooling_request && state.cooling_wait_ms == APP_COOLING_MIN_OFF_MS - 1000U);
}

static void test_invalid_numeric_sensor_data(void)
{
    app_climate_state_t state;
    app_climate_inputs_t inputs = sample(0U, 1);
    app_climate_init(&state, 26U);
    app_climate_apply_action(&state, APP_ACTION_POWER);
    inputs.temperature = NAN;
    app_climate_step(&state, &inputs, 0U);
    assert(!state.sensor_fresh && !state.cooling_request);
    assert(state.temperature_quality == APP_DATA_INVALID && state.humidity_fresh);
    inputs.temperature = 27.0f;
    inputs.humidity = INFINITY;
    refresh(&inputs, 500U);
    app_climate_step(&state, &inputs, 500U);
    assert(state.sensor_fresh && !state.humidity_fresh);
    assert(state.humidity_quality == APP_DATA_INVALID);
    inputs.humidity = 101.0f;
    refresh(&inputs, 1000U);
    app_climate_step(&state, &inputs, 1000U);
    assert(state.sensor_fresh && !state.humidity_fresh);
    inputs.humidity = -1.0f;
    refresh(&inputs, 1500U);
    app_climate_step(&state, &inputs, 1500U);
    assert(state.sensor_fresh && !state.humidity_fresh);
    inputs.humidity_valid = 0U;
    uint32_t now = 1500U;
    advance_fresh(&state, &inputs, &now, APP_COOLING_MIN_OFF_MS);
    assert(state.sensor_fresh && state.cooling_request);
    assert(state.humidity_quality == APP_DATA_MISSING);
    inputs.humidity_valid = 1U;
    inputs.humidity = 55.0f;
    inputs.humidity_updated_ms = now - APP_SENSOR_STALE_MS;
    app_climate_step(&state, &inputs, now);
    assert(state.sensor_fresh && !state.humidity_fresh && state.cooling_request);
    assert(state.humidity_quality == APP_DATA_STALE);
    inputs.sensor_updated_ms = now - APP_SENSOR_STALE_MS;
    app_climate_step(&state, &inputs, now);
    assert(!state.sensor_fresh && !state.cooling_request);
    assert(state.temperature_quality == APP_DATA_STALE);
}

static void test_tick_wrap_and_output_contract(void)
{
    app_climate_state_t state;
    uint32_t now = UINT32_MAX - 500U;
    app_climate_inputs_t inputs = sample(now, 1);
    app_climate_init(&state, 26U);
    app_climate_apply_action(&state, APP_ACTION_POWER);
    app_climate_step(&state, &inputs, now);
    advance_fresh(&state, &inputs, &now, APP_PEOPLE_CONFIRM_MS);
    assert(state.presence == APP_PRESENCE_PRESENT);
    assert(state.people_fresh && state.als_fresh && state.sensor_fresh);
    advance_fresh(&state, &inputs, &now, APP_COOLING_MIN_OFF_MS - APP_PEOPLE_CONFIRM_MS);
    assert(state.cooling_request && state.cooling_wait_ms == 0U);
    assert(app_climate_output_mask(&state) == 0x0BU);
    app_climate_apply_action(&state, APP_ACTION_SWING_LR);
    assert(app_climate_output_mask(&state) == 0x0FU);
    app_climate_apply_action(&state, APP_ACTION_LAMP);
    assert(app_climate_output_mask(&state) == 0x07U);
    app_climate_apply_action(&state, APP_ACTION_POWER);
    assert(app_climate_output_mask(&state) == 0U);
    assert(app_climate_output_mask(NULL) == 0U);
    assert(strcmp(app_climate_reason_name(APP_REASON_COOLING_WAIT), "Cooling restart wait") == 0);
    assert(strcmp(app_climate_reason_name((app_climate_reason_t)99), "Unknown reason") == 0);
}

static void test_snapshot_input_bridge(void)
{
    app_ui_snapshot_t snapshot = {0};
    snapshot.people_count = 1;
    snapshot.people_valid = 1U;
    snapshot.als = 250U;
    snapshot.als_valid = 1U;
    snapshot.temperature = 27.0f;
    snapshot.humidity = 55.0f;
    snapshot.sensor_valid = 1U;
    snapshot.humidity_valid = 1U;
    app_climate_init(&snapshot.climate, 26U);
    app_climate_apply_action(&snapshot.climate, APP_ACTION_POWER);
    app_climate_inputs_t inputs = app_ui_snapshot_inputs(&snapshot);
    app_climate_step(&snapshot.climate, &inputs, 0U);
    assert(snapshot.climate.people_fresh && snapshot.climate.als_fresh && snapshot.climate.sensor_fresh);
    inputs = app_ui_snapshot_inputs(NULL);
    app_climate_step(&snapshot.climate, &inputs, 500U);
    assert(snapshot.climate.presence == APP_PRESENCE_UNKNOWN);
    assert(!snapshot.climate.sensor_fresh && !snapshot.climate.cooling_request);
}

int main(void)
{
    test_light_hysteresis();
    test_occupancy_policy_and_shutdown();
    test_dark_person_sleep();
    test_remote_actions();
    test_freshness_and_zero_timestamp();
    test_presence_confirmation_and_light_failure();
    test_new_absence_policy_and_manual_restart();
    test_long_gap_restarts_confirmation();
    test_manual_mode_and_effective_preferences();
    test_fan_cycle_and_temperature_limits();
    test_hysteresis_and_restart_protection();
    test_invalid_numeric_sensor_data();
    test_tick_wrap_and_output_contract();
    test_snapshot_input_bridge();
    puts("app climate logic tests passed");
    return 0;
}
