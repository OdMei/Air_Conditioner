#ifndef APP_AUDIO_PIPELINE_H
#define APP_AUDIO_PIPELINE_H

#include <stdint.h>
#include "audio_frontend.h"

#define APP_AUDIO_PIPELINE_QUEUE_DEPTH (2U)

typedef enum {
    APP_AUDIO_PIPELINE_OK = 0,
    APP_AUDIO_PIPELINE_INVALID = 1,
    APP_AUDIO_PIPELINE_EMPTY = 2,
    APP_AUDIO_PIPELINE_STOPPED = 3,
    APP_AUDIO_PIPELINE_PROCESS_ERROR = 4
} app_audio_pipeline_status_t;

typedef void (*app_audio_pipeline_sink_t)(const audio_frame_t *frame,
                                          void *context);

typedef struct {
    uint32_t submitted_frames;
    uint32_t processed_frames;
    uint32_t dropped_frames;
    uint32_t processing_errors;
    uint8_t queued_frames;
    uint8_t running;
} app_audio_pipeline_stats_t;

typedef struct {
    audio_frontend_t *frontend;
    app_audio_pipeline_sink_t kws_sink;
    app_audio_pipeline_sink_t tx_sink;
    void *sink_context;
    audio_frame_t queue[APP_AUDIO_PIPELINE_QUEUE_DEPTH];
    volatile uint8_t queued_frames;
    uint8_t read_index;
    uint8_t write_index;
    uint8_t running;
    uint32_t submitted_frames;
    uint32_t processed_frames;
    uint32_t dropped_frames;
    uint32_t processing_errors;
} app_audio_pipeline_t;

app_audio_pipeline_status_t app_audio_pipeline_init(
    app_audio_pipeline_t *pipeline,
    audio_frontend_t *frontend,
    app_audio_pipeline_sink_t kws_sink,
    app_audio_pipeline_sink_t tx_sink,
    void *sink_context);
void app_audio_pipeline_stop(app_audio_pipeline_t *pipeline);
app_audio_pipeline_status_t app_audio_pipeline_submit(
    app_audio_pipeline_t *pipeline,
    const audio_frame_t *frame);
app_audio_pipeline_status_t app_audio_pipeline_poll(
    app_audio_pipeline_t *pipeline);
void app_audio_pipeline_get_stats(
    const app_audio_pipeline_t *pipeline,
    app_audio_pipeline_stats_t *stats);

#endif
