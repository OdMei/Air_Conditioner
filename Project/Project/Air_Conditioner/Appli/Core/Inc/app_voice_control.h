#ifndef APP_VOICE_CONTROL_H
#define APP_VOICE_CONTROL_H

#include <stdint.h>
#include "app_climate_logic.h"

/* Offline recognizers can feed either a UTF-8 phrase or a keyword ID. */
typedef struct {
    uint16_t keyword_id;
    uint8_t confidence_percent;
    const char *phrase;
} app_voice_result_t;

typedef enum {
    APP_VOICE_UNKNOWN = 0,
    APP_VOICE_ACCEPTED,
    APP_VOICE_REJECTED_LOW_CONFIDENCE
} app_voice_status_t;

#define APP_VOICE_MIN_CONFIDENCE (60U)

app_voice_status_t app_voice_to_action(const app_voice_result_t *result,
                                       app_climate_action_t *action);

/* Convenience entry point for an ASR task that owns the climate state. */
app_voice_status_t app_voice_apply_result(app_climate_state_t *state,
                                           const app_voice_result_t *result);

#endif
