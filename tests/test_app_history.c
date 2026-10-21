#include "app_history.h"

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

static app_history_input_t input_default(void)
{
    app_history_input_t input;
    memset(&input, 0, sizeof(input));
    input.temperature_x10 = 271;
    input.humidity_x10 = 550;
    input.als = 250;
    input.people = 1;
    input.target_x10 = 260;
    input.reason = 4;
    input.presence = 2;
    input.valid_flags = 0xFFU;
    input.source = APP_HISTORY_SOURCE_BOARD;
    return input;
}

static void test_first_point_and_interval(void)
{
    app_history_t history;
    app_history_stats_t stats;
    app_history_input_t input = input_default();
    app_history_init(&history);
    assert(app_history_sample_at(&history, 0) == NULL);
    app_history_observe(&history, &input, 1234);
    app_history_get_stats(&history, &stats);
    assert(stats.initialized && stats.session_id == 1);
    assert(stats.sample_count == 1 && stats.event_count == 1);
    assert(stats.elapsed_ms == 0);
    assert(app_history_event_at(&history, 0)->changed_flags ==
           APP_HISTORY_EVENT_SESSION);
    app_history_observe(&history, &input,
                        1234 + APP_HISTORY_SAMPLE_INTERVAL_MS - 1U);
    assert(history.stats.sample_count == 1);
    app_history_observe(&history, &input,
                        1234 + APP_HISTORY_SAMPLE_INTERVAL_MS);
    assert(history.stats.sample_count == 2);
    assert(app_history_sample_at(&history, 1)->elapsed_ms ==
           APP_HISTORY_SAMPLE_INTERVAL_MS);
    assert(history.stats.event_count == 1);
}

static void test_missed_points_not_invented(void)
{
    app_history_t history;
    app_history_input_t input = input_default();
    app_history_init(&history);
    app_history_observe(&history, &input, 0);
    input.temperature_x10 = 283;
    app_history_observe(&history, &input,
                        APP_HISTORY_SAMPLE_INTERVAL_MS * 9U);
    assert(history.stats.sample_count == 2);
    assert(app_history_sample_at(&history, 1)->data.temperature_x10 == 283);
    assert(app_history_sample_at(&history, 1)->elapsed_ms ==
           APP_HISTORY_SAMPLE_INTERVAL_MS * 9U);
    assert(history.stats.request_known_ms == 0);
    assert(history.stats.request_unknown_ms ==
           APP_HISTORY_SAMPLE_INTERVAL_MS * 9U);
}

static void test_bounded_oldest_first_samples(void)
{
    app_history_t history;
    app_history_input_t input = input_default();
    uint32_t n;
    app_history_init(&history);
    for (n = 0; n < APP_HISTORY_SAMPLE_CAPACITY + 17U; ++n)
    {
        input.people = (uint16_t)n;
        app_history_observe(&history, &input,
                            n * APP_HISTORY_SAMPLE_INTERVAL_MS);
    }
    assert(history.stats.sample_count == APP_HISTORY_SAMPLE_CAPACITY);
    for (n = 0; n < APP_HISTORY_SAMPLE_CAPACITY; ++n)
    {
        const app_history_sample_t *sample =
            app_history_sample_at(&history, (uint16_t)n);
        assert(sample->data.people == n + 17U);
        assert(sample->elapsed_ms ==
               (n + 17U) * (uint64_t)APP_HISTORY_SAMPLE_INTERVAL_MS);
    }
    assert(app_history_sample_at(&history, APP_HISTORY_SAMPLE_CAPACITY) == NULL);
}

static void test_missing_values_and_event_quality(void)
{
    app_history_t history;
    app_history_input_t input = input_default();
    const app_history_sample_t *sample;
    const app_history_event_t *event;
    app_history_init(&history);
    app_history_observe(&history, &input, 0);
    input.valid_flags = APP_HISTORY_VALID_TARGET;
    input.temperature_x10 = -123;
    input.humidity_x10 = 999;
    input.als = 987;
    input.people = 321;
    input.request_mask = 15;
    input.reason = 99;
    input.presence = 2;
    app_history_observe(&history, &input, APP_HISTORY_SAMPLE_INTERVAL_MS);
    sample = app_history_sample_at(&history, 1);
    assert(sample->data.valid_flags == APP_HISTORY_VALID_TARGET);
    assert(sample->data.temperature_x10 == 0);
    assert(sample->data.humidity_x10 == 0);
    assert(sample->data.als == 0 && sample->data.people == 0);
    assert(sample->data.target_x10 == 260);
    assert(sample->data.request_mask == 0 && sample->data.reason == 0);
    assert(sample->data.presence == 0);
    event = app_history_event_at(&history, 1);
    assert(event->changed_flags == (APP_HISTORY_EVENT_REASON |
           APP_HISTORY_EVENT_REQUEST | APP_HISTORY_EVENT_PRESENCE |
           APP_HISTORY_EVENT_QUALITY));
    assert(history.stats.request_known_ms == 0);
    assert(history.stats.request_unknown_ms == APP_HISTORY_SAMPLE_INTERVAL_MS);
}

static void test_bounded_events_and_no_numeric_noise(void)
{
    app_history_t history;
    app_history_input_t input = input_default();
    uint32_t n;
    app_history_init(&history);
    app_history_observe(&history, &input, 0);
    /* Numeric fluctuations alone belong in trends, not the event ring. */
    for (n = 1; n < 10; ++n)
    {
        input.temperature_x10++;
        input.humidity_x10++;
        input.als++;
        input.people++;
        input.target_x10++;
        app_history_observe(&history, &input, n);
    }
    assert(history.stats.event_count == 1);
    for (n = 1; n <= APP_HISTORY_EVENT_CAPACITY + 5U; ++n)
    {
        input.reason = (uint8_t)n;
        app_history_observe(&history, &input, 100 + n);
    }
    assert(history.stats.event_count == APP_HISTORY_EVENT_CAPACITY);
    for (n = 0; n < APP_HISTORY_EVENT_CAPACITY; ++n)
    {
        const app_history_event_t *event =
            app_history_event_at(&history, (uint16_t)n);
        assert(event->reason == n + 6U);
        assert(event->changed_flags == APP_HISTORY_EVENT_REASON);
    }
    assert(app_history_event_at(&history, APP_HISTORY_EVENT_CAPACITY) == NULL);
    input.quality = 0xA5A5;
    app_history_observe(&history, &input, 200);
    assert(app_history_event_at(&history,
                               APP_HISTORY_EVENT_CAPACITY - 1)->changed_flags ==
           APP_HISTORY_EVENT_QUALITY);
}

static void test_tick_wrap_and_multiple_wrap_elapsed(void)
{
    app_history_t history;
    app_history_input_t input = input_default();
    uint32_t now = UINT32_MAX - 100U;
    uint32_t n;
    app_history_init(&history);
    app_history_observe(&history, &input, now);
    now += 200U;
    app_history_observe(&history, &input, now);
    assert(history.stats.session_id == 1);
    assert(history.stats.elapsed_ms == 200U);
    assert(history.stats.request_known_ms == 200U);
    /* A lifetime can exceed one tick period without overflowing elapsed time. */
    for (n = 0; n < 6U; ++n)
    {
        now += 0x40000000UL;
        app_history_observe(&history, &input, now);
    }
    assert(history.stats.session_id == 1);
    assert(history.stats.elapsed_ms == 200ULL + 6ULL * 0x40000000UL);
    assert(history.stats.request_unknown_ms == 6ULL * 0x40000000UL);
}

static void test_backward_tick_and_source_change_reset(void)
{
    app_history_t history;
    app_history_input_t input = input_default();
    uint32_t old_revision;
    app_history_init(&history);
    app_history_observe(&history, &input, 1000);
    app_history_observe(&history, &input, 2000);
    old_revision = history.stats.revision;
    app_history_observe(&history, &input, 1999);
    assert(history.stats.session_id == 2);
    assert(history.stats.elapsed_ms == 0);
    assert(history.stats.request_known_ms == 0);
    assert(history.stats.sample_count == 1 && history.stats.event_count == 1);
    assert(history.stats.revision != old_revision);
    input.source = APP_HISTORY_SOURCE_SIMULATED;
    input.request_mask = APP_HISTORY_COOLING_REQUEST;
    app_history_observe(&history, &input, 2999);
    assert(history.stats.session_id == 3);
    assert(history.stats.source == APP_HISTORY_SOURCE_SIMULATED);
    assert(history.stats.elapsed_ms == 0);
    assert(history.stats.cooling_request_starts == 0);
    assert(app_history_sample_at(&history, 0)->data.source ==
           APP_HISTORY_SOURCE_SIMULATED);
    input.source = APP_HISTORY_SOURCE_BOARD;
    app_history_observe(&history, &input, 3999);
    assert(history.stats.session_id == 4);
    assert(history.stats.sample_count == 1);
    assert(app_history_event_at(&history, 0)->source == APP_HISTORY_SOURCE_BOARD);
}

static void test_conservative_request_time_and_starts(void)
{
    app_history_t history;
    app_history_input_t input = input_default();
    app_history_init(&history);
    app_history_observe(&history, &input, 0);
    app_history_observe(&history, &input, 1000);
    assert(history.stats.request_known_ms == 1000);
    input.request_mask = APP_HISTORY_COOLING_REQUEST;
    app_history_observe(&history, &input, 2000);
    assert(history.stats.cooling_request_starts == 1);
    assert(history.stats.cooling_request_ms == 0);
    assert(history.stats.request_unknown_ms == 1000);
    app_history_observe(&history, &input, 3000);
    assert(history.stats.cooling_request_ms == 1000);
    /* Other software request changes cannot erase a known cooling interval. */
    input.request_mask |= 0x0EU;
    app_history_observe(&history, &input, 4000);
    assert(history.stats.cooling_request_ms == 2000);
    input.valid_flags &= (uint8_t)~APP_HISTORY_VALID_REQUEST;
    app_history_observe(&history, &input, 5000);
    input.valid_flags |= APP_HISTORY_VALID_REQUEST;
    app_history_observe(&history, &input, 6000);
    assert(history.stats.cooling_request_starts == 1);
    assert(history.stats.cooling_request_ms == 2000);
    app_history_observe(&history, &input, 16000);
    assert(history.stats.cooling_request_ms == 2000);
    assert(history.stats.request_unknown_ms == 13000);
    app_history_observe(&history, &input, 17000);
    assert(history.stats.cooling_request_ms == 3000);
    input.request_mask = 0;
    app_history_observe(&history, &input, 18000);
    input.request_mask = APP_HISTORY_COOLING_REQUEST;
    app_history_observe(&history, &input, 28000);
    assert(history.stats.cooling_request_starts == 1);
    assert(history.stats.request_known_ms + history.stats.request_unknown_ms ==
           history.stats.elapsed_ms);
}

static void test_request_gap_boundary_and_initial_on(void)
{
    app_history_t history;
    app_history_input_t input = input_default();
    uint32_t now = 0;
    app_history_init(&history);
    input.request_mask = APP_HISTORY_COOLING_REQUEST;
    app_history_observe(&history, &input, now);
    assert(history.stats.cooling_request_starts == 0);
    now += APP_HISTORY_MAX_KNOWN_GAP_MS;
    app_history_observe(&history, &input, now);
    assert(history.stats.cooling_request_ms == APP_HISTORY_MAX_KNOWN_GAP_MS);
    now += APP_HISTORY_MAX_KNOWN_GAP_MS + 1U;
    app_history_observe(&history, &input, now);
    assert(history.stats.cooling_request_ms == APP_HISTORY_MAX_KNOWN_GAP_MS);
    assert(history.stats.request_unknown_ms == APP_HISTORY_MAX_KNOWN_GAP_MS + 1U);
    input.request_mask = 0;
    app_history_observe(&history, &input, now);
    input.request_mask = APP_HISTORY_COOLING_REQUEST;
    app_history_observe(&history, &input, now);
    assert(history.stats.cooling_request_starts == 1);
    history.stats.cooling_request_starts = UINT32_MAX;
    input.request_mask = 0;
    app_history_observe(&history, &input, now);
    input.request_mask = APP_HISTORY_COOLING_REQUEST;
    app_history_observe(&history, &input, now);
    assert(history.stats.cooling_request_starts == UINT32_MAX);
}

static uint32_t random_next(uint32_t *state)
{
    *state = *state * 1664525U + 1013904223U;
    return *state;
}

static void test_randomized_accounting_and_ring_invariants(void)
{
    app_history_t history;
    app_history_input_t input = input_default();
    uint32_t random = 0xBAC00591U;
    uint32_t now = UINT32_MAX - 10000U;
    uint32_t n;
    app_history_init(&history);
    for (n = 0; n < 100000U; ++n)
    {
        uint16_t i;
        uint64_t previous_elapsed = 0;
        uint32_t value = random_next(&random);
        now += value % 10000U;
        input.valid_flags = (uint8_t)value;
        input.request_mask = (uint8_t)(value >> 8) & 0x0FU;
        input.reason = (uint8_t)(value >> 16) & 7U;
        input.presence = (uint8_t)(value >> 24) & 3U;
        input.quality = (uint16_t)(value >> 11);
        app_history_observe(&history, &input, now);
        assert(history.stats.sample_count <= APP_HISTORY_SAMPLE_CAPACITY);
        assert(history.stats.event_count <= APP_HISTORY_EVENT_CAPACITY);
        assert(history.stats.cooling_request_ms <= history.stats.request_known_ms);
        assert(history.stats.request_known_ms + history.stats.request_unknown_ms ==
               history.stats.elapsed_ms);
        for (i = 0; i < history.stats.sample_count; ++i)
        {
            const app_history_sample_t *sample = app_history_sample_at(&history, i);
            assert(sample->elapsed_ms >= previous_elapsed);
            assert(sample->elapsed_ms <= history.stats.elapsed_ms);
            previous_elapsed = sample->elapsed_ms;
        }
    }
    assert(history.stats.session_id == 1);
}

static void test_nulls_and_idempotent_tick(void)
{
    app_history_t history;
    app_history_stats_t stats;
    app_history_input_t input = input_default();
    uint32_t revision;
    app_history_init(NULL);
    app_history_observe(NULL, &input, 0);
    app_history_init(&history);
    app_history_observe(&history, NULL, 0);
    assert(!history.stats.initialized);
    app_history_observe(&history, &input, 0);
    revision = history.stats.revision;
    app_history_observe(&history, &input, 0);
    assert(history.stats.revision == revision);
    assert(history.stats.elapsed_ms == 0);
    assert(app_history_sample_at(NULL, 0) == NULL);
    assert(app_history_event_at(NULL, 0) == NULL);
    app_history_get_stats(NULL, &stats);
    assert(!stats.initialized && !stats.sample_count && !stats.event_count);
    app_history_get_stats(&history, NULL);
}

int main(void)
{
    test_first_point_and_interval();
    test_missed_points_not_invented();
    test_bounded_oldest_first_samples();
    test_missing_values_and_event_quality();
    test_bounded_events_and_no_numeric_noise();
    test_tick_wrap_and_multiple_wrap_elapsed();
    test_backward_tick_and_source_change_reset();
    test_conservative_request_time_and_starts();
    test_request_gap_boundary_and_initial_on();
    test_randomized_accounting_and_ring_invariants();
    test_nulls_and_idempotent_tick();
    puts("PASS: bounded history, events, source sessions and conservative software request statistics (100000 randomized observations)");
    return 0;
}
