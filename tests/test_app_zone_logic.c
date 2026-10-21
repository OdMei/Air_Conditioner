#include "app_zone_logic.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;

#define CHECK(condition) do { \
    ++checks; \
    if (!(condition)) { \
        fprintf(stderr, "FAIL line %u: %s\n", (unsigned)__LINE__, #condition); \
        exit(1); \
    } \
} while (0)

static app_zone_frame_t make_frame(uint32_t id, uint32_t captured,
                                    unsigned left, unsigned right)
{
    app_zone_frame_t frame;
    unsigned i;
    memset(&frame, 0, sizeof(frame));
    frame.frame_id = id;
    frame.captured_ms = captured;
    frame.valid = 1U;
    frame.count = (uint8_t)(left + right);
    for (i = 0U; i < left + right && i < APP_ZONE_MAX_BOXES; ++i) {
        frame.boxes[i].x = (uint16_t)(i < left ? 50U : 600U);
        frame.boxes[i].y = 30U;
        frame.boxes[i].width = 80U;
        frame.boxes[i].height = 200U;
    }
    return frame;
}

static app_zone_snapshot_t feed(app_zone_logic_t *state, uint32_t id,
                                 uint32_t time, unsigned left, unsigned right)
{
    app_zone_frame_t frame = make_frame(id, time, left, right);
    app_zone_snapshot_t result;
    app_zone_logic_update(state, &frame, time);
    app_zone_logic_get(state, time, &result);
    return result;
}

static void confirm(app_zone_logic_t *state, uint32_t id, uint32_t time,
                     unsigned left, unsigned right)
{
    (void)feed(state, id, time, left, right);
    (void)feed(state, id + 1U, time + 500U, left, right);
    (void)feed(state, id + 2U, time + 1000U, left, right);
}

static void test_initial_and_stable(void)
{
    app_zone_logic_t state;
    app_zone_snapshot_t out;
    app_zone_logic_init(&state);
    app_zone_logic_get(&state, 0U, &out);
    CHECK(out.quality == APP_ZONE_MISSING && out.age_ms == UINT32_MAX);
    CHECK(out.advice == APP_ZONE_ADVICE_UNKNOWN && !out.frame_valid);
    out = feed(&state, 0U, 0U, 1U, 0U);
    CHECK(out.raw_count[0] == 1U && out.raw_count[1] == 0U);
    CHECK(out.confirming_mask == 3U && out.known_mask == 0U);
    out = feed(&state, 1U, 499U, 1U, 0U);
    CHECK(out.known_mask == 0U);
    out = feed(&state, 2U, 500U, 1U, 0U);
    CHECK(out.known_mask == 1U && out.occupied_mask == 1U);
    CHECK(out.confirming_mask == 2U && out.advice == APP_ZONE_ADVICE_UNKNOWN);
    out = feed(&state, 3U, 999U, 1U, 0U);
    CHECK(out.advice == APP_ZONE_ADVICE_UNKNOWN);
    out = feed(&state, 4U, 1000U, 1U, 0U);
    CHECK(out.known_mask == 3U && out.confirming_mask == 0U);
    CHECK(out.advice == APP_ZONE_ADVICE_LEFT);
    out = feed(&state, 5U, 1100U, 2U, 0U);
    CHECK(out.raw_count[0] == 2U && out.advice == APP_ZONE_ADVICE_LEFT);
}

static void test_transitions_and_jitter(void)
{
    app_zone_logic_t state;
    app_zone_snapshot_t out;
    app_zone_logic_init(&state);
    confirm(&state, 1U, 0U, 1U, 0U);
    out = feed(&state, 4U, 1100U, 0U, 1U);
    CHECK(out.occupied_mask == 1U && out.confirming_mask == 3U);
    CHECK(out.advice == APP_ZONE_ADVICE_UNKNOWN);
    out = feed(&state, 5U, 1600U, 0U, 1U);
    CHECK(out.occupied_mask == 3U && out.confirming_mask == 1U);
    out = feed(&state, 6U, 2099U, 0U, 1U);
    CHECK(out.confirming_mask == 1U);
    out = feed(&state, 7U, 2100U, 0U, 1U);
    CHECK(out.advice == APP_ZONE_ADVICE_RIGHT && out.occupied_mask == 2U);
    out = feed(&state, 8U, 2200U, 0U, 0U);
    CHECK(out.occupied_mask == 2U && out.advice == APP_ZONE_ADVICE_UNKNOWN);
    out = feed(&state, 9U, 3199U, 0U, 0U);
    CHECK(out.occupied_mask == 2U);
    out = feed(&state, 10U, 3200U, 0U, 0U);
    CHECK(out.advice == APP_ZONE_ADVICE_NONE && out.occupied_mask == 0U);
    out = feed(&state, 11U, 3300U, 1U, 0U);
    CHECK(out.confirming_mask == 1U);
    out = feed(&state, 12U, 3400U, 0U, 0U);
    CHECK(out.confirming_mask == 0U && out.advice == APP_ZONE_ADVICE_NONE);
    out = feed(&state, 13U, 3500U, 1U, 0U);
    out = feed(&state, 14U, 3999U, 1U, 0U);
    CHECK(out.occupied_mask == 0U);
    out = feed(&state, 15U, 4000U, 1U, 0U);
    CHECK(out.advice == APP_ZONE_ADVICE_LEFT);
    out = feed(&state, 16U, 4100U, 1U, 1U);
    out = feed(&state, 17U, 4600U, 1U, 1U);
    CHECK(out.advice == APP_ZONE_ADVICE_BOTH);
}

static void test_duplicate_old_and_expiry(void)
{
    app_zone_logic_t state;
    app_zone_frame_t frame;
    app_zone_snapshot_t out;
    app_zone_logic_init(&state);
    (void)feed(&state, 1U, 0U, 1U, 0U);
    (void)feed(&state, 2U, 500U, 1U, 0U);
    frame = make_frame(2U, 1000U, 0U, 3U);
    app_zone_logic_update(&state, &frame, 1000U);
    app_zone_logic_get(&state, 1000U, &out);
    CHECK(out.frame_id == 2U && out.captured_ms == 500U);
    CHECK(out.raw_count[0] == 1U && out.raw_count[1] == 0U);
    CHECK(out.known_mask == 1U && out.confirming_mask == 2U);
    out = feed(&state, 3U, 1000U, 1U, 0U);
    CHECK(out.advice == APP_ZONE_ADVICE_LEFT);
    frame = make_frame(1U, 1500U, 0U, 3U);
    app_zone_logic_update(&state, &frame, 1500U);
    app_zone_logic_get(&state, 1500U, &out);
    CHECK(out.frame_id == 3U && out.captured_ms == 1000U);
    CHECK(out.advice == APP_ZONE_ADVICE_LEFT);
    frame = make_frame(3U, 3999U, 1U, 0U);
    app_zone_logic_update(&state, &frame, 3999U);
    app_zone_logic_get(&state, 3999U, &out);
    CHECK(out.frame_valid && out.age_ms == 2999U);
    app_zone_logic_get(&state, 4000U, &out);
    CHECK(out.quality == APP_ZONE_STALE && out.advice == APP_ZONE_ADVICE_UNKNOWN);
    CHECK(out.known_mask == 0U && out.raw_count[0] == 0U);
    app_zone_logic_update(&state, 0, 4000U);
    out = feed(&state, 4U, 4001U, 1U, 0U);
    CHECK(out.known_mask == 0U && out.confirming_mask == 3U);
    out = feed(&state, 5U, 4501U, 1U, 0U);
    out = feed(&state, 6U, 5001U, 1U, 0U);
    CHECK(out.advice == APP_ZONE_ADVICE_LEFT);
    out = feed(&state, 7U, 8001U, 1U, 0U);
    CHECK(out.known_mask == 0U && out.advice == APP_ZONE_ADVICE_UNKNOWN);
}

static void test_invalid_and_restart(void)
{
    app_zone_logic_t state;
    app_zone_frame_t frame;
    app_zone_snapshot_t out;
    app_zone_logic_init(&state);
    confirm(&state, 10U, 0U, 1U, 1U);
    frame = make_frame(12U, 1000U, 1U, 1U);
    frame.valid = 0U;
    app_zone_logic_update(&state, &frame, 1000U);
    app_zone_logic_get(&state, 1000U, &out);
    CHECK(out.quality == APP_ZONE_INVALID && out.known_mask == 0U);
    CHECK(out.advice == APP_ZONE_ADVICE_UNKNOWN && !out.frame_valid);
    frame.valid = 1U;
    app_zone_logic_update(&state, &frame, 1500U);
    app_zone_logic_get(&state, 1500U, &out);
    CHECK(out.quality == APP_ZONE_INVALID);
    out = feed(&state, 13U, 1500U, 0U, 0U);
    CHECK(out.quality == APP_ZONE_FRESH && out.confirming_mask == 3U);
    out = feed(&state, 14U, 2500U, 0U, 0U);
    CHECK(out.advice == APP_ZONE_ADVICE_NONE);
    frame = make_frame(15U, 2400U, 1U, 0U);
    app_zone_logic_update(&state, &frame, 2600U);
    app_zone_logic_get(&state, 2600U, &out);
    CHECK(out.quality == APP_ZONE_INVALID);
    app_zone_logic_init(&state);
    out = feed(&state, 0U, 2700U, 0U, 1U);
    CHECK(out.frame_valid && out.frame_id == 0U && out.known_mask == 0U);
    out = feed(&state, 1U, 3700U, 0U, 1U);
    CHECK(out.advice == APP_ZONE_ADVICE_RIGHT);
    frame = make_frame(2U, 3801U, 0U, 1U);
    app_zone_logic_update(&state, &frame, 3800U);
    app_zone_logic_get(&state, 3800U, &out);
    CHECK(out.quality == APP_ZONE_INVALID);
    app_zone_logic_init(&state);
    frame = make_frame(0U, 0U, 0U, 0U);
    app_zone_logic_update(&state, &frame, 3000U);
    app_zone_logic_get(&state, 3000U, &out);
    CHECK(out.quality == APP_ZONE_STALE && out.known_mask == 0U);
}

static void test_geometry_and_boundaries(void)
{
    app_zone_logic_t state;
    app_zone_frame_t frame;
    app_zone_snapshot_t out;
    unsigned variant;
    for (variant = 0U; variant < 7U; ++variant) {
        app_zone_logic_init(&state);
        confirm(&state, 0U, 0U, 1U, 1U);
        frame = make_frame(3U, 1100U, 1U, 1U);
        switch (variant) {
        case 0U: frame.count = 11U; break;
        case 1U: frame.boxes[1].width = 0U; break;
        case 2U: frame.boxes[1].height = 0U; break;
        case 3U: frame.boxes[1].x = 799U; frame.boxes[1].width = 2U; break;
        case 4U: frame.boxes[1].y = 479U; frame.boxes[1].height = 2U; break;
        case 5U: frame.boxes[1].x = UINT16_MAX; break;
        default: frame.boxes[1].width = UINT16_MAX; break;
        }
        app_zone_logic_update(&state, &frame, 1100U);
        app_zone_logic_get(&state, 1100U, &out);
        CHECK(out.quality == APP_ZONE_INVALID && out.advice == APP_ZONE_ADVICE_UNKNOWN);
        CHECK(out.known_mask == 0U && out.raw_count[0] == 0U);
    }
    app_zone_logic_init(&state);
    frame = make_frame(0U, 0U, 0U, 0U);
    frame.count = 5U;
    frame.boxes[0] = (app_zone_box_t){0U, 0U, 1U, 1U};
    frame.boxes[1] = (app_zone_box_t){399U, 0U, 1U, 1U};
    frame.boxes[2] = (app_zone_box_t){399U, 0U, 2U, 1U};
    frame.boxes[3] = (app_zone_box_t){400U, 0U, 1U, 1U};
    frame.boxes[4] = (app_zone_box_t){799U, 479U, 1U, 1U};
    app_zone_logic_update(&state, &frame, 0U);
    app_zone_logic_get(&state, 0U, &out);
    CHECK(out.frame_valid && out.raw_count[0] == 2U && out.raw_count[1] == 3U);
    frame.frame_id = 1U;
    frame.captured_ms = 500U;
    app_zone_logic_update(&state, &frame, 500U);
    app_zone_logic_get(&state, 500U, &out);
    CHECK(out.advice == APP_ZONE_ADVICE_BOTH);
    out = feed(&state, 2U, 600U, 5U, 5U);
    CHECK(out.raw_count[0] == 5U && out.raw_count[1] == 5U);
    CHECK(out.advice == APP_ZONE_ADVICE_BOTH);
}

static void test_wrap_and_null(void)
{
    app_zone_logic_t state;
    app_zone_snapshot_t out;
    uint32_t start = UINT32_MAX - 600U;
    app_zone_logic_init(&state);
    out = feed(&state, UINT32_MAX - 1U, start, 1U, 0U);
    CHECK(out.known_mask == 0U);
    out = feed(&state, UINT32_MAX, start + 500U, 1U, 0U);
    CHECK(out.known_mask == 1U);
    out = feed(&state, 0U, start + 1000U, 1U, 0U);
    CHECK(out.advice == APP_ZONE_ADVICE_LEFT && out.frame_id == 0U);
    app_zone_logic_get(&state, start + 3999U, &out);
    CHECK(out.frame_valid && out.age_ms == 2999U);
    app_zone_logic_get(&state, start + 4000U, &out);
    CHECK(out.quality == APP_ZONE_STALE);
    app_zone_logic_init(0);
    app_zone_logic_update(0, 0, 0U);
    app_zone_logic_get(0, 0U, &out);
    CHECK(out.quality == APP_ZONE_MISSING);
    app_zone_logic_get(&state, 0U, 0);
}

int main(void)
{
    test_initial_and_stable();
    test_transitions_and_jitter();
    test_duplicate_old_and_expiry();
    test_invalid_and_restart();
    test_geometry_and_boundaries();
    test_wrap_and_null();
    printf("PASS: zone logic (%u behavior assertions)\n", checks);
    return 0;
}
