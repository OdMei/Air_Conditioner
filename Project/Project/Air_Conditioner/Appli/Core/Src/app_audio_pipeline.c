#include "app_audio_pipeline.h"
#include <string.h>

app_audio_pipeline_status_t app_audio_pipeline_init(
    app_audio_pipeline_t *pipeline,
    audio_frontend_t *frontend,
    app_audio_pipeline_sink_t kws_sink,
    app_audio_pipeline_sink_t tx_sink,
    void *sink_context)
{
    if (pipeline == 0 || frontend == 0) {
        return APP_AUDIO_PIPELINE_INVALID;
    }
    memset(pipeline, 0, sizeof(*pipeline));
    pipeline->frontend = frontend;
    pipeline->kws_sink = kws_sink;
    pipeline->tx_sink = tx_sink;
    pipeline->sink_context = sink_context;
    pipeline->running = 1U;
    return APP_AUDIO_PIPELINE_OK;
}

void app_audio_pipeline_stop(app_audio_pipeline_t *pipeline)
{
    if (pipeline == 0) {
        return;
    }
    pipeline->running = 0U;
    pipeline->queued_frames = 0U;
    pipeline->read_index = 0U;
    pipeline->write_index = 0U;
}

static void advance_index(uint8_t *index)
{
    *index = (uint8_t)((*index + 1U) % APP_AUDIO_PIPELINE_QUEUE_DEPTH);
}

app_audio_pipeline_status_t app_audio_pipeline_submit(
    app_audio_pipeline_t *pipeline,
    const audio_frame_t *frame)
{
    if (pipeline == 0 || frame == 0 || pipeline->frontend == 0) {
        return APP_AUDIO_PIPELINE_INVALID;
    }
    if (pipeline->running == 0U) {
        return APP_AUDIO_PIPELINE_STOPPED;
    }
    if (audio_frame_validate(frame) != AUDIO_FRAME_OK) {
        return APP_AUDIO_PIPELINE_INVALID;
    }
    if (pipeline->queued_frames >= APP_AUDIO_PIPELINE_QUEUE_DEPTH) {
        advance_index(&pipeline->read_index);
        pipeline->queued_frames--;
        pipeline->dropped_frames++;
    }
    memcpy(&pipeline->queue[pipeline->write_index], frame,
           sizeof(audio_frame_t));
    advance_index(&pipeline->write_index);
    pipeline->queued_frames++;
    pipeline->submitted_frames++;
    return APP_AUDIO_PIPELINE_OK;
}

app_audio_pipeline_status_t app_audio_pipeline_poll(
    app_audio_pipeline_t *pipeline)
{
    audio_frame_t *frame;
    audio_frontend_status_code_t result;
    if (pipeline == 0 || pipeline->frontend == 0) {
        return APP_AUDIO_PIPELINE_INVALID;
    }
    if (pipeline->running == 0U) {
        return APP_AUDIO_PIPELINE_STOPPED;
    }
    if (pipeline->queued_frames == 0U) {
        return APP_AUDIO_PIPELINE_EMPTY;
    }
    frame = &pipeline->queue[pipeline->read_index];
    pipeline->read_index = (uint8_t)((pipeline->read_index + 1U) %
                                      APP_AUDIO_PIPELINE_QUEUE_DEPTH);
    pipeline->queued_frames--;
    result = audio_frontend_process(pipeline->frontend, frame);
    if (result == AUDIO_FRONTEND_INVALID) {
        pipeline->processing_errors++;
        return APP_AUDIO_PIPELINE_PROCESS_ERROR;
    }
    if (pipeline->kws_sink != 0) {
        pipeline->kws_sink(frame, pipeline->sink_context);
    }
    if (pipeline->tx_sink != 0) {
        pipeline->tx_sink(frame, pipeline->sink_context);
    }
    pipeline->processed_frames++;
    return APP_AUDIO_PIPELINE_OK;
}

void app_audio_pipeline_get_stats(
    const app_audio_pipeline_t *pipeline,
    app_audio_pipeline_stats_t *stats)
{
    if (pipeline == 0 || stats == 0) {
        return;
    }
    stats->submitted_frames = pipeline->submitted_frames;
    stats->processed_frames = pipeline->processed_frames;
    stats->dropped_frames = pipeline->dropped_frames;
    stats->processing_errors = pipeline->processing_errors;
    stats->queued_frames = pipeline->queued_frames;
    stats->running = pipeline->running;
}
