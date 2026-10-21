#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "app_frame_rate.h"

static void test_startup_and_regular_stream(void)
{
    app_frame_rate_t state = {0};
    uint32_t now_ms;

    app_frame_rate_sample(&state, 0U, 0U, 0U, 5000U);
    assert(state.valid == 0U && state.rate_x10 == 0U);
    /* A first frame at HAL tick zero must not be mistaken for no frame. */
    app_frame_rate_sample(&state, 1U, 0U, 1U, 0U);
    assert(state.valid == 0U);
    for (now_ms = 10U; now_ms <= 1000U; now_ms += 10U)
    {
        uint32_t new_frames = (now_ms * 30U) / 1000U;
        uint32_t last_frame_ms = (new_frames * 1000U) / 30U;
        app_frame_rate_sample(&state, 1U + new_frames, last_frame_ms, 1U, now_ms);
        if (now_ms < 1000U)
        {
            assert(state.valid == 0U);
        }
    }
    assert(state.valid == 1U && state.rate_x10 == 300U);
}

static void test_stop_and_recovery(void)
{
    app_frame_rate_t state = {0};

    app_frame_rate_sample(&state, 1U, 0U, 1U, 0U);
    app_frame_rate_sample(&state, 31U, 1000U, 1U, 1000U);
    assert(state.rate_x10 == 300U);
    app_frame_rate_sample(&state, 31U, 1000U, 1U, 1999U);
    assert(state.rate_x10 == 300U);
    app_frame_rate_sample(&state, 31U, 1000U, 1U, 2000U);
    assert(state.valid == 1U && state.rate_x10 == 0U);
    app_frame_rate_sample(&state, 31U, 1000U, 1U, 4000U);
    assert(state.rate_x10 == 0U);
    app_frame_rate_sample(&state, 32U, 4050U, 1U, 4050U);
    assert(state.valid == 1U && state.rate_x10 == 0U);
    app_frame_rate_sample(&state, 61U, 5017U, 1U, 5049U);
    assert(state.rate_x10 == 0U);
    app_frame_rate_sample(&state, 62U, 5050U, 1U, 5050U);
    assert(state.valid == 1U && state.rate_x10 == 300U);
}

static void test_irregular_sampling_and_wraparound(void)
{
    app_frame_rate_t state = {0};

    app_frame_rate_sample(&state, 1U, 100U, 1U, 100U);
    app_frame_rate_sample(&state, 38U, 1335U, 1U, 1335U);
    assert(state.rate_x10 == 299U); /* 37 frames / 1.235 seconds */
    state = (app_frame_rate_t){0};
    app_frame_rate_sample(&state, UINT32_MAX - 9U, UINT32_MAX - 499U,
                          1U, UINT32_MAX - 499U);
    app_frame_rate_sample(&state, 20U, 500U, 1U, 500U);
    assert(state.valid == 1U && state.rate_x10 == 300U);
    app_frame_rate_sample(&state, 20U, 500U, 1U, 1500U);
    assert(state.rate_x10 == 0U);
}

static void test_wide_intermediate(void)
{
    app_frame_rate_t state = {0};

    app_frame_rate_sample(&state, 1U, 0U, 1U, 0U);
    app_frame_rate_sample(&state, 500001U, 1000U, 1U, 1000U);
    assert(state.rate_x10 == 5000000U); /* numerator exceeds uint32_t */
    state = (app_frame_rate_t){0};
    app_frame_rate_sample(&state, 0U, 0U, 1U, 0U);
    app_frame_rate_sample(&state, UINT32_MAX, 1000U, 1U, 1000U);
    assert(state.rate_x10 == UINT32_MAX);
}

int main(void)
{
    test_startup_and_regular_stream();
    test_stop_and_recovery();
    test_irregular_sampling_and_wraparound();
    test_wide_intermediate();
    puts("PASS: frame rate startup, 30 FPS, stop/recovery, timing/count wrap, wide arithmetic");
    return 0;
}
