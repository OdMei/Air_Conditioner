#ifndef APP_VOICE_KWS_H
#define APP_VOICE_KWS_H

#include <stdint.h>
#include <stddef.h>
#include "app_voice_control.h"

#define APP_VOICE_SAMPLE_RATE_HZ (16000U)
#define APP_VOICE_WINDOW_SAMPLES (16000U)
#define APP_VOICE_HOP_SAMPLES (1600U)

typedef uint8_t (*app_voice_kws_infer_cb_t)(const int16_t *samples,
                                            size_t sample_count,
                                            uint16_t *keyword_id,
                                            uint8_t *confidence_percent);

typedef struct {
    int16_t window[APP_VOICE_WINDOW_SAMPLES];
    size_t filled;
    app_climate_state_t *climate;
    app_voice_kws_infer_cb_t infer;
    uint32_t windows_processed;
    uint32_t commands_accepted;
    uint32_t commands_rejected;
} app_voice_kws_t;

void app_voice_kws_init(app_voice_kws_t *kws,
                        app_climate_state_t *climate,
                        app_voice_kws_infer_cb_t infer);
void app_voice_kws_feed(app_voice_kws_t *kws,
                        const int16_t *samples,
                        size_t sample_count);

#endif
