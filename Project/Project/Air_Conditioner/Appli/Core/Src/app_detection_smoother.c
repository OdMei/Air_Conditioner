#include "app_detection_smoother.h"

#include <math.h>
#include <string.h>

static float maximum(float a, float b) { return a > b ? a : b; }
static float minimum(float a, float b) { return a < b ? a : b; }
static float absolute(float value) { return value < 0.0f ? -value : value; }

static uint8_t sanitize(const app_ui_detection_t *raw, app_ui_detection_t *box)
{
    if (raw->width == 0U || raw->height == 0U ||
        raw->x >= LCD_FG_WIDTH || raw->y >= LCD_FG_HEIGHT ||
        raw->class_index < 0 || !isfinite(raw->confidence) ||
        raw->confidence < 0.0f || raw->confidence > 1.0f) { return 0U; }
    *box = *raw;
    uint32_t available_width = LCD_FG_WIDTH - raw->x;
    uint32_t available_height = LCD_FG_HEIGHT - raw->y;
    if (box->width > available_width) { box->width = (uint16_t)available_width; }
    if (box->height > available_height) { box->height = (uint16_t)available_height; }
    return 1U;
}

static float correspondence_score(const app_ui_detection_t *current,
                                   const app_ui_detection_t *previous)
{
    if (current->class_index != previous->class_index) { return 0.0f; }
    float left = maximum(current->x, previous->x);
    float top = maximum(current->y, previous->y);
    float right = minimum((float)current->x + current->width,
                          (float)previous->x + previous->width);
    float bottom = minimum((float)current->y + current->height,
                           (float)previous->y + previous->height);
    float intersection = maximum(0.0f, right - left) * maximum(0.0f, bottom - top);
    float union_area = (float)current->width * current->height +
        (float)previous->width * previous->height - intersection;
    float overlap = intersection / union_area;
    if (overlap >= 0.10f) { return overlap; }

    /* Very small rectangles can lose overlap after just a few pixels of
       motion. Only nearby, similarly sized boxes may bridge that case. The
       lower score always gives a genuine-overlap match priority. */
    float width_ratio = (float)current->width / previous->width;
    float height_ratio = (float)current->height / previous->height;
    if (width_ratio < 0.5f || width_ratio > 2.0f ||
        height_ratio < 0.5f || height_ratio > 2.0f) { return 0.0f; }
    float center_x = absolute((float)current->x + current->width * 0.5f -
                              previous->x - previous->width * 0.5f);
    float center_y = absolute((float)current->y + current->height * 0.5f -
                              previous->y - previous->height * 0.5f);
    float gate = maximum(12.0f, minimum(current->width, current->height) * 0.15f);
    float distance = maximum(center_x, center_y);
    if (distance >= gate) { return 0.0f; }
    return 0.05f * (1.0f - distance / gate);
}

static uint8_t comes_before(const app_ui_detection_t *a, const app_ui_detection_t *b)
{
    if (a->x != b->x) { return (uint8_t)(a->x < b->x); }
    if (a->y != b->y) { return (uint8_t)(a->y < b->y); }
    if (a->width != b->width) { return (uint8_t)(a->width < b->width); }
    if (a->height != b->height) { return (uint8_t)(a->height < b->height); }
    return (uint8_t)(a->class_index < b->class_index);
}

static void start_slot(app_detection_smoother_slot_t *slot, const app_ui_detection_t *raw)
{
    slot->x = raw->x;
    slot->y = raw->y;
    slot->width = raw->width;
    slot->height = raw->height;
    slot->raw = *raw;
}

static void update_slot(app_detection_smoother_slot_t *slot,
                         const app_detection_smoother_slot_t *previous,
                         const app_ui_detection_t *raw, uint32_t elapsed_ms)
{
    float center_x = absolute((float)raw->x + raw->width * 0.5f -
                              previous->raw.x - previous->raw.width * 0.5f);
    float center_y = absolute((float)raw->y + raw->height * 0.5f -
                              previous->raw.y - previous->raw.height * 0.5f);
    float snap_distance = maximum(80.0f,
        minimum(previous->raw.width, previous->raw.height) * 0.5f);
    float width_change = absolute((float)raw->width - previous->raw.width);
    float height_change = absolute((float)raw->height - previous->raw.height);
    if (maximum(center_x, center_y) >= snap_distance ||
        width_change >= previous->raw.width * 0.5f ||
        height_change >= previous->raw.height * 0.5f) {
        start_slot(slot, raw);
        return;
    }
    float displacement = maximum(absolute((float)raw->x - previous->x),
                                 absolute((float)raw->y - previous->y));
    displacement = maximum(displacement, width_change * 0.5f);
    displacement = maximum(displacement, height_change * 0.5f);
    float fast_threshold = maximum(20.0f, minimum(raw->width, raw->height) * 0.15f);
    float time_constant = displacement >= fast_threshold ? 8.0f : 75.0f;
    float alpha = (float)elapsed_ms / ((float)elapsed_ms + time_constant);
    slot->x = previous->x + alpha * ((float)raw->x - previous->x);
    slot->y = previous->y + alpha * ((float)raw->y - previous->y);
    slot->width = previous->width + alpha * ((float)raw->width - previous->width);
    slot->height = previous->height + alpha * ((float)raw->height - previous->height);
    slot->raw = *raw;
}

static void publish_slot(const app_detection_smoother_slot_t *slot, app_ui_detection_t *box)
{
    *box = slot->raw; /* Confidence and class always describe the current raw result. */
    box->x = (uint16_t)(slot->x + 0.5f);
    box->y = (uint16_t)(slot->y + 0.5f);
    box->width = (uint16_t)(slot->width + 0.5f);
    box->height = (uint16_t)(slot->height + 0.5f);
    if (box->x >= LCD_FG_WIDTH) { box->x = LCD_FG_WIDTH - 1U; }
    if (box->y >= LCD_FG_HEIGHT) { box->y = LCD_FG_HEIGHT - 1U; }
    if (box->width == 0U) { box->width = 1U; }
    if (box->height == 0U) { box->height = 1U; }
    if (box->width > LCD_FG_WIDTH - box->x) { box->width = (uint16_t)(LCD_FG_WIDTH - box->x); }
    if (box->height > LCD_FG_HEIGHT - box->y) { box->height = (uint16_t)(LCD_FG_HEIGHT - box->y); }
}

void app_detection_smoother_init(app_detection_smoother_t *state)
{
    app_detection_smoother_reset(state);
}

void app_detection_smoother_reset(app_detection_smoother_t *state)
{
    if (state != NULL) { memset(state, 0, sizeof(*state)); }
}

const app_vision_result_t *app_detection_smoother_update(
    app_detection_smoother_t *state, const app_vision_result_t *raw, uint8_t people_fresh)
{
    if (state == NULL) { return NULL; }
    if (raw == NULL || !raw->valid || !people_fresh || raw->count == 0U ||
        raw->count > APP_UI_MAX_DETECTIONS) {
        app_detection_smoother_reset(state);
        return &state->output;
    }

    app_ui_detection_t current[APP_UI_MAX_DETECTIONS];
    uint8_t count = 0U;
    for (unsigned int i = 0U; i < raw->count; i++) {
        if (sanitize(&raw->boxes[i], &current[count])) { count++; }
    }
    if (count == 0U) {
        app_detection_smoother_reset(state);
        return &state->output;
    }
    if (state->initialized && raw->frame_id == state->last_frame_id) { return &state->output; }

    uint32_t elapsed_ms = raw->captured_ms - state->last_captured_ms;
    uint8_t restart = (uint8_t)(!state->initialized ||
        elapsed_ms > APP_DETECTION_SMOOTHER_MAX_GAP_MS);
    float scores[APP_UI_MAX_DETECTIONS][APP_UI_MAX_DETECTIONS];
    uint8_t current_used[APP_UI_MAX_DETECTIONS] = {0};
    uint8_t previous_used[APP_UI_MAX_DETECTIONS] = {0};
    int8_t matched[APP_UI_MAX_DETECTIONS];
    memset(matched, -1, sizeof(matched));
    if (!restart) {
        for (unsigned int i = 0U; i < count; i++) {
            for (unsigned int j = 0U; j < state->output.count; j++) {
                scores[i][j] = correspondence_score(&current[i], &state->slots[j].raw);
            }
        }
        for (unsigned int pass = 0U; pass < count; pass++) {
            float best = 0.0f;
            unsigned int best_current = 0U, best_previous = 0U;
            for (unsigned int i = 0U; i < count; i++) {
                if (current_used[i]) { continue; }
                for (unsigned int j = 0U; j < state->output.count; j++) {
                    if (previous_used[j] || scores[i][j] <= 0.0f) { continue; }
                    uint8_t tied_before = (uint8_t)(scores[i][j] == best &&
                        (comes_before(&current[i], &current[best_current]) ||
                         (!comes_before(&current[best_current], &current[i]) &&
                          comes_before(&state->slots[j].raw, &state->slots[best_previous].raw))));
                    if (scores[i][j] > best || tied_before) {
                        best = scores[i][j]; best_current = i; best_previous = j;
                    }
                }
            }
            if (best <= 0.0f) { break; }
            matched[best_current] = (int8_t)best_previous;
            current_used[best_current] = previous_used[best_previous] = 1U;
        }
    }

    app_detection_smoother_slot_t next[APP_UI_MAX_DETECTIONS];
    memset(next, 0, sizeof(next));
    for (unsigned int i = 0U; i < count; i++) {
        if (matched[i] >= 0) {
            update_slot(&next[i], &state->slots[(unsigned int)matched[i]], &current[i], elapsed_ms);
        } else { start_slot(&next[i], &current[i]); }
    }
    memcpy(state->slots, next, sizeof(next));
    memset(&state->output, 0, sizeof(state->output));
    state->output.frame_id = raw->frame_id;
    state->output.captured_ms = raw->captured_ms;
    state->output.completed_ms = raw->completed_ms;
    state->output.count = count;
    state->output.valid = 1U;
    for (unsigned int i = 0U; i < count; i++) { publish_slot(&state->slots[i], &state->output.boxes[i]); }
    state->last_frame_id = raw->frame_id;
    state->last_captured_ms = raw->captured_ms;
    state->initialized = 1U;
    return &state->output;
}
