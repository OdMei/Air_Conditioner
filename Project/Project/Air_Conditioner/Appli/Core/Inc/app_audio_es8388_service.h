#ifndef APP_AUDIO_ES8388_SERVICE_H
#define APP_AUDIO_ES8388_SERVICE_H

#include <stddef.h>
#include <stdint.h>
#include "app_audio_pipeline.h"
#include "app_voice_kws.h"

#ifndef APP_AUDIO_ES8388_SERVICE_ENABLE
#define APP_AUDIO_ES8388_SERVICE_ENABLE (0U)
#endif

typedef enum {
    APP_AUDIO_ES8388_OK = 0,
    APP_AUDIO_ES8388_INVALID = 1,
    APP_AUDIO_ES8388_DISABLED = 2,
    APP_AUDIO_ES8388_ERROR = 3
} app_audio_es8388_status_t;

typedef struct {
    audio_frontend_t frontend;
    app_audio_pipeline_t pipeline;
    app_voice_kws_t *kws;
    uint32_t frames_received;
    uint32_t frames_submitted;
    uint32_t tx_drops;
    uint8_t running;
} app_audio_es8388_service_t;

app_audio_es8388_status_t app_audio_es8388_service_init(
    app_audio_es8388_service_t *service,
    app_voice_kws_t *kws);
app_audio_es8388_status_t app_audio_es8388_service_start(
    app_audio_es8388_service_t *service);
void app_audio_es8388_service_poll(app_audio_es8388_service_t *service);
void app_audio_es8388_service_stop(app_audio_es8388_service_t *service);
app_audio_es8388_status_t app_audio_es8388_service_unpack_stereo(
    const int16_t *interleaved,
    size_t frame_count,
    audio_frame_t *frame);

#endif
