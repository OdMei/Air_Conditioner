#include <assert.h>
#include <stddef.h>
#include "app_audio_pipeline.h"

static uint32_t kws_count;
static uint32_t tx_count;

static void on_kws(const audio_frame_t *frame, void *context)
{
    (void)context;
    assert(frame != NULL);
    kws_count++;
}

static void on_tx(const audio_frame_t *frame, void *context)
{
    (void)context;
    assert(frame != NULL);
    tx_count++;
}

int main(void)
{
    audio_frontend_t frontend;
    app_audio_pipeline_t pipeline;
    app_audio_pipeline_stats_t stats;
    audio_frame_t frame = {0};
    audio_frontend_config_t config;
    audio_frontend_default_config(&config);
    audio_frontend_init(&frontend, &config);
    frame.sample_rate_hz = AUDIO_FRAME_SAMPLE_RATE_HZ;
    frame.sample_count = AUDIO_FRAME_SAMPLES;
    frame.channel_count = AUDIO_FRAME_CHANNELS;
    assert(app_audio_pipeline_init(&pipeline, &frontend,
                                   on_kws, on_tx, NULL) ==
           APP_AUDIO_PIPELINE_OK);
    assert(app_audio_pipeline_submit(&pipeline, &frame) ==
           APP_AUDIO_PIPELINE_OK);
    assert(app_audio_pipeline_submit(&pipeline, &frame) ==
           APP_AUDIO_PIPELINE_OK);
    assert(app_audio_pipeline_submit(&pipeline, &frame) ==
           APP_AUDIO_PIPELINE_OK);
    app_audio_pipeline_get_stats(&pipeline, &stats);
    assert(stats.dropped_frames == 1U);
    assert(app_audio_pipeline_poll(&pipeline) == APP_AUDIO_PIPELINE_OK);
    assert(kws_count == 1U && tx_count == 1U);
    app_audio_pipeline_stop(&pipeline);
    assert(app_audio_pipeline_submit(&pipeline, &frame) ==
           APP_AUDIO_PIPELINE_STOPPED);
    return 0;
}
