#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app_perf_window.h"

static void test_empty_and_null(void)
{
    app_perf_window_t window;
    app_perf_stats_t stats;

    memset(&window, 0xA5, sizeof(window));
    app_perf_window_reset(&window);
    memset(&stats, 0xA5, sizeof(stats));
    app_perf_window_snapshot(&window, &stats);
    assert(stats.count == 0U && stats.valid == 0U);
    assert(stats.last_us == 0U && stats.mean_x10_us == 0U);
    assert(stats.p95_us == 0U && stats.max_us == 0U);
    app_perf_window_reset(NULL);
    app_perf_window_record(NULL, 123U);
    app_perf_window_snapshot(&window, NULL);
    app_perf_window_snapshot(NULL, &stats);
    assert(stats.count == 0U && stats.valid == 0U);
}

static void test_zero_and_single_sample(void)
{
    app_perf_window_t window = {0};
    app_perf_stats_t stats;

    app_perf_window_record(&window, 0U);
    app_perf_window_snapshot(&window, &stats);
    assert(stats.valid == 1U && stats.count == 1U);
    assert(stats.last_us == 0U && stats.mean_x10_us == 0U);
    assert(stats.p95_us == 0U && stats.max_us == 0U);
    app_perf_window_reset(&window);
    app_perf_window_record(&window, 17001U);
    app_perf_window_snapshot(&window, &stats);
    assert(stats.valid == 1U && stats.count == 1U);
    assert(stats.last_us == 17001U && stats.mean_x10_us == 170010U);
    assert(stats.p95_us == 17001U && stats.max_us == 17001U);
}

static void test_percentile_ranks_and_fractional_mean(void)
{
    app_perf_window_t window = {0};
    app_perf_stats_t stats;
    uint32_t value;

    for (value = 1U; value <= 20U; value++)
    {
        app_perf_window_record(&window, value);
    }
    app_perf_window_snapshot(&window, &stats);
    assert(stats.p95_us == 19U && stats.mean_x10_us == 105U);
    app_perf_window_record(&window, 21U);
    app_perf_window_snapshot(&window, &stats);
    assert(stats.p95_us == 20U && stats.mean_x10_us == 110U);
    for (value = 22U; value <= APP_PERF_WINDOW_CAPACITY; value++)
    {
        app_perf_window_record(&window, value);
    }
    app_perf_window_snapshot(&window, &stats);
    assert(stats.count == 64U && stats.p95_us == 61U);
    assert(stats.max_us == 64U && stats.last_us == 64U);
    assert(stats.mean_x10_us == 325U);

    app_perf_window_reset(&window);
    app_perf_window_record(&window, 1U);
    app_perf_window_record(&window, 1U);
    app_perf_window_record(&window, 2U);
    app_perf_window_snapshot(&window, &stats);
    assert(stats.mean_x10_us == 13U); /* Truncate only after wide arithmetic. */
}

static void test_ring_overwrite_and_snapshot_is_read_only(void)
{
    app_perf_window_t window = {0};
    app_perf_window_t before;
    app_perf_stats_t stats;
    uint32_t value;

    for (value = 1U; value <= 65U; value++)
    {
        app_perf_window_record(&window, value);
    }
    before = window;
    app_perf_window_snapshot(&window, &stats);
    assert(memcmp(&before, &window, sizeof(window)) == 0);
    assert(stats.count == 64U && stats.mean_x10_us == 335U);
    assert(stats.p95_us == 62U && stats.last_us == 65U);
    for (value = 0U; value < APP_PERF_WINDOW_CAPACITY; value++)
    {
        app_perf_window_record(&window, 0U);
    }
    app_perf_window_snapshot(&window, &stats);
    assert(stats.count == 64U && stats.valid == 1U);
    assert(stats.mean_x10_us == 0U && stats.max_us == 0U);
    assert(stats.p95_us == 0U && stats.last_us == 0U);
}

static void test_outlier_and_large_values(void)
{
    app_perf_window_t window = {0};
    app_perf_stats_t stats;
    uint32_t index;

    for (index = 0U; index < APP_PERF_WINDOW_CAPACITY - 1U; index++)
    {
        app_perf_window_record(&window, 1000U);
    }
    app_perf_window_record(&window, UINT32_MAX);
    app_perf_window_snapshot(&window, &stats);
    assert(stats.p95_us == 1000U && stats.max_us == UINT32_MAX);
    assert(stats.last_us == UINT32_MAX);
    assert(stats.mean_x10_us == ((UINT64_C(63000) + UINT32_MAX) * 10U) / 64U);
    for (index = 0U; index < 3U; index++)
    {
        app_perf_window_record(&window, UINT32_MAX);
    }
    app_perf_window_snapshot(&window, &stats);
    assert(stats.p95_us == UINT32_MAX); /* Four / 64 exceed the P95 rank. */

    app_perf_window_reset(&window);
    for (index = 0U; index < APP_PERF_WINDOW_CAPACITY * 3U; index++)
    {
        app_perf_window_record(&window, UINT32_MAX);
    }
    app_perf_window_snapshot(&window, &stats);
    assert(window.sum_us == (uint64_t)UINT32_MAX * 64U);
    assert(stats.mean_x10_us == (uint64_t)UINT32_MAX * 10U);
    assert(stats.max_us == UINT32_MAX && stats.p95_us == UINT32_MAX);
}

static void test_elapsed_tick_wrap_and_conversion(void)
{
    assert(app_perf_elapsed_ms(1234U, 1245U) == 11U);
    assert(app_perf_elapsed_ms(0U, 0U) == 0U);
    assert(app_perf_elapsed_ms(UINT32_MAX - 9U, 5U) == 15U);
    assert(app_perf_elapsed_ms(1U, 0U) == UINT32_MAX);
    assert(app_perf_ms_to_us(0U) == 0U);
    assert(app_perf_ms_to_us(33U) == 33000U);
    assert(app_perf_ms_to_us(UINT32_MAX / 1000U) ==
           (UINT32_MAX / 1000U) * 1000U);
    assert(app_perf_ms_to_us(UINT32_MAX / 1000U + 1U) == UINT32_MAX);
    assert(app_perf_ms_to_us(UINT32_MAX) == UINT32_MAX);
    assert(app_perf_ms_to_us(app_perf_elapsed_ms(UINT32_MAX - 9U, 5U)) == 15000U);
}

static void test_cycle_duration_and_tick_fallback(void)
{
    uint32_t cycle_start = UINT32_MAX - 399U;
    uint32_t cycle_end = 1199U;
    uint32_t wrapped_delta = cycle_end - cycle_start;

    assert(app_perf_duration_us(800U * 1234U, 1U, 800U, 1U) == 1234U);
    assert(app_perf_duration_us(800U * 1234U, 1U, 800U, 0U) == 1000U);
    assert(app_perf_duration_us(800U * 1234U, 1U, 0U, 1U) == 1000U);
    assert(app_perf_duration_us(0U, 0U, 800U, 1U) == 0U);
    assert(app_perf_duration_us(0U, 1U, 800U, 1U) == 1000U);
    assert(app_perf_duration_us(0U, 0U, 0U, 1U) == 0U);
    assert(app_perf_duration_us(800U * 25U, 0U, 800U, 1U) == 25U);
    assert(app_perf_duration_us(800U * 999U, 1U, 800U, 1U) == 999U);
    assert(app_perf_duration_us(800U * 3999000U, 3999U, 800U, 1U) == 3999000U);
    assert(app_perf_duration_us(800U * 4000001U, 4000U, 800U, 1U) == 4000000U);

    /* A counter which stopped partway has a nonzero delta. It still needs
       coarse-clock validation instead of passing a nonzero-only check. */
    assert(app_perf_duration_us(800U * 5000U, 10U, 800U, 1U) == 10000U);
    assert(app_perf_duration_us(800U * 15000U, 10U, 800U, 1U) == 10000U);
    assert(app_perf_duration_us(800U * 8000U, 10U, 800U, 1U) == 8000U);
    assert(app_perf_duration_us(800U * 12000U, 10U, 800U, 1U) == 12000U);
    assert(app_perf_duration_us(800U * 7999U, 10U, 800U, 1U) == 10000U);
    assert(app_perf_duration_us(800U * 12001U, 10U, 800U, 1U) == 10000U);
    assert(wrapped_delta == 1599U);
    assert(app_perf_duration_us(wrapped_delta, 0U, 800U, 1U) == 1U);

    assert(app_perf_duration_us(UINT32_MAX, 0U, 800U, 1U) == 0U);
    assert(app_perf_duration_us(UINT32_MAX, UINT32_MAX, 800U, 1U) == UINT32_MAX);
    assert(app_perf_duration_us(0U, UINT32_MAX, 0U, 0U) == UINT32_MAX);
}

static int compare_u32(const void *left, const void *right)
{
    uint32_t a = *(const uint32_t *)left;
    uint32_t b = *(const uint32_t *)right;
    return (a > b) - (a < b);
}

static void test_against_independent_reference(void)
{
    app_perf_window_t window = {0};
    app_perf_stats_t stats;
    uint32_t stream[257U];
    uint32_t seed = 0x13579BDFU;
    uint32_t index;

    for (index = 0U; index < 257U; index++)
    {
        uint32_t sorted[APP_PERF_WINDOW_CAPACITY];
        uint32_t count = index + 1U < APP_PERF_WINDOW_CAPACITY ?
                         index + 1U : APP_PERF_WINDOW_CAPACITY;
        uint32_t first = index + 1U - count;
        uint64_t sum = 0U;
        uint32_t sample;

        seed = seed * 1664525U + 1013904223U;
        stream[index] = seed;
        app_perf_window_record(&window, stream[index]);
        for (sample = 0U; sample < count; sample++)
        {
            sorted[sample] = stream[first + sample];
            sum += sorted[sample];
        }
        qsort(sorted, count, sizeof(sorted[0]), compare_u32);
        app_perf_window_snapshot(&window, &stats);
        assert(stats.count == count && stats.valid == 1U);
        assert(stats.last_us == stream[index]);
        assert(stats.mean_x10_us == (sum * 10U) / count);
        assert(stats.max_us == sorted[count - 1U]);
        assert(stats.p95_us == sorted[(count * 95U + 99U) / 100U - 1U]);
    }
}

int main(void)
{
    test_empty_and_null();
    test_zero_and_single_sample();
    test_percentile_ranks_and_fractional_mean();
    test_ring_overwrite_and_snapshot_is_read_only();
    test_outlier_and_large_values();
    test_elapsed_tick_wrap_and_conversion();
    test_cycle_duration_and_tick_fallback();
    test_against_independent_reference();
    puts("PASS: performance window empty/zero/ranks/ring/outliers/wide arithmetic/tick wrap/cycle fallback/reference");
    return 0;
}
