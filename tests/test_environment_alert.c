#include "app_environment_alert.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

_Static_assert(sizeof(app_environment_alert_t) <= 96U,
               "Environment reminder must remain bounded and allocation-free");
_Static_assert(sizeof(app_environment_view_t) <= 128U,
               "Environment view must fit the display thread's stack budget");

static unsigned assertions;
static unsigned groups;

#define REQUIRE(condition) do { \
    ++assertions; \
    if (!(condition)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
        exit(1); \
    } \
} while (0)

static app_environment_input_t input(int16_t temperature, uint16_t humidity)
{
    app_environment_input_t value = {0};
    value.temperature_x10 = temperature;
    value.humidity_x10 = humidity;
    value.temperature_valid = 1U;
    value.humidity_valid = 1U;
    value.source = APP_ENV_SOURCE_BOARD;
    return value;
}

static app_environment_view_t view(const app_environment_alert_t *state,
                                   uint32_t now_ms)
{
    app_environment_view_t result;
    memset(&result, 0x5a, sizeof(result));
    app_environment_get_view(state, now_ms, &result);
    return result;
}

static uint8_t temperature_events(app_environment_events_t events)
{
    return APP_ENV_EVENTS_FOR_CHANNEL(events, APP_ENV_CHANNEL_TEMPERATURE);
}

static uint8_t humidity_events(app_environment_events_t events)
{
    return APP_ENV_EVENTS_FOR_CHANNEL(events, APP_ENV_CHANNEL_HUMIDITY);
}

static void test_defaults_and_bounds(void)
{
    app_environment_alert_t state;
    app_environment_config_t config;
    app_environment_view_t result;
    ++groups;
    app_environment_init(&state, NULL, APP_ENV_SOURCE_BOARD);
    result = view(&state, 0U);
    REQUIRE(result.config.temperature_high_x10 == 300);
    REQUIRE(result.config.humidity_high == 70U);
    REQUIRE(result.config.hold_ms == 120000U);
    REQUIRE(result.config.snooze_ms == 300000U);
    REQUIRE(result.channels[0].status == APP_ENV_UNKNOWN);
    REQUIRE(result.channels[1].status == APP_ENV_UNKNOWN);
    app_environment_config_defaults(&config);
    config.temperature_high_x10 = -300;
    config.humidity_high = 0U;
    config.hold_ms = 0U;
    config.snooze_ms = 0U;
    REQUIRE(app_environment_set_config(&state, &config, 0U) != 0U);
    REQUIRE(state.config.temperature_high_x10 == 160);
    REQUIRE(state.config.humidity_high == 30U);
    REQUIRE(state.config.hold_ms == APP_ENV_HOLD_MIN_MS);
    REQUIRE(state.config.snooze_ms == 60000U);
    config.temperature_high_x10 = 2000;
    config.humidity_high = 255U;
    config.hold_ms = UINT32_MAX;
    config.snooze_ms = UINT32_MAX;
    app_environment_set_config(&state, &config, 1U);
    REQUIRE(state.config.temperature_high_x10 == 400);
    REQUIRE(state.config.humidity_high == 95U);
    REQUIRE(state.config.hold_ms == 1800000U);
    REQUIRE(state.config.snooze_ms == 1800000U);
    REQUIRE(app_environment_set_config(&state, &config, 2U) == 0U);
    app_environment_init(&state, &state.config, APP_ENV_SOURCE_SIMULATED);
    REQUIRE(state.config.temperature_high_x10 == 400);
    REQUIRE(state.config.humidity_high == 95U);
    REQUIRE(state.config.hold_ms == 1800000U);
    REQUIRE(state.source == APP_ENV_SOURCE_SIMULATED);
}

static void test_brief_breach_and_boundary(void)
{
    app_environment_alert_t state;
    app_environment_input_t sample = input(299, 699U);
    app_environment_view_t result;
    app_environment_events_t events;
    ++groups;
    app_environment_init(&state, NULL, APP_ENV_SOURCE_BOARD);
    REQUIRE(app_environment_update(&state, &sample, 0U) == 0U);
    result = view(&state, 0U);
    REQUIRE(result.channels[0].status == APP_ENV_NORMAL);
    REQUIRE(result.channels[1].status == APP_ENV_NORMAL);
    sample.temperature_x10 = 300;
    sample.humidity_x10 = 700U;
    REQUIRE(app_environment_update(&state, &sample, 1000U) == 0U);
    REQUIRE(app_environment_update(&state, &sample, 120999U) == 0U);
    result = view(&state, 120999U);
    REQUIRE(result.channels[0].pending_ms == 119999U);
    REQUIRE(result.channels[0].status == APP_ENV_PENDING);
    sample.temperature_x10 = 299;
    sample.humidity_x10 = 699U;
    REQUIRE(app_environment_update(&state, &sample, 121000U) == 0U);
    REQUIRE(view(&state, 121000U).channels[0].status == APP_ENV_NORMAL);
    sample.temperature_x10 = 300;
    sample.humidity_x10 = 700U;
    REQUIRE(app_environment_update(&state, &sample, 200000U) == 0U);
    REQUIRE(app_environment_update(&state, &sample, 319999U) == 0U);
    events = app_environment_update(&state, &sample, 320000U);
    REQUIRE(temperature_events(events) == APP_ENV_EVENT_RAISED);
    REQUIRE(humidity_events(events) == APP_ENV_EVENT_RAISED);
    REQUIRE(app_environment_update(&state, &sample, 321000U) == 0U);
    result = view(&state, 321000U);
    REQUIRE(result.channels[0].status == APP_ENV_ACTIVE);
    REQUIRE(result.channels[0].active_ms == 1000U);
    REQUIRE(result.channels[1].status == APP_ENV_ACTIVE);
}

static void test_recovery_hysteresis(void)
{
    app_environment_alert_t state;
    app_environment_input_t sample = input(310, 800U);
    app_environment_events_t events;
    ++groups;
    app_environment_init(&state, NULL, APP_ENV_SOURCE_BOARD);
    app_environment_update(&state, &sample, 0U);
    app_environment_update(&state, &sample, 120000U);
    sample.temperature_x10 = 291;
    sample.humidity_x10 = 671U;
    REQUIRE(app_environment_update(&state, &sample, 121000U) == 0U);
    REQUIRE(view(&state, 121000U).channels[0].raised == 1U);
    REQUIRE(view(&state, 121000U).channels[1].raised == 1U);
    sample.temperature_x10 = 290;
    sample.humidity_x10 = 670U;
    events = app_environment_update(&state, &sample, 122000U);
    REQUIRE(temperature_events(events) == APP_ENV_EVENT_RECOVERED);
    REQUIRE(humidity_events(events) == APP_ENV_EVENT_RECOVERED);
    REQUIRE(view(&state, 122000U).channels[0].status == APP_ENV_NORMAL);
    REQUIRE(app_environment_update(&state, &sample, 123000U) == 0U);
}

static void test_invalid_cache_and_continuity(void)
{
    app_environment_alert_t state;
    app_environment_input_t sample = input(310, 750U);
    app_environment_events_t events;
    app_environment_view_t result;
    ++groups;
    app_environment_init(&state, NULL, APP_ENV_SOURCE_BOARD);
    app_environment_update(&state, &sample, 0U);
    app_environment_update(&state, &sample, 119999U);
    sample.temperature_valid = 0U;
    events = app_environment_update(&state, &sample, 120000U);
    REQUIRE(temperature_events(events) == APP_ENV_EVENT_UNKNOWN);
    REQUIRE(humidity_events(events) == APP_ENV_EVENT_RAISED);
    REQUIRE(view(&state, 120000U).channels[0].status == APP_ENV_UNKNOWN);
    REQUIRE(view(&state, 120000U).channels[0].pending_ms == 0U);
    REQUIRE(app_environment_update(&state, &sample, 900000U) == 0U);
    sample.temperature_valid = 1U;
    REQUIRE(app_environment_update(&state, &sample, 900001U) == 0U);
    REQUIRE(view(&state, 900001U).channels[0].pending_ms == 0U);
    REQUIRE(app_environment_update(&state, &sample, 1020000U) == 0U);
    REQUIRE(temperature_events(app_environment_update(&state, &sample,
                                                       1020001U)) ==
            APP_ENV_EVENT_RAISED);
    sample.temperature_x10 = 100; /* Invalid cached low value cannot recover. */
    sample.temperature_valid = 0U;
    events = app_environment_update(&state, &sample, 1020002U);
    REQUIRE(temperature_events(events) == APP_ENV_EVENT_UNKNOWN);
    result = view(&state, 1020002U);
    REQUIRE(result.channels[0].status == APP_ENV_UNKNOWN);
    REQUIRE(result.channels[0].raised == 1U);
    REQUIRE(result.channels[0].value_x10 == 310);
    REQUIRE(app_environment_update(&state, &sample, 1020003U) == 0U);
    sample.temperature_valid = 1U;
    REQUIRE(temperature_events(app_environment_update(&state, &sample,
                                                       1020004U)) ==
            APP_ENV_EVENT_RECOVERED);
}

static void test_independent_sensor_quality(void)
{
    app_environment_alert_t state;
    app_environment_input_t sample = input(310, 800U);
    app_environment_events_t events;
    ++groups;
    app_environment_init(&state, NULL, APP_ENV_SOURCE_BOARD);
    sample.temperature_valid = 0U;
    app_environment_update(&state, &sample, 100U);
    events = app_environment_update(&state, &sample, 120100U);
    REQUIRE(temperature_events(events) == 0U);
    REQUIRE(humidity_events(events) == APP_ENV_EVENT_RAISED);
    REQUIRE(view(&state, 120100U).channels[0].status == APP_ENV_UNKNOWN);
    sample.temperature_valid = 1U;
    sample.humidity_valid = 0U;
    events = app_environment_update(&state, &sample, 130000U);
    REQUIRE(humidity_events(events) == APP_ENV_EVENT_UNKNOWN);
    REQUIRE(view(&state, 130000U).channels[0].status == APP_ENV_PENDING);
    events = app_environment_update(&state, &sample, 250000U);
    REQUIRE(temperature_events(events) == APP_ENV_EVENT_RAISED);
    REQUIRE(humidity_events(events) == 0U);
    sample.temperature_x10 = INT16_MAX;
    sample.humidity_x10 = UINT16_MAX;
    sample.humidity_valid = 1U;
    events = app_environment_update(&state, &sample, 250001U);
    REQUIRE(temperature_events(events) == APP_ENV_EVENT_UNKNOWN);
    REQUIRE(view(&state, 250001U).channels[1].status == APP_ENV_UNKNOWN);
}

static void test_acknowledgement_and_recurrence(void)
{
    app_environment_alert_t state;
    app_environment_input_t sample = input(310, 750U);
    app_environment_events_t events;
    ++groups;
    app_environment_init(&state, NULL, APP_ENV_SOURCE_BOARD);
    REQUIRE(app_environment_ack(&state, APP_ENV_CHANNEL_MASK_ALL, 0U) == 0U);
    app_environment_update(&state, &sample, 1U);
    REQUIRE(app_environment_ack(&state, APP_ENV_CHANNEL_MASK_ALL, 2U) == 0U);
    app_environment_update(&state, &sample, 120001U);
    events = app_environment_ack(&state, APP_ENV_CHANNEL_MASK_TEMPERATURE,
                                 120002U);
    REQUIRE(temperature_events(events) == APP_ENV_EVENT_ACK);
    REQUIRE(humidity_events(events) == 0U);
    REQUIRE(view(&state, 120002U).channels[0].status == APP_ENV_ACKNOWLEDGED);
    REQUIRE(view(&state, 120002U).channels[1].status == APP_ENV_ACTIVE);
    REQUIRE(app_environment_ack(&state, APP_ENV_CHANNEL_MASK_TEMPERATURE,
                                 120003U) == 0U);
    REQUIRE(app_environment_update(&state, &sample, 900000U) == 0U);
    sample.temperature_x10 = 290;
    REQUIRE(temperature_events(app_environment_update(&state, &sample,
                                                       900001U)) ==
            APP_ENV_EVENT_RECOVERED);
    sample.temperature_x10 = 310;
    app_environment_update(&state, &sample, 900002U);
    REQUIRE(temperature_events(app_environment_update(&state, &sample,
                                                       1020002U)) ==
            APP_ENV_EVENT_RAISED);
    REQUIRE(view(&state, 1020002U).channels[0].acknowledged == 0U);
}

static void test_snooze_expiry_and_recovery(void)
{
    app_environment_alert_t state;
    app_environment_input_t sample = input(310, 750U);
    app_environment_events_t events;
    app_environment_view_t result;
    ++groups;
    app_environment_init(&state, NULL, APP_ENV_SOURCE_BOARD);
    app_environment_update(&state, &sample, 0U);
    app_environment_update(&state, &sample, 120000U);
    events = app_environment_snooze(&state, APP_ENV_CHANNEL_MASK_ALL, 120001U);
    REQUIRE(temperature_events(events) == APP_ENV_EVENT_SNOOZE);
    REQUIRE(humidity_events(events) == APP_ENV_EVENT_SNOOZE);
    result = view(&state, 120001U);
    REQUIRE(result.channels[0].status == APP_ENV_SNOOZED);
    REQUIRE(result.channels[0].snooze_remaining_ms == 300000U);
    REQUIRE(view(&state, 420000U).channels[0].snooze_remaining_ms == 1U);
    REQUIRE(view(&state, 420001U).channels[0].status == APP_ENV_ACTIVE);
    /* An already-active reminder resumes; it must not emit a second RAISED. */
    REQUIRE(app_environment_update(&state, &sample, 420001U) == 0U);
    REQUIRE(view(&state, 420001U).channels[0].pending_ms == 0U);
    app_environment_snooze(&state, APP_ENV_CHANNEL_MASK_ALL, 420002U);
    sample.temperature_x10 = 290;
    events = app_environment_update(&state, &sample, 420003U);
    REQUIRE(temperature_events(events) == APP_ENV_EVENT_RECOVERED);
    REQUIRE(view(&state, 420003U).channels[0].snooze_remaining_ms == 0U);
    sample.temperature_x10 = 310;
    REQUIRE(app_environment_update(&state, &sample, 500000U) == 0U);
    REQUIRE(view(&state, 500000U).channels[0].pending_ms == 0U);
    REQUIRE(temperature_events(app_environment_update(&state, &sample,
                                                       620000U)) ==
            APP_ENV_EVENT_RAISED);
    REQUIRE(view(&state, 620000U).channels[0].status == APP_ENV_ACTIVE);
}

static void test_unknown_active_and_snooze(void)
{
    app_environment_alert_t state;
    app_environment_input_t sample = input(310, 750U);
    app_environment_events_t events;
    ++groups;
    app_environment_init(&state, NULL, APP_ENV_SOURCE_BOARD);
    app_environment_update(&state, &sample, 0U);
    app_environment_update(&state, &sample, 120000U);
    app_environment_snooze(&state, APP_ENV_CHANNEL_MASK_ALL, 120001U);
    sample.temperature_valid = 0U;
    sample.humidity_valid = 0U;
    app_environment_update(&state, &sample, 120002U);
    REQUIRE(view(&state, 420001U).channels[0].status == APP_ENV_UNKNOWN);
    REQUIRE(view(&state, 420001U).channels[0].raised == 1U);
    REQUIRE(app_environment_update(&state, &sample, 420001U) == 0U);
    sample.temperature_valid = 1U;
    sample.humidity_valid = 1U;
    REQUIRE(app_environment_update(&state, &sample, 420002U) == 0U);
    REQUIRE(view(&state, 420002U).channels[0].status == APP_ENV_ACTIVE);
    sample.temperature_valid = 0U;
    app_environment_update(&state, &sample, 420003U);
    events = app_environment_ack(&state, APP_ENV_CHANNEL_MASK_TEMPERATURE,
                                 420004U);
    REQUIRE(temperature_events(events) == APP_ENV_EVENT_ACK);
    REQUIRE(view(&state, 420004U).channels[0].status == APP_ENV_UNKNOWN);
    sample.temperature_valid = 1U;
    REQUIRE(app_environment_update(&state, &sample, 420005U) == 0U);
    REQUIRE(view(&state, 420005U).channels[0].status == APP_ENV_ACKNOWLEDGED);
}

static void test_configuration_reset(void)
{
    app_environment_alert_t state;
    app_environment_config_t config;
    app_environment_input_t sample = input(350, 800U);
    app_environment_events_t events;
    ++groups;
    app_environment_init(&state, NULL, APP_ENV_SOURCE_BOARD);
    app_environment_update(&state, &sample, 0U);
    app_environment_update(&state, &sample, 119999U);
    config = state.config;
    config.temperature_high_x10 = 320;
    events = app_environment_set_config(&state, &config, 120000U);
    REQUIRE(temperature_events(events) == APP_ENV_EVENT_CONFIG_RESET);
    REQUIRE(humidity_events(events) == APP_ENV_EVENT_CONFIG_RESET);
    REQUIRE(view(&state, 120000U).channels[0].status == APP_ENV_UNKNOWN);
    REQUIRE(app_environment_update(&state, &sample, 120001U) == 0U);
    REQUIRE(view(&state, 120001U).channels[0].pending_ms == 0U);
    REQUIRE(app_environment_update(&state, &sample, 240000U) == 0U);
    REQUIRE(temperature_events(app_environment_update(&state, &sample,
                                                       240001U)) ==
            APP_ENV_EVENT_RAISED);
    config.temperature_high_x10 = 360;
    events = app_environment_set_config(&state, &config, 240002U);
    REQUIRE((temperature_events(events) & APP_ENV_EVENT_RECOVERED) == 0U);
    REQUIRE(view(&state, 240002U).channels[0].raised == 0U);
    REQUIRE(app_environment_set_config(&state, &config, 240003U) == 0U);
    app_environment_update(&state, &sample, 240004U);
    REQUIRE(view(&state, 240004U).channels[0].status == APP_ENV_NORMAL);
}

static void test_source_sessions(void)
{
    app_environment_alert_t state;
    app_environment_input_t sample = input(350, 800U);
    app_environment_events_t events;
    ++groups;
    app_environment_init(&state, NULL, APP_ENV_SOURCE_BOARD);
    app_environment_update(&state, &sample, 0U);
    app_environment_update(&state, &sample, 120000U);
    sample.source = APP_ENV_SOURCE_SIMULATED;
    events = app_environment_update(&state, &sample, 120001U);
    REQUIRE(temperature_events(events) == APP_ENV_EVENT_SOURCE_RESET);
    REQUIRE(humidity_events(events) == APP_ENV_EVENT_SOURCE_RESET);
    REQUIRE(view(&state, 120001U).source == APP_ENV_SOURCE_SIMULATED);
    REQUIRE(view(&state, 120001U).channels[0].status == APP_ENV_PENDING);
    REQUIRE(view(&state, 120001U).channels[0].raised == 0U);
    REQUIRE(app_environment_update(&state, &sample, 240000U) == 0U);
    app_environment_update(&state, &sample, 240001U);
    sample.source = APP_ENV_SOURCE_DEMO;
    REQUIRE(temperature_events(app_environment_update(&state, &sample,
                                                       240002U)) ==
            APP_ENV_EVENT_SOURCE_RESET);
    sample.source = APP_ENV_SOURCE_BOARD;
    sample.temperature_valid = 0U;
    sample.humidity_valid = 0U;
    events = app_environment_update(&state, &sample, 240003U);
    REQUIRE((temperature_events(events) & APP_ENV_EVENT_RECOVERED) == 0U);
    REQUIRE(view(&state, 240003U).channels[0].status == APP_ENV_UNKNOWN);
    REQUIRE(view(&state, 240003U).channels[0].raised == 0U);
}

static void test_clock_wrap_and_backwards(void)
{
    app_environment_alert_t state;
    app_environment_input_t sample = input(350, 800U);
    app_environment_events_t events;
    uint32_t start = UINT32_MAX - 1000U;
    ++groups;
    app_environment_init(&state, NULL, APP_ENV_SOURCE_BOARD);
    REQUIRE(app_environment_update(&state, &sample, start) == 0U);
    REQUIRE(app_environment_update(&state, &sample, start + 119999U) == 0U);
    events = app_environment_update(&state, &sample, start + 120000U);
    REQUIRE(temperature_events(events) == APP_ENV_EVENT_RAISED);
    REQUIRE(humidity_events(events) == APP_ENV_EVENT_RAISED);
    app_environment_snooze(&state, APP_ENV_CHANNEL_MASK_ALL, UINT32_MAX - 100U);
    /* The previous update is far earlier: this is a backward jump, not wrap. */
    REQUIRE(view(&state, UINT32_MAX - 100U).channels[0].raised == 0U);
    app_environment_update(&state, &sample, UINT32_MAX - 100U);
    app_environment_update(&state, &sample, (UINT32_MAX - 100U) + 120000U);
    app_environment_snooze(&state, APP_ENV_CHANNEL_MASK_ALL, 120000U);
    REQUIRE(view(&state, 419999U).channels[0].snooze_remaining_ms == 1U);
    REQUIRE(view(&state, 420000U).channels[0].status == APP_ENV_ACTIVE);
    events = app_environment_update(&state, &sample, 110000U);
    REQUIRE(temperature_events(events) == APP_ENV_EVENT_CONFIG_RESET);
    REQUIRE(view(&state, 110000U).channels[0].status == APP_ENV_PENDING);
    REQUIRE(view(&state, 110000U).channels[0].pending_ms == 0U);
}

static void test_fast_hold_override_and_null_contract(void)
{
    app_environment_alert_t state;
    app_environment_config_t config;
    app_environment_input_t sample = input(350, 800U);
    app_environment_view_t result;
    ++groups;
    app_environment_config_defaults(&config);
    config.hold_ms = APP_ENV_HOLD_MIN_MS;
    app_environment_init(&state, &config, APP_ENV_SOURCE_BOARD);
    app_environment_update(&state, &sample, 0U);
    REQUIRE(app_environment_update(&state, &sample, APP_ENV_HOLD_MIN_MS - 1U)
            == 0U);
    REQUIRE(temperature_events(app_environment_update(&state, &sample,
                                                       APP_ENV_HOLD_MIN_MS))
            == APP_ENV_EVENT_RAISED);
    REQUIRE(app_environment_update(NULL, &sample, 0U) == 0U);
    REQUIRE(app_environment_update(&state, NULL, 0U) == 0U);
    REQUIRE(app_environment_set_config(&state, NULL, 0U) == 0U);
    REQUIRE(app_environment_ack(NULL, 3U, 0U) == 0U);
    REQUIRE(app_environment_snooze(NULL, 3U, 0U) == 0U);
    app_environment_init(NULL, NULL, 0U);
    app_environment_config_defaults(NULL);
    app_environment_get_view(NULL, 0U, &result);
    app_environment_get_view(&state, 0U, NULL);
}

static void test_invalid_input_property(void)
{
    app_environment_alert_t state;
    app_environment_input_t sample = input(350, 800U);
    uint32_t now_ms = 120001U;
    uint32_t random = 0x84532031U;
    unsigned index;
    ++groups;
    app_environment_init(&state, NULL, APP_ENV_SOURCE_BOARD);
    app_environment_update(&state, &sample, 0U);
    app_environment_update(&state, &sample, 120000U);
    sample.temperature_valid = 0U;
    sample.humidity_valid = 0U;
    for (index = 0U; index < 10000U; ++index)
    {
        app_environment_events_t events;
        app_environment_view_t result;
        random = random * 1664525U + 1013904223U;
        now_ms += (random & 1023U) + 1U;
        sample.temperature_x10 = (int16_t)(random & 0xffffU);
        sample.humidity_x10 = (uint16_t)(random >> 16U);
        events = app_environment_update(&state, &sample, now_ms);
        REQUIRE((temperature_events(events) &
                 (APP_ENV_EVENT_RAISED | APP_ENV_EVENT_RECOVERED)) == 0U);
        REQUIRE((humidity_events(events) &
                 (APP_ENV_EVENT_RAISED | APP_ENV_EVENT_RECOVERED)) == 0U);
        result = view(&state, now_ms);
        REQUIRE(result.channels[0].status == APP_ENV_UNKNOWN);
        REQUIRE(result.channels[1].status == APP_ENV_UNKNOWN);
        REQUIRE(result.channels[0].raised == 1U);
        REQUIRE(result.channels[1].raised == 1U);
        REQUIRE(result.channels[0].value_x10 == 350);
        REQUIRE(result.channels[1].value_x10 == 800);
    }
}

int main(void)
{
    test_defaults_and_bounds();
    test_brief_breach_and_boundary();
    test_recovery_hysteresis();
    test_invalid_cache_and_continuity();
    test_independent_sensor_quality();
    test_acknowledgement_and_recurrence();
    test_snooze_expiry_and_recovery();
    test_unknown_active_and_snooze();
    test_configuration_reset();
    test_source_sessions();
    test_clock_wrap_and_backwards();
    test_fast_hold_override_and_null_contract();
    test_invalid_input_property();
    printf("PASS environment alerts: %u groups, %u assertions; state=%u view=%u bytes; hold_min=%lu ms\n",
           groups, assertions, (unsigned)sizeof(app_environment_alert_t),
           (unsigned)sizeof(app_environment_view_t),
           (unsigned long)APP_ENV_HOLD_MIN_MS);
    return 0;
}
