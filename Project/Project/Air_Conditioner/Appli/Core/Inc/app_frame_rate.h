#ifndef APP_FRAME_RATE_H
#define APP_FRAME_RATE_H

#include <stdint.h>

#define APP_FRAME_RATE_WINDOW_MS (1000U)

/* Thread-owned estimator. The producer supplies an atomic ISR snapshot. */
typedef struct
{
    uint32_t window_frame_count;
    uint32_t window_started_ms;
    uint32_t rate_x10;
    uint8_t valid;
    uint8_t window_started;
} app_frame_rate_t;

static inline void app_frame_rate_sample(app_frame_rate_t *state,
                                         uint32_t frame_count,
                                         uint32_t last_frame_ms,
                                         uint8_t frame_seen,
                                         uint32_t now_ms)
{
    uint32_t elapsed_ms;
    uint32_t completed_frames;
    uint64_t rate_x10;

    if (frame_seen == 0U)
    {
        state->rate_x10 = 0U;
        state->valid = 0U;
        state->window_started = 0U;
        return;
    }

    /* A known stopped stream is zero FPS. Restart its window on recovery,
       excluding old frames and idle time from the next measurement. */
    if ((uint32_t)(now_ms - last_frame_ms) >= APP_FRAME_RATE_WINDOW_MS)
    {
        state->rate_x10 = 0U;
        state->valid = 1U;
        state->window_started = 0U;
        return;
    }

    if (state->window_started == 0U)
    {
        state->window_frame_count = frame_count;
        state->window_started_ms = now_ms;
        state->window_started = 1U;
        return;
    }

    elapsed_ms = (uint32_t)(now_ms - state->window_started_ms);
    if (elapsed_ms < APP_FRAME_RATE_WINDOW_MS)
    {
        return;
    }

    completed_frames = (uint32_t)(frame_count - state->window_frame_count);
    rate_x10 = ((uint64_t)completed_frames * 10000U) / elapsed_ms;
    state->rate_x10 = (rate_x10 > UINT32_MAX) ? UINT32_MAX : (uint32_t)rate_x10;
    state->valid = 1U;
    state->window_frame_count = frame_count;
    state->window_started_ms = now_ms;
}

#endif /* APP_FRAME_RATE_H */
