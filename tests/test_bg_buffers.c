#include "app_bg_buffers.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_first_frame_and_delayed_vblank(void)
{
    app_bg_buffers_t state;
    app_bg_capture_plan_t plan;
    uint8_t candidate;
    uint32_t frame_id;
    uint32_t index;

    assert(app_bg_buffers_init(&state, 3U, 1U, 0U));
    assert(app_bg_buffers_frame_complete(&state, 1U, &plan));
    assert(plan.completed_index == 0U && plan.next_fill == 2U);
    assert(state.front == 1U && state.fill == 0U);
    assert(app_bg_buffers_valid(&state));
    /* Never publish the DMA target before the hardware retarget succeeds. */
    assert(!app_bg_buffers_candidate(&state, &candidate, &frame_id));
    assert(!app_bg_buffers_submission_result(&state, true));
    assert(!app_bg_buffers_capture_target_result(&state, 1U, true));
    assert(app_bg_buffers_capture_target_result(&state, 2U, true));
    assert(state.fill == 2U && state.front == 1U);
    assert(app_bg_buffers_candidate(&state, &candidate, &frame_id));
    assert(candidate == 0U && frame_id == 1U);
    assert(app_bg_buffers_submission_result(&state, true));
    assert(state.pending == 0U);
    for (index = 2U; index <= 100002U; index++)
    {
        assert(!app_bg_buffers_observe_reload(&state, true));
        assert(!app_bg_buffers_frame_complete(&state, index, &plan));
        assert(plan.next_fill == 2U && plan.need_retarget == 0U);
        assert(state.front == 1U && state.pending == 0U);
        assert(app_bg_buffers_valid(&state));
    }
    assert(state.dropped == 100001U);
    /* Only layer-specific hardware completion releases the old front. */
    assert(app_bg_buffers_observe_reload(&state, false));
    assert(state.front == 0U && state.front_frame_id == 1U);
    assert(!app_bg_buffers_observe_reload(&state, false));
    assert(app_bg_buffers_frame_complete(&state, 100003U, &plan));
    assert(plan.completed_index == 2U && plan.next_fill == 1U);
    assert(app_bg_buffers_valid(&state));
}

static void test_failed_target_and_failed_reload(void)
{
    app_bg_buffers_t state;
    app_bg_capture_plan_t plan;
    uint8_t candidate;
    uint32_t frame_id;
    uint32_t index;

    assert(app_bg_buffers_init(&state, 3U, 1U, 0U));
    assert(app_bg_buffers_frame_complete(&state, 20U, &plan));
    assert(app_bg_buffers_capture_target_result(&state, plan.next_fill, false));
    assert(state.fill == 0U && state.front == 1U);
    assert(state.phase == APP_BG_PREPARED_NONE);
    assert(state.capture_target_failures == 1U && state.dropped == 1U);
    assert(app_bg_buffers_frame_complete(&state, 21U, &plan));
    assert(app_bg_buffers_capture_target_result(&state, plan.next_fill, true));
    assert(app_bg_buffers_submission_result(&state, false));
    for (index = 22U; index < 1022U; index++)
    {
        /* Even VBR clear cannot complete a request HAL did not accept. */
        assert(!app_bg_buffers_observe_reload(&state, false));
        assert(!app_bg_buffers_frame_complete(&state, index, &plan));
        assert(plan.next_fill == 2U);
        assert(app_bg_buffers_candidate(&state, &candidate, &frame_id));
        assert(candidate == 0U && frame_id == 21U);
        assert(app_bg_buffers_submission_result(&state, false));
        assert(app_bg_buffers_valid(&state));
    }
    assert(state.front == 1U && state.pending == APP_BG_BUFFER_NONE);
    assert(state.reload_failures == 1001U);
    assert(app_bg_buffers_submission_result(&state, true));
    assert(app_bg_buffers_observe_reload(&state, false));
    assert(state.front == 0U && state.front_frame_id == 21U);
    assert(app_bg_buffers_valid(&state));
}

static void test_probe_pin_and_recovery(void)
{
    app_bg_buffers_t state;
    app_bg_capture_plan_t plan;
    uint8_t probe;
    uint8_t extra_probe;
    uint32_t frame_id;

    assert(app_bg_buffers_init(&state, 3U, 1U, 0U));
    assert(app_bg_buffers_pin_front(&state, &probe, &frame_id));
    assert(probe == 1U && frame_id == 0U);
    assert(!app_bg_buffers_pin_front(&state, &extra_probe, &frame_id));
    assert(app_bg_buffers_frame_complete(&state, 1U, &plan));
    assert(app_bg_buffers_capture_target_result(&state, plan.next_fill, true));
    assert(app_bg_buffers_submission_result(&state, true));
    app_bg_buffers_stop(&state);
    assert(!app_bg_buffers_frame_complete(&state, 2U, &plan));
    assert(state.captures == 1U);
    assert(state.front == 1U && state.pending == 0U && state.fill == 2U);
    assert(app_bg_buffers_restart(&state));
    assert(!app_bg_buffers_observe_reload(&state, true));
    assert(!app_bg_buffers_frame_complete(&state, 3U, &plan));
    assert(app_bg_buffers_observe_reload(&state, false));
    /* Probe holds the old front after reload; all three slots are occupied. */
    assert(!app_bg_buffers_frame_complete(&state, 4U, &plan));
    assert(state.front == 0U && state.fill == 2U && state.probe == 1U);
    assert(!app_bg_buffers_release_probe(&state, 0U));
    assert(app_bg_buffers_release_probe(&state, probe));
    assert(app_bg_buffers_frame_complete(&state, 5U, &plan));
    assert(plan.next_fill == 1U);
    app_bg_buffers_stop(&state); /* stop before any LTDC submission */
    assert(state.phase == APP_BG_PREPARED_NONE && state.fill == 2U);
    assert(app_bg_buffers_restart(&state));
    assert(app_bg_buffers_frame_complete(&state, 6U, &plan));
    assert(app_bg_buffers_capture_target_result(&state, plan.next_fill, true));
    assert(app_bg_buffers_submission_result(&state, false));
    app_bg_buffers_stop(&state); /* failed submission must remain quarantined */
    assert(state.phase == APP_BG_PREPARED_RELOAD && state.prepared == 2U);
    assert(app_bg_buffers_restart(&state));
    assert(!app_bg_buffers_frame_complete(&state, 7U, &plan));
    assert(plan.next_fill == 1U);
    assert(app_bg_buffers_submission_result(&state, true));
    assert(app_bg_buffers_observe_reload(&state, false));
    assert(state.front == 2U && state.fill == 1U);
    assert(app_bg_buffers_valid(&state));
}

typedef struct
{
    app_bg_buffers_t ownership;
    uint8_t actual_front;
    uint8_t actual_fill;
    uint8_t actual_pending;
    uint8_t running;
    uint32_t memory_version[3];
    uint32_t next_frame;
    uint32_t probe_version;
    uint32_t rng;
} hardware_model_t;

static uint32_t next_random(hardware_model_t *hw)
{
    hw->rng ^= hw->rng << 13U;
    hw->rng ^= hw->rng >> 17U;
    hw->rng ^= hw->rng << 5U;
    return hw->rng;
}

static void assert_physical_ownership(const hardware_model_t *hw)
{
    assert(app_bg_buffers_valid(&hw->ownership));
    assert(hw->actual_fill == hw->ownership.fill);
    assert(hw->actual_fill != hw->actual_front);
    assert(hw->actual_fill != hw->actual_pending);
    assert(hw->actual_fill != hw->ownership.probe);
    if (hw->ownership.probe != APP_BG_BUFFER_NONE)
    {
        assert(hw->memory_version[hw->ownership.probe] == hw->probe_version);
    }
}

static void try_hardware_reload(hardware_model_t *hw, uint32_t decision)
{
    uint8_t candidate;
    uint32_t frame_id;
    bool accepted;

    if (!app_bg_buffers_candidate(&hw->ownership, &candidate, &frame_id))
    {
        return;
    }
    assert(candidate != hw->actual_fill);
    assert(hw->memory_version[candidate] == frame_id);
    accepted = (decision & 3U) != 0U;
    /* Also model a conservative HAL failure: the hardware may have armed
     * VBR despite reporting failure. The module must protect both fronts. */
    if (accepted || ((decision & 4U) != 0U))
    {
        hw->actual_pending = candidate;
    }
    assert(app_bg_buffers_submission_result(&hw->ownership, accepted));
}

static void capture_frame(hardware_model_t *hw, uint32_t decision)
{
    app_bg_capture_plan_t plan;
    bool target_accepted;

    if (hw->running == 0U)
    {
        return;
    }
    assert_physical_ownership(hw);
    /* Camera writes before its completion interrupt. This is an independent
     * memory model: pending/front content must remain unchanged. */
    hw->memory_version[hw->actual_fill] = ++hw->next_frame;
    app_bg_buffers_observe_reload(&hw->ownership,
                                  hw->actual_pending != APP_BG_BUFFER_NONE);
    if (app_bg_buffers_frame_complete(&hw->ownership, hw->next_frame, &plan))
    {
        target_accepted = (decision & 15U) != 0U;
        if (target_accepted)
        {
            assert(plan.next_fill != hw->actual_front);
            assert(plan.next_fill != hw->actual_pending);
            assert(plan.next_fill != hw->ownership.probe);
            hw->actual_fill = plan.next_fill;
        }
        assert(app_bg_buffers_capture_target_result(
            &hw->ownership, plan.next_fill, target_accepted));
    }
    try_hardware_reload(hw, decision >> 4U);
}

static void test_random_hardware_interleavings(void)
{
    hardware_model_t hw;
    uint32_t seed;
    uint32_t step;
    uint32_t decision;
    uint8_t probe;
    uint32_t frame_id;

    for (seed = 1U; seed <= 32U; seed++)
    {
        memset(&hw, 0, sizeof(hw));
        hw.actual_front = 1U;
        hw.actual_fill = 0U;
        hw.actual_pending = APP_BG_BUFFER_NONE;
        hw.running = 1U;
        hw.rng = seed * 0x9E3779B9U;
        assert(app_bg_buffers_init(&hw.ownership, 3U, 1U, 0U));
        for (step = 0U; step < 50000U; step++)
        {
            decision = next_random(&hw);
            switch (decision % 8U)
            {
                case 0U:
                case 1U:
                case 2U:
                    capture_frame(&hw, decision >> 3U);
                    break;
                case 3U:
                    if (hw.actual_pending != APP_BG_BUFFER_NONE)
                    {
                        hw.actual_front = hw.actual_pending;
                        hw.actual_pending = APP_BG_BUFFER_NONE;
                    }
                    /* Sometimes delay the software observer across frames. */
                    if ((decision & 256U) != 0U)
                    {
                        app_bg_buffers_observe_reload(&hw.ownership, false);
                    }
                    break;
                case 4U:
                    try_hardware_reload(&hw, decision >> 3U);
                    break;
                case 5U:
                    if (hw.ownership.probe == APP_BG_BUFFER_NONE)
                    {
                        assert(app_bg_buffers_pin_front(
                            &hw.ownership, &probe, &frame_id));
                        hw.probe_version = hw.memory_version[probe];
                    }
                    else
                    {
                        assert(app_bg_buffers_release_probe(
                            &hw.ownership, hw.ownership.probe));
                    }
                    break;
                case 6U:
                    if (hw.running != 0U)
                    {
                        hw.running = 0U; /* hardware stop precedes bookkeeping */
                        app_bg_buffers_stop(&hw.ownership);
                    }
                    else
                    {
                        assert(app_bg_buffers_restart(&hw.ownership));
                        hw.running = 1U;
                    }
                    break;
                default:
                    app_bg_buffers_observe_reload(
                        &hw.ownership,
                        hw.actual_pending != APP_BG_BUFFER_NONE);
                    break;
            }
            assert_physical_ownership(&hw);
        }
    }
}

static void test_validation_and_frame_id_wrap(void)
{
    app_bg_buffers_t state = {0};
    app_bg_capture_plan_t plan;

    assert(!app_bg_buffers_init(NULL, 3U, 1U, 0U));
    assert(!app_bg_buffers_init(&state, 2U, 1U, 0U));
    assert(!app_bg_buffers_init(&state, 3U, 1U, 1U));
    assert(!app_bg_buffers_valid(NULL));
    assert(!app_bg_buffers_restart(&state));
    assert(app_bg_buffers_init(&state, 3U, 1U, 0U));
    assert(app_bg_buffers_frame_complete(&state, UINT32_MAX, &plan));
    assert(app_bg_buffers_capture_target_result(&state, plan.next_fill, true));
    assert(app_bg_buffers_submission_result(&state, true));
    assert(app_bg_buffers_observe_reload(&state, false));
    assert(state.front_frame_id == UINT32_MAX);
    assert(app_bg_buffers_frame_complete(&state, 0U, &plan));
    assert(app_bg_buffers_capture_target_result(&state, plan.next_fill, true));
    assert(app_bg_buffers_submission_result(&state, true));
    assert(app_bg_buffers_observe_reload(&state, false));
    assert(state.front_frame_id == 0U && state.presentations == 2U);
    assert(app_bg_buffers_valid(&state));
    state.front = state.fill;
    assert(!app_bg_buffers_valid(&state));
}

int main(void)
{
    test_first_frame_and_delayed_vblank();
    test_failed_target_and_failed_reload();
    test_probe_pin_and_recovery();
    test_random_hardware_interleavings();
    test_validation_and_frame_id_wrap();
    puts("PASS: background ownership, 100001 delayed-VBlank captures, HAL failures, probe pin, recovery and 1600000 randomized hardware events");
    return 0;
}
