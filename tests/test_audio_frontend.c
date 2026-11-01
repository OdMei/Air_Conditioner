#include <assert.h>
#include <stddef.h>
#include "audio_frontend.h"

static uint8_t dnn_passthrough(const int16_t *input, int16_t *output,
                               size_t count, void *context)
{
    size_t i;
    (void)context;
    for (i = 0U; i < count; ++i) {
        output[i] = input[i];
    }
    return 1U;
}

int main(void)
{
    audio_frontend_t frontend;
    audio_frontend_config_t config;
    audio_frontend_status_t status;
    audio_frame_t frame = {0};
    audio_frontend_default_config(&config);
    config.max_gain_db = 90U;
    assert(audio_frontend_init(&frontend, &config) == AUDIO_FRONTEND_OK);
    assert(frontend.config.max_gain_db == 60U);
    audio_frontend_set_dnn_callback(&frontend, dnn_passthrough, NULL);
    frame.sample_rate_hz = AUDIO_FRAME_SAMPLE_RATE_HZ;
    frame.sample_count = AUDIO_FRAME_SAMPLES;
    frame.channel_count = AUDIO_FRAME_CHANNELS;
    assert(audio_frontend_process(&frontend, &frame) == AUDIO_FRONTEND_OK);
    audio_frontend_get_status(&frontend, &status);
    assert(status.aec_bypassed == 1U);
    assert(status.doa_valid == 0U);
    assert(status.frames_processed == 1U);
    frame.reference_valid = 1U;
    frame.mic[0][0] = 1000;
    frame.mic[1][0] = 1000;
    assert(audio_frontend_process(&frontend, &frame) == AUDIO_FRONTEND_OK);
    audio_frontend_get_status(&frontend, &status);
    assert(status.dnn_active == 1U);
    return 0;
}
