#ifndef APP_BG_BUFFERS_H
#define APP_BG_BUFFERS_H

#include <stdbool.h>
#include <stdint.h>

#define APP_BG_BUFFER_NONE UINT8_MAX

typedef enum
{
    APP_BG_PREPARED_NONE = 0,
    APP_BG_PREPARED_CAPTURE_TARGET,
    APP_BG_PREPARED_RELOAD
} app_bg_prepared_phase_t;

/* HAL-free ownership bookkeeping. Serialize every operation in the camera
 * interrupt or a short interrupt critical section. A completed frame is not
 * display-owned until DMA has successfully moved away from its buffer. */
typedef struct
{
    uint8_t count;
    uint8_t front;
    uint8_t fill;
    uint8_t pending;
    uint8_t prepared;
    uint8_t next_fill;
    uint8_t probe;
    uint8_t running;
    app_bg_prepared_phase_t phase;
    uint32_t front_frame_id;
    uint32_t pending_frame_id;
    uint32_t prepared_frame_id;
    uint32_t captures;
    uint32_t submissions;
    uint32_t presentations;
    uint32_t dropped;
    uint32_t capture_target_failures;
    uint32_t reload_failures;
} app_bg_buffers_t;

typedef struct
{
    uint8_t completed_index;
    uint8_t next_fill;
    uint8_t need_retarget;
    uint32_t frame_id;
} app_bg_capture_plan_t;

/* Initialization requires DMA stopped and the specified front already owned
 * by scanout, with no outstanding LTDC reload. Never use init to time out a
 * hardware reload. At least three buffers are required. */
bool app_bg_buffers_init(app_bg_buffers_t *state, uint8_t count,
                         uint8_t front, uint8_t fill);

/* Pass the actual layer-specific VBR bit, after an accepted reload request.
 * A pending transfer stays quarantined for arbitrarily long VBlank delays. */
bool app_bg_buffers_observe_reload(app_bg_buffers_t *state, bool vbr_active);

/* On no free slot / outstanding hand-off, capture continues in the same fill
 * buffer. The caller must retarget DCMIPP before requesting an LTDC reload. */
bool app_bg_buffers_frame_complete(app_bg_buffers_t *state, uint32_t frame_id,
                                  app_bg_capture_plan_t *plan);
bool app_bg_buffers_capture_target_result(app_bg_buffers_t *state,
                                         uint8_t next_fill, bool success);

/* A failed HAL submission keeps the candidate quarantined, including when
 * SetAddress_NoReload succeeded first. Retrying the same candidate is safe;
 * a failed attempt must never be interpreted as completed scanout. */
bool app_bg_buffers_candidate(const app_bg_buffers_t *state,
                              uint8_t *index, uint32_t *frame_id);
bool app_bg_buffers_submission_result(app_bg_buffers_t *state, bool accepted);

/* Call stop only after camera DMA is actually stopped. This preserves all
 * LTDC ownership and failed-submission quarantine across camera recovery. */
void app_bg_buffers_stop(app_bg_buffers_t *state);
bool app_bg_buffers_restart(app_bg_buffers_t *state);

/* Pin confirmed scanout for the thread-context nine-pixel diagnostic probe.
 * It can become an old front during sampling, but cannot become a DMA target
 * until released. Never hold the pin across a blocking operation. */
bool app_bg_buffers_pin_front(app_bg_buffers_t *state,
                              uint8_t *index, uint32_t *frame_id);
bool app_bg_buffers_release_probe(app_bg_buffers_t *state, uint8_t index);

/* Useful for host model tests and assertions; no hardware side effects. */
bool app_bg_buffers_valid(const app_bg_buffers_t *state);

#endif
