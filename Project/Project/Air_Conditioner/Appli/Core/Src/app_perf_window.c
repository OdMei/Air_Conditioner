#include "app_perf_window.h"

#include <stddef.h>
#include <string.h>

void app_perf_window_reset(app_perf_window_t *window)
{
    if (window != NULL)
    {
        memset(window, 0, sizeof(*window));
    }
}

void app_perf_window_record(app_perf_window_t *window, uint32_t value_us)
{
    if (window == NULL)
    {
        return;
    }
    if (window->count == APP_PERF_WINDOW_CAPACITY)
    {
        window->sum_us -= window->samples_us[window->next];
    }
    else
    {
        window->count++;
    }
    window->samples_us[window->next] = value_us;
    window->sum_us += value_us;
    window->last_us = value_us;
    window->next = (uint8_t)((window->next + 1U) % APP_PERF_WINDOW_CAPACITY);
}

void app_perf_window_snapshot(const app_perf_window_t *window,
                              app_perf_stats_t *stats)
{
    uint32_t sorted[APP_PERF_WINDOW_CAPACITY];
    uint32_t index;
    uint32_t rank;

    if (stats == NULL)
    {
        return;
    }
    memset(stats, 0, sizeof(*stats));
    if ((window == NULL) || (window->count == 0U))
    {
        return;
    }

    /* Bounded insertion sort of a private copy; no heap, MCU, or OS calls. */
    for (index = 0U; index < window->count; index++)
    {
        uint32_t value = window->samples_us[index];
        uint32_t position = index;
        while ((position > 0U) && (sorted[position - 1U] > value))
        {
            sorted[position] = sorted[position - 1U];
            position--;
        }
        sorted[position] = value;
    }

    /* Nearest-rank P95: ceil(0.95 * count), with a one-based rank. */
    rank = (95U * window->count + 99U) / 100U;
    stats->last_us = window->last_us;
    stats->mean_x10_us = (window->sum_us * 10U) / window->count;
    stats->p95_us = sorted[rank - 1U];
    stats->max_us = sorted[window->count - 1U];
    stats->count = window->count;
    stats->valid = 1U;
}

uint32_t app_perf_elapsed_ms(uint32_t started_ms, uint32_t completed_ms)
{
    return (uint32_t)(completed_ms - started_ms);
}

uint32_t app_perf_ms_to_us(uint32_t elapsed_ms)
{
    uint64_t converted = (uint64_t)elapsed_ms * 1000U;
    return (converted > UINT32_MAX) ? UINT32_MAX : (uint32_t)converted;
}

uint32_t app_perf_duration_us(uint32_t elapsed_cycles,
                              uint32_t elapsed_ms,
                              uint32_t cycles_per_us,
                              uint8_t cycle_clock_valid)
{
    if ((cycle_clock_valid != 0U) && (cycles_per_us != 0U) &&
        (elapsed_ms < 4000U) &&
        ((elapsed_cycles != 0U) || (elapsed_ms == 0U)))
    {
        uint32_t cycle_us = elapsed_cycles / cycles_per_us;
        uint64_t coarse_us = (uint64_t)elapsed_ms * 1000U;
        if (((uint64_t)cycle_us + 2000U >= coarse_us) &&
            ((uint64_t)cycle_us <= coarse_us + 2000U))
        {
            return cycle_us;
        }
    }
    return app_perf_ms_to_us(elapsed_ms);
}
