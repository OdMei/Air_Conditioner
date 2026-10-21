#ifndef APP_DETECTION_SMOOTHER_H
#define APP_DETECTION_SMOOTHER_H

#include "app_ui.h"

#define APP_DETECTION_SMOOTHER_MAX_GAP_MS (250UL)

/* Display-only, ephemeral correspondence. These slots are neither persistent
 * person identities nor input to occupancy/control decisions. */
typedef struct
{
    float x, y, width, height;
    app_ui_detection_t raw;
} app_detection_smoother_slot_t;

typedef struct
{
    app_detection_smoother_slot_t slots[APP_UI_MAX_DETECTIONS];
    app_vision_result_t output;
    uint32_t last_frame_id;
    uint32_t last_captured_ms;
    uint8_t initialized;
} app_detection_smoother_t;

void app_detection_smoother_init(app_detection_smoother_t *state);
void app_detection_smoother_reset(app_detection_smoother_t *state);
/* Serialized UI caller only. The returned state-owned result stays valid
 * until update/reset. raw is never modified. Invalid/unfresh/empty reports
 * remove every drawing immediately, even if frame_id repeats. Repeated valid
 * frame IDs leave the output unchanged. Captured timestamps drive filtering;
 * long/backward gaps restart from the current geometry. Lost boxes are never
 * retained. Invalid geometry is omitted only from this drawing result. */
const app_vision_result_t *app_detection_smoother_update(
    app_detection_smoother_t *state,
    const app_vision_result_t *raw,
    uint8_t people_fresh);

#endif /* APP_DETECTION_SMOOTHER_H */
