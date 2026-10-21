#include "app_zone_logic.h"

#include <string.h>

static uint8_t newer_frame(uint32_t next, uint32_t previous)
{
    uint32_t distance = next - previous;
    return (uint8_t)(distance != 0U && distance < UINT32_C(0x80000000));
}

static void forget_estimates(app_zone_logic_t *state, app_zone_quality_t quality)
{
    state->raw_count[0] = 0U;
    state->raw_count[1] = 0U;
    state->known_mask = 0U;
    state->occupied_mask = 0U;
    state->candidate_mask = 0U;
    state->confirming_mask = 0U;
    state->candidate_since_ms[0] = 0U;
    state->candidate_since_ms[1] = 0U;
    state->quality = quality;
}

void app_zone_logic_init(app_zone_logic_t *state)
{
    if (state != 0) {
        memset(state, 0, sizeof(*state));
        state->quality = APP_ZONE_MISSING;
    }
}

static uint8_t count_boxes(const app_zone_frame_t *frame,
                           uint8_t count[APP_ZONE_COUNT])
{
    uint8_t i;
    count[0] = 0U;
    count[1] = 0U;
    if (frame->count > APP_ZONE_MAX_BOXES) {
        return 0U;
    }
    for (i = 0U; i < frame->count; ++i) {
        const app_zone_box_t *box = &frame->boxes[i];
        uint32_t right = (uint32_t)box->x + box->width;
        uint32_t bottom = (uint32_t)box->y + box->height;
        uint32_t doubled_center;
        if (box->width == 0U || box->height == 0U ||
            right > APP_ZONE_WIDTH || bottom > APP_ZONE_HEIGHT) {
            /* Do not turn missing geometry into a false vacant zone. */
            return 0U;
        }
        doubled_center = (uint32_t)box->x * 2U + box->width;
        ++count[doubled_center < (APP_ZONE_SPLIT_X * 2U) ? 0U : 1U];
    }
    return 1U;
}

void app_zone_logic_update(app_zone_logic_t *state,
                           const app_zone_frame_t *frame, uint32_t now_ms)
{
    uint8_t count[APP_ZONE_COUNT];
    uint8_t zone;
    if (state == 0) {
        return;
    }
    if (state->quality == APP_ZONE_FRESH &&
        (uint32_t)(now_ms - state->captured_ms) >= APP_ZONE_STALE_MS) {
        forget_estimates(state, APP_ZONE_STALE);
    }
    if (frame == 0) {
        return;
    }
    if (!frame->valid || !count_boxes(frame, count)) {
        state->have_frame = 1U;
        state->frame_id = frame->frame_id;
        state->captured_ms = frame->captured_ms;
        forget_estimates(state, APP_ZONE_INVALID);
        return;
    }
    if (state->have_frame && !newer_frame(frame->frame_id, state->frame_id)) {
        return;
    }
    if (state->have_frame &&
        (uint32_t)(frame->captured_ms - state->captured_ms) >=
            UINT32_C(0x80000000)) {
        /* A new frame id with a backwards capture clock is not fresh evidence. */
        state->frame_id = frame->frame_id;
        state->captured_ms = frame->captured_ms;
        forget_estimates(state, APP_ZONE_INVALID);
        return;
    }
    state->have_frame = 1U;
    state->frame_id = frame->frame_id;
    state->captured_ms = frame->captured_ms;
    if ((uint32_t)(now_ms - frame->captured_ms) >= APP_ZONE_STALE_MS) {
        forget_estimates(state,
                         (uint32_t)(now_ms - frame->captured_ms) >=
                                 UINT32_C(0x80000000)
                             ? APP_ZONE_INVALID : APP_ZONE_STALE);
        return;
    }
    state->quality = APP_ZONE_FRESH;
    state->raw_count[0] = count[0];
    state->raw_count[1] = count[1];
    for (zone = 0U; zone < APP_ZONE_COUNT; ++zone) {
        uint8_t bit = (uint8_t)(1U << zone);
        uint8_t target = (uint8_t)(count[zone] != 0U);
        uint8_t occupied = (uint8_t)((state->occupied_mask & bit) != 0U);
        uint8_t candidate = (uint8_t)((state->candidate_mask & bit) != 0U);
        if ((state->known_mask & bit) != 0U && occupied == target) {
            state->confirming_mask &= (uint8_t)~bit;
            continue;
        }
        if ((state->confirming_mask & bit) == 0U || candidate != target) {
            state->confirming_mask |= bit;
            state->candidate_since_ms[zone] = frame->captured_ms;
            if (target) {
                state->candidate_mask |= bit;
            } else {
                state->candidate_mask &= (uint8_t)~bit;
            }
            continue;
        }
        if ((uint32_t)(frame->captured_ms - state->candidate_since_ms[zone]) >=
            (target ? APP_ZONE_ENTER_MS : APP_ZONE_LEAVE_MS)) {
            state->known_mask |= bit;
            state->confirming_mask &= (uint8_t)~bit;
            if (target) {
                state->occupied_mask |= bit;
            } else {
                state->occupied_mask &= (uint8_t)~bit;
            }
        }
    }
}

void app_zone_logic_get(const app_zone_logic_t *state, uint32_t now_ms,
                        app_zone_snapshot_t *snapshot)
{
    if (snapshot == 0) {
        return;
    }
    memset(snapshot, 0, sizeof(*snapshot));
    snapshot->advice = APP_ZONE_ADVICE_UNKNOWN;
    snapshot->quality = APP_ZONE_MISSING;
    snapshot->age_ms = UINT32_MAX;
    if (state == 0) {
        return;
    }
    snapshot->frame_id = state->frame_id;
    snapshot->captured_ms = state->captured_ms;
    snapshot->quality = state->quality;
    if (state->have_frame) {
        snapshot->age_ms = now_ms - state->captured_ms;
    }
    if (snapshot->quality == APP_ZONE_FRESH &&
        snapshot->age_ms >= APP_ZONE_STALE_MS) {
        snapshot->quality = APP_ZONE_STALE;
    }
    if (snapshot->quality != APP_ZONE_FRESH) {
        return;
    }
    snapshot->frame_valid = 1U;
    snapshot->raw_count[0] = state->raw_count[0];
    snapshot->raw_count[1] = state->raw_count[1];
    snapshot->known_mask = state->known_mask;
    snapshot->occupied_mask = state->occupied_mask;
    snapshot->confirming_mask = state->confirming_mask;
    if (snapshot->known_mask == APP_ZONE_ALL_MASK &&
        snapshot->confirming_mask == 0U) {
        switch (snapshot->occupied_mask) {
        case 0U: snapshot->advice = APP_ZONE_ADVICE_NONE; break;
        case APP_ZONE_LEFT_MASK: snapshot->advice = APP_ZONE_ADVICE_LEFT; break;
        case APP_ZONE_RIGHT_MASK: snapshot->advice = APP_ZONE_ADVICE_RIGHT; break;
        default: snapshot->advice = APP_ZONE_ADVICE_BOTH; break;
        }
    }
}
