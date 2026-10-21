#include "app_bg_buffers.h"

#include <stddef.h>
#include <string.h>

static uint8_t app_bg_find_free(const app_bg_buffers_t *state)
{
    uint8_t index;

    for (index = 0U; index < state->count; index++)
    {
        if ((index != state->front) && (index != state->fill) &&
            (index != state->pending) && (index != state->prepared) &&
            (index != state->probe))
        {
            return index;
        }
    }
    return APP_BG_BUFFER_NONE;
}

bool app_bg_buffers_init(app_bg_buffers_t *state, uint8_t count,
                         uint8_t front, uint8_t fill)
{
    if ((state == NULL) || (count < 3U) ||
        (front >= count) || (fill >= count) || (front == fill))
    {
        return false;
    }
    memset(state, 0, sizeof(*state));
    state->count = count;
    state->front = front;
    state->fill = fill;
    state->pending = APP_BG_BUFFER_NONE;
    state->prepared = APP_BG_BUFFER_NONE;
    state->next_fill = APP_BG_BUFFER_NONE;
    state->probe = APP_BG_BUFFER_NONE;
    state->running = 1U;
    return true;
}

bool app_bg_buffers_observe_reload(app_bg_buffers_t *state, bool vbr_active)
{
    if ((state == NULL) || (state->pending == APP_BG_BUFFER_NONE) ||
        vbr_active)
    {
        return false;
    }
    state->front = state->pending;
    state->front_frame_id = state->pending_frame_id;
    state->pending = APP_BG_BUFFER_NONE;
    state->pending_frame_id = 0U;
    state->presentations++;
    return true;
}

bool app_bg_buffers_frame_complete(app_bg_buffers_t *state, uint32_t frame_id,
                                  app_bg_capture_plan_t *plan)
{
    uint8_t next_fill;

    if ((state == NULL) || (plan == NULL))
    {
        return false;
    }
    plan->completed_index = state->fill;
    plan->next_fill = state->fill;
    plan->need_retarget = 0U;
    plan->frame_id = frame_id;
    if (state->running == 0U)
    {
        return false;
    }
    state->captures++;
    if ((state->pending != APP_BG_BUFFER_NONE) ||
        (state->phase != APP_BG_PREPARED_NONE))
    {
        state->dropped++;
        return false;
    }
    next_fill = app_bg_find_free(state);
    if (next_fill == APP_BG_BUFFER_NONE)
    {
        state->dropped++;
        return false;
    }
    state->prepared = state->fill;
    state->prepared_frame_id = frame_id;
    state->next_fill = next_fill;
    state->phase = APP_BG_PREPARED_CAPTURE_TARGET;
    plan->next_fill = next_fill;
    plan->need_retarget = 1U;
    return true;
}

bool app_bg_buffers_capture_target_result(app_bg_buffers_t *state,
                                         uint8_t next_fill, bool success)
{
    if ((state == NULL) ||
        (state->phase != APP_BG_PREPARED_CAPTURE_TARGET) ||
        (next_fill != state->next_fill))
    {
        return false;
    }
    if (!success)
    {
        /* No LTDC request has occurred: the old capture target may be reused.
         * HAL uncertainty about DMA itself must be handled by the caller. */
        state->prepared = APP_BG_BUFFER_NONE;
        state->prepared_frame_id = 0U;
        state->next_fill = APP_BG_BUFFER_NONE;
        state->phase = APP_BG_PREPARED_NONE;
        state->capture_target_failures++;
        state->dropped++;
        return true;
    }
    state->fill = next_fill;
    state->next_fill = APP_BG_BUFFER_NONE;
    state->phase = APP_BG_PREPARED_RELOAD;
    return true;
}

bool app_bg_buffers_candidate(const app_bg_buffers_t *state,
                              uint8_t *index, uint32_t *frame_id)
{
    if ((state == NULL) || (index == NULL) || (frame_id == NULL) ||
        (state->phase != APP_BG_PREPARED_RELOAD))
    {
        return false;
    }
    *index = state->prepared;
    *frame_id = state->prepared_frame_id;
    return true;
}

bool app_bg_buffers_submission_result(app_bg_buffers_t *state, bool accepted)
{
    if ((state == NULL) || (state->phase != APP_BG_PREPARED_RELOAD))
    {
        return false;
    }
    if (!accepted)
    {
        state->reload_failures++;
        return true;
    }
    state->pending = state->prepared;
    state->pending_frame_id = state->prepared_frame_id;
    state->prepared = APP_BG_BUFFER_NONE;
    state->prepared_frame_id = 0U;
    state->phase = APP_BG_PREPARED_NONE;
    state->submissions++;
    return true;
}

void app_bg_buffers_stop(app_bg_buffers_t *state)
{
    if (state == NULL)
    {
        return;
    }
    state->running = 0U;
    if (state->phase == APP_BG_PREPARED_CAPTURE_TARGET)
    {
        /* This phase precedes every LTDC call, so it has no scanout claim. */
        state->prepared = APP_BG_BUFFER_NONE;
        state->prepared_frame_id = 0U;
        state->next_fill = APP_BG_BUFFER_NONE;
        state->phase = APP_BG_PREPARED_NONE;
        state->dropped++;
    }
}

bool app_bg_buffers_restart(app_bg_buffers_t *state)
{
    if (!app_bg_buffers_valid(state))
    {
        return false;
    }
    state->running = 1U;
    return true;
}

bool app_bg_buffers_pin_front(app_bg_buffers_t *state,
                              uint8_t *index, uint32_t *frame_id)
{
    if ((state == NULL) || (index == NULL) || (frame_id == NULL) ||
        (state->probe != APP_BG_BUFFER_NONE))
    {
        return false;
    }
    state->probe = state->front;
    *index = state->front;
    *frame_id = state->front_frame_id;
    return true;
}

bool app_bg_buffers_release_probe(app_bg_buffers_t *state, uint8_t index)
{
    if ((state == NULL) || (state->probe == APP_BG_BUFFER_NONE) ||
        (index != state->probe))
    {
        return false;
    }
    state->probe = APP_BG_BUFFER_NONE;
    return true;
}

bool app_bg_buffers_valid(const app_bg_buffers_t *state)
{
    if ((state == NULL) || (state->count < 3U) ||
        (state->front >= state->count) || (state->fill >= state->count) ||
        (state->front == state->fill) || (state->running > 1U))
    {
        return false;
    }
    if ((state->pending != APP_BG_BUFFER_NONE) &&
        ((state->pending >= state->count) ||
         (state->pending == state->front) ||
         (state->pending == state->fill) ||
         (state->phase != APP_BG_PREPARED_NONE)))
    {
        return false;
    }
    if ((state->probe != APP_BG_BUFFER_NONE) &&
        ((state->probe >= state->count) || (state->probe == state->fill)))
    {
        return false;
    }
    switch (state->phase)
    {
        case APP_BG_PREPARED_NONE:
            return (state->prepared == APP_BG_BUFFER_NONE) &&
                   (state->next_fill == APP_BG_BUFFER_NONE);
        case APP_BG_PREPARED_CAPTURE_TARGET:
            return (state->prepared == state->fill) &&
                   (state->next_fill < state->count) &&
                   (state->next_fill != state->front) &&
                   (state->next_fill != state->fill) &&
                   (state->next_fill != state->probe);
        case APP_BG_PREPARED_RELOAD:
            return (state->prepared < state->count) &&
                   (state->prepared != state->front) &&
                   (state->prepared != state->fill) &&
                   (state->prepared != state->probe) &&
                   (state->next_fill == APP_BG_BUFFER_NONE);
        default:
            return false;
    }
}
