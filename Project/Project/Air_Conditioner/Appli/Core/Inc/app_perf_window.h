#ifndef APP_PERF_WINDOW_H
#define APP_PERF_WINDOW_H

#include <stdint.h>

/* A sample-count window: the latest 64 observations, not a time interval. */
#define APP_PERF_WINDOW_CAPACITY (64U)

typedef struct
{
    uint32_t samples_us[APP_PERF_WINDOW_CAPACITY];
    uint64_t sum_us;
    uint32_t last_us;
    uint8_t count;
    uint8_t next;
} app_perf_window_t;

typedef struct
{
    /* Ten times the mean. A 64-bit field preserves large valid samples. */
    uint64_t mean_x10_us;
    uint32_t last_us;
    uint32_t p95_us;
    uint32_t max_us;
    uint8_t count;
    uint8_t valid;
} app_perf_stats_t;

/* Thread/ISR synchronization is the caller's responsibility. Reset and
   record are O(1). To avoid sorting while holding a shared lock, copy the
   window under that lock and snapshot the private copy after releasing it. */
void app_perf_window_reset(app_perf_window_t *window);
void app_perf_window_record(app_perf_window_t *window, uint32_t value_us);
void app_perf_window_snapshot(const app_perf_window_t *window,
                              app_perf_stats_t *stats);

/* Unsigned tick subtraction allows one 32-bit millisecond tick rollover.
   The elapsed interval must be less than the full tick-counter period. */
uint32_t app_perf_elapsed_ms(uint32_t started_ms, uint32_t completed_ms);

/* Coarse millisecond observations remain usable through the microsecond
   interface. Very large durations saturate rather than silently wrap. */
uint32_t app_perf_ms_to_us(uint32_t elapsed_ms);

/* Use a short cycle-counter interval only when its converted duration agrees
   with the independent millisecond interval within 2 ms. This guards stopped
   or mis-scaled counters; otherwise return the saturating tick conversion.
   elapsed_cycles is an unsigned difference, already computed by the port. */
uint32_t app_perf_duration_us(uint32_t elapsed_cycles,
                              uint32_t elapsed_ms,
                              uint32_t cycles_per_us,
                              uint8_t cycle_clock_valid);

#endif /* APP_PERF_WINDOW_H */
