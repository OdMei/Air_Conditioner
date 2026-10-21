#include "app_detection_smoother.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned int s_checks;
#define REQUIRE(condition) do { s_checks++; if (!(condition)) { \
    fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); return 0; } } while (0)

static app_vision_result_t frame(uint32_t id, uint32_t captured, uint16_t x)
{
    app_vision_result_t result = {0};
    result.valid = 1U; result.count = 1U;
    result.frame_id = id; result.captured_ms = captured; result.completed_ms = captured + 7U;
    result.boxes[0] = (app_ui_detection_t){x, 100U, 100U, 180U, 0.91f, 0};
    return result;
}

static int stationary_jitter(void)
{
    app_detection_smoother_t state;
    app_detection_smoother_init(&state);
    double raw_energy = 0.0, filtered_energy = 0.0;
    unsigned int observations = 0U;
    for (unsigned int i = 0U; i < 180U; i++) {
        int jitter = i & 1U ? 1 : -1;
        app_vision_result_t input = frame(i, i * 33U, (uint16_t)(200 + jitter * 8));
        input.boxes[0].y = (uint16_t)(100 + jitter * 6);
        input.boxes[0].width = (uint16_t)(100 + jitter * 4);
        input.boxes[0].height = (uint16_t)(180 + jitter * 6);
        input.boxes[0].confidence = i & 1U ? 0.83f : 0.61f;
        app_vision_result_t untouched = input;
        const app_vision_result_t *output = app_detection_smoother_update(&state, &input, 1U);
        REQUIRE(memcmp(&input, &untouched, sizeof(input)) == 0);
        REQUIRE(output->valid && output->count == 1U);
        REQUIRE(output->boxes[0].confidence == input.boxes[0].confidence);
        REQUIRE(output->boxes[0].class_index == input.boxes[0].class_index);
        REQUIRE(output->frame_id == input.frame_id && output->captured_ms == input.captured_ms &&
                output->completed_ms == input.completed_ms);
        if (i >= 60U) {
            double raw_error = (double)input.boxes[0].x - 200.0;
            double filtered_error = (double)output->boxes[0].x - 200.0;
            raw_energy += raw_error * raw_error;
            filtered_energy += filtered_error * filtered_error;
            observations++;
            REQUIRE(abs((int)output->boxes[0].x - 200) <= 3);
        }
    }
    double raw_rms = sqrt(raw_energy / observations);
    double filtered_rms = sqrt(filtered_energy / observations);
    printf("30 Hz stationary jitter: raw RMS=%.3f px, display RMS=%.3f px, reduction=%.1f%%\n",
        raw_rms, filtered_rms, (1.0 - filtered_rms / raw_rms) * 100.0);
    REQUIRE(filtered_rms <= raw_rms * 0.5);
    return 1;
}

static int fast_motion_and_snap(void)
{
    unsigned int maximum_lag = 0U;
    const unsigned int steps[] = {10U, 30U};
    for (unsigned int speed = 0U; speed < 2U; speed++) {
        app_detection_smoother_t state;
        app_detection_smoother_init(&state);
        for (unsigned int i = 0U; i < 20U; i++) {
            app_vision_result_t input = frame(i, i * 33U, (uint16_t)(40U + i * steps[speed]));
            const app_vision_result_t *output = app_detection_smoother_update(&state, &input, 1U);
            unsigned int lag = input.boxes[0].x - output->boxes[0].x;
            if (lag > maximum_lag) { maximum_lag = lag; }
            REQUIRE(lag <= (speed ? 8U : 20U));
            REQUIRE(output->count == 1U && output->boxes[0].x <= input.boxes[0].x);
        }
    }
    printf("30 Hz moving boxes: maximum observed lag=%u px (10/30 px per frame)\n", maximum_lag);
    app_detection_smoother_t state;
    app_detection_smoother_init(&state);
    app_vision_result_t input = frame(1U, 1000U, 100U);
    input.boxes[0].width = 400U; /* Large jump still overlaps this wide box. */
    app_detection_smoother_update(&state, &input, 1U);
    input.frame_id++; input.captured_ms += 33U; input.boxes[0].x = 220U;
    const app_vision_result_t *output = app_detection_smoother_update(&state, &input, 1U);
    REQUIRE(output->boxes[0].x == 220U);
    input.frame_id++; input.captured_ms += 33U; input.boxes[0].width = 180U;
    output = app_detection_smoother_update(&state, &input, 1U);
    REQUIRE(output->boxes[0].width == 180U);
    input.frame_id++; input.captured_ms += 33U; input.boxes[0].x = 550U;
    output = app_detection_smoother_update(&state, &input, 1U);
    REQUIRE(output->boxes[0].x == 550U); /* Unmatched discontinuity has no lag. */
    return 1;
}

static int order_independence_and_no_ghosts(void)
{
    app_detection_smoother_t ordered, reversed;
    app_detection_smoother_init(&ordered); app_detection_smoother_init(&reversed);
    app_vision_result_t input = frame(1U, 1000U, 100U);
    input.count = 2U;
    input.boxes[1] = (app_ui_detection_t){500U, 100U, 100U, 180U, 0.74f, 0};
    app_detection_smoother_update(&ordered, &input, 1U);
    app_detection_smoother_update(&reversed, &input, 1U);
    input.frame_id++; input.captured_ms += 33U;
    input.boxes[0].x += 8U; input.boxes[1].x -= 8U;
    const app_vision_result_t *first = app_detection_smoother_update(&ordered, &input, 1U);
    app_ui_detection_t swap = input.boxes[0]; input.boxes[0] = input.boxes[1]; input.boxes[1] = swap;
    const app_vision_result_t *second = app_detection_smoother_update(&reversed, &input, 1U);
    REQUIRE(first->boxes[0].x == second->boxes[1].x && first->boxes[1].x == second->boxes[0].x);
    REQUIRE(first->boxes[0].confidence == second->boxes[1].confidence &&
            first->boxes[1].confidence == second->boxes[0].confidence);
    REQUIRE(second->boxes[0].x > 480U && second->boxes[1].x < 150U);
    input.frame_id++; input.captured_ms += 33U; input.count = 1U;
    second = app_detection_smoother_update(&reversed, &input, 1U);
    REQUIRE(second->count == 1U && second->boxes[0].x > 480U);
    REQUIRE(second->boxes[1].width == 0U && reversed.slots[1].width == 0.0f);
    input.frame_id++; input.captured_ms += 33U;
    input.boxes[0].class_index = 1; input.boxes[0].x = 490U;
    second = app_detection_smoother_update(&reversed, &input, 1U);
    REQUIRE(second->boxes[0].class_index == 1 && second->boxes[0].x == 490U);
    input.count = 0U;
    second = app_detection_smoother_update(&reversed, &input, 1U);
    REQUIRE(!second->valid && !second->count && !reversed.initialized);
    input.count = 1U;
    second = app_detection_smoother_update(&reversed, &input, 1U);
    REQUIRE(second->boxes[0].x == input.boxes[0].x);
    return 1;
}

static int repeated_frames_and_immediate_invalidity(void)
{
    app_detection_smoother_t state;
    app_detection_smoother_init(&state);
    app_vision_result_t input = frame(9U, 1000U, 100U);
    app_detection_smoother_update(&state, &input, 1U);
    input.frame_id++; input.captured_ms += 33U; input.boxes[0].x = 108U;
    app_detection_smoother_update(&state, &input, 1U);
    app_detection_smoother_t frozen = state;
    input.captured_ms += 10000U; input.boxes[0].x = 200U;
    for (unsigned int i = 0U; i < 100U; i++) {
        app_detection_smoother_update(&state, &input, 1U);
        REQUIRE(memcmp(&state, &frozen, sizeof(state)) == 0);
    }
    input.valid = 0U;
    const app_vision_result_t *output = app_detection_smoother_update(&state, &input, 1U);
    REQUIRE(!output->valid && output->count == 0U);
    input.valid = 1U;
    output = app_detection_smoother_update(&state, &input, 1U);
    REQUIRE(output->boxes[0].x == 200U);
    output = app_detection_smoother_update(&state, &input, 0U);
    REQUIRE(!output->valid && !output->count);
    input.count = APP_UI_MAX_DETECTIONS + 1U;
    output = app_detection_smoother_update(&state, &input, 1U);
    REQUIRE(!output->valid && !output->count);
    output = app_detection_smoother_update(&state, NULL, 1U);
    REQUIRE(!output->valid && !output->count);
    REQUIRE(app_detection_smoother_update(NULL, &input, 1U) == NULL);
    app_detection_smoother_reset(NULL);
    return 1;
}

static int capture_timing_and_wrap(void)
{
    app_detection_smoother_t state;
    app_detection_smoother_init(&state);
    app_vision_result_t input = frame(UINT32_MAX, UINT32_MAX - 20U, 100U);
    app_detection_smoother_update(&state, &input, 1U);
    input.frame_id = 0U; input.captured_ms = 12U; input.boxes[0].x = 108U;
    const app_vision_result_t *output = app_detection_smoother_update(&state, &input, 1U);
    REQUIRE(output->frame_id == 0U && output->boxes[0].x > 100U && output->boxes[0].x < 108U);
    input.frame_id++; input.captured_ms += APP_DETECTION_SMOOTHER_MAX_GAP_MS + 1U;
    input.boxes[0].x = 116U;
    output = app_detection_smoother_update(&state, &input, 1U);
    REQUIRE(output->boxes[0].x == 116U);
    input.frame_id++; input.captured_ms--;
    input.boxes[0].x = 124U;
    output = app_detection_smoother_update(&state, &input, 1U);
    REQUIRE(output->boxes[0].x == 124U);
    input.frame_id++; input.captured_ms += APP_DETECTION_SMOOTHER_MAX_GAP_MS;
    input.boxes[0].x = 132U;
    output = app_detection_smoother_update(&state, &input, 1U);
    REQUIRE(output->boxes[0].x > 124U && output->boxes[0].x < 132U);
    uint16_t position = output->boxes[0].x;
    input.frame_id++; input.boxes[0].x += 2U; /* New ID at identical capture time. */
    output = app_detection_smoother_update(&state, &input, 1U);
    REQUIRE(output->boxes[0].x == position);
    app_detection_smoother_reset(&state);
    input = frame(0U, 0U, 100U);
    app_detection_smoother_update(&state, &input, 1U);
    input.frame_id++; input.captured_ms = 33U; input.boxes[0].x = 108U;
    output = app_detection_smoother_update(&state, &input, 1U);
    REQUIRE(output->boxes[0].x > 100U && output->boxes[0].x < 108U);

    app_detection_smoother_t short_interval, long_interval;
    app_detection_smoother_init(&short_interval); app_detection_smoother_init(&long_interval);
    input = frame(1U, 1000U, 400U);
    app_detection_smoother_update(&short_interval, &input, 1U);
    app_detection_smoother_update(&long_interval, &input, 1U);
    input.frame_id++; input.boxes[0].x = 410U; input.captured_ms = 1005U;
    uint16_t short_x = app_detection_smoother_update(&short_interval, &input, 1U)->boxes[0].x;
    input.captured_ms = 1100U;
    uint16_t long_x = app_detection_smoother_update(&long_interval, &input, 1U)->boxes[0].x;
    REQUIRE(short_x < long_x && long_x < 410U);
    return 1;
}

static int malformed_geometry_and_compaction(void)
{
    const float bad_confidence[] = {NAN, INFINITY, -INFINITY, -0.01f, 1.01f};
    app_detection_smoother_t state;
    app_detection_smoother_init(&state);
    for (unsigned int i = 0U; i < sizeof(bad_confidence) / sizeof(bad_confidence[0]); i++) {
        app_vision_result_t input = frame(i, i * 33U, 100U);
        input.boxes[0].confidence = bad_confidence[i];
        const app_vision_result_t *output = app_detection_smoother_update(&state, &input, 1U);
        REQUIRE(!output->valid && !output->count);
    }
    for (unsigned int field = 0U; field < 5U; field++) {
        app_vision_result_t input = frame(field, field * 33U, 100U);
        if (field == 0U) { input.boxes[0].width = 0U; }
        if (field == 1U) { input.boxes[0].height = 0U; }
        if (field == 2U) { input.boxes[0].x = LCD_FG_WIDTH; }
        if (field == 3U) { input.boxes[0].y = LCD_FG_HEIGHT; }
        if (field == 4U) { input.boxes[0].class_index = -1; }
        const app_vision_result_t *output = app_detection_smoother_update(&state, &input, 1U);
        REQUIRE(!output->valid && !output->count);
    }
    app_vision_result_t input = frame(20U, 1000U, LCD_FG_WIDTH - 1U);
    input.boxes[0].y = LCD_FG_HEIGHT - 1U;
    input.boxes[0].width = UINT16_MAX; input.boxes[0].height = UINT16_MAX;
    const app_vision_result_t *output = app_detection_smoother_update(&state, &input, 1U);
    REQUIRE(output->count == 1U && output->boxes[0].width == 1U && output->boxes[0].height == 1U);
    input.frame_id++; input.captured_ms += 33U; input.count = 3U;
    input.boxes[0].confidence = NAN;
    input.boxes[1] = (app_ui_detection_t){200U, 100U, 100U, 180U, 0.0f, 0};
    input.boxes[2] = (app_ui_detection_t){400U, 100U, 100U, 180U, 1.0f, 1};
    app_vision_result_t untouched = input;
    output = app_detection_smoother_update(&state, &input, 1U);
    REQUIRE(output->count == 2U && output->boxes[0].confidence == 0.0f && output->boxes[1].confidence == 1.0f);
    REQUIRE(output->boxes[0].x == 200U && output->boxes[1].x == 400U);
    REQUIRE(memcmp(&input, &untouched, sizeof(input)) == 0);
    return 1;
}

static int tiny_boxes_and_ten_slots(void)
{
    app_detection_smoother_t state;
    app_detection_smoother_init(&state);
    app_vision_result_t input = frame(1U, 1000U, 100U);
    input.boxes[0].width = input.boxes[0].height = 10U;
    app_detection_smoother_update(&state, &input, 1U);
    input.frame_id++; input.captured_ms += 33U; input.boxes[0].x += 10U;
    const app_vision_result_t *output = app_detection_smoother_update(&state, &input, 1U);
    REQUIRE(output->boxes[0].x > 100U && output->boxes[0].x < 110U);
    input.frame_id++; input.captured_ms += 33U; input.boxes[0].x += 13U;
    output = app_detection_smoother_update(&state, &input, 1U);
    REQUIRE(output->boxes[0].x == input.boxes[0].x);

    app_detection_smoother_reset(&state);
    input = frame(1U, 1000U, 20U);
    input.count = APP_UI_MAX_DETECTIONS;
    for (unsigned int i = 0U; i < input.count; i++) {
        input.boxes[i] = (app_ui_detection_t){(uint16_t)(20U + 70U * i), 100U, 30U, 100U, 0.95f, 0};
    }
    app_detection_smoother_update(&state, &input, 1U);
    input.frame_id++; input.captured_ms += 33U;
    for (unsigned int i = 0U; i < input.count / 2U; i++) {
        app_ui_detection_t swap = input.boxes[i];
        input.boxes[i] = input.boxes[input.count - 1U - i];
        input.boxes[input.count - 1U - i] = swap;
    }
    for (unsigned int i = 0U; i < input.count; i++) { input.boxes[i].x += 2U; }
    output = app_detection_smoother_update(&state, &input, 1U);
    REQUIRE(output->count == APP_UI_MAX_DETECTIONS);
    for (unsigned int i = 0U; i < input.count; i++) {
        REQUIRE(output->boxes[i].x >= input.boxes[i].x - 2U && output->boxes[i].x < input.boxes[i].x);
    }
    return 1;
}

static uint32_t random_value(uint32_t *seed)
{
    *seed ^= *seed << 13; *seed ^= *seed >> 17; *seed ^= *seed << 5;
    return *seed;
}

static int randomized_boundaries(void)
{
    app_detection_smoother_t state;
    app_detection_smoother_init(&state);
    uint32_t seed = 0x64393A7BU, captured = UINT32_MAX - 500U, id = UINT32_MAX - 20U;
    for (unsigned int event = 0U; event < 100000U; event++) {
        captured += random_value(&seed) % 350U;
        if (random_value(&seed) % 5U) { id++; }
        app_vision_result_t input = frame(id, captured, 100U);
        input.valid = (uint8_t)(random_value(&seed) % 11U != 0U);
        input.count = (uint8_t)(random_value(&seed) % (APP_UI_MAX_DETECTIONS + 2U));
        for (unsigned int i = 0U; i < APP_UI_MAX_DETECTIONS; i++) {
            input.boxes[i] = (app_ui_detection_t){
                (uint16_t)(random_value(&seed) % 850U), (uint16_t)(random_value(&seed) % 530U),
                (uint16_t)(random_value(&seed) % 200U), (uint16_t)(random_value(&seed) % 300U),
                (float)(random_value(&seed) % 101U) / 100.0f, (int32_t)(random_value(&seed) % 3U)};
            if (random_value(&seed) % 31U == 0U) { input.boxes[i].confidence = NAN; }
        }
        app_vision_result_t untouched = input;
        uint8_t fresh = (uint8_t)(random_value(&seed) % 10U != 0U);
        const app_vision_result_t *output = app_detection_smoother_update(&state, &input, fresh);
        REQUIRE(memcmp(&input, &untouched, sizeof(input)) == 0);
        REQUIRE(output->count <= APP_UI_MAX_DETECTIONS);
        if (!input.valid || !fresh || input.count == 0U || input.count > APP_UI_MAX_DETECTIONS) {
            REQUIRE(!output->valid && !output->count);
        }
        for (unsigned int i = 0U; i < output->count; i++) {
            const app_ui_detection_t *box = &output->boxes[i];
            REQUIRE(box->width != 0U && box->height != 0U);
            REQUIRE((uint32_t)box->x + box->width <= LCD_FG_WIDTH);
            REQUIRE((uint32_t)box->y + box->height <= LCD_FG_HEIGHT);
            REQUIRE(isfinite(box->confidence) && box->confidence >= 0.0f && box->confidence <= 1.0f);
        }
    }
    printf("100000 randomized reports: geometry bounds, invalidity and immutable input verified\n");
    return 1;
}

int main(void)
{
    struct { const char *name; int (*run)(void); } tests[] = {
        {"stationary jitter reduction and unchanged raw/confidence", stationary_jitter},
        {"fast motion and discontinuity snap", fast_motion_and_snap},
        {"order-independent correspondence, class separation and no ghost boxes", order_independence_and_no_ghosts},
        {"repeated frame stability and immediate invalidity", repeated_frames_and_immediate_invalidity},
        {"capture intervals, long gaps, timestamp zero and uint32 wrap", capture_timing_and_wrap},
        {"nonfinite confidence, clipped geometry and invalid-box compaction", malformed_geometry_and_compaction},
        {"small-box correspondence and all ten reordered slots", tiny_boxes_and_ten_slots},
        {"randomized boundary reports", randomized_boundaries}
    };
    printf("Detection smoother state=%zu B, slot=%zu B, dynamic allocation=0\n",
        sizeof(app_detection_smoother_t), sizeof(app_detection_smoother_slot_t));
    for (unsigned int i = 0U; i < sizeof(tests) / sizeof(tests[0]); i++) {
        if (!tests[i].run()) { return 1; }
        printf("PASS: %s\n", tests[i].name);
    }
    printf("PASS: all 8 groups / %u assertions\n", s_checks);
    return 0;
}
