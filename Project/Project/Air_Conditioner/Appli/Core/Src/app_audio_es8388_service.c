#include "app_audio_es8388_service.h"
#include <string.h>

app_audio_es8388_status_t app_audio_es8388_service_init(
    app_audio_es8388_service_t *service,
    app_voice_kws_t *kws)
{
    audio_frontend_config_t config;
    if (service == 0) {
        return APP_AUDIO_ES8388_INVALID;
    }
    memset(service, 0, sizeof(*service));
    audio_frontend_default_config(&config);
    if (audio_frontend_init(&service->frontend, &config) !=
        AUDIO_FRONTEND_OK) {
        return APP_AUDIO_ES8388_ERROR;
    }
    service->kws = kws;
    return APP_AUDIO_ES8388_OK;
}

app_audio_es8388_status_t app_audio_es8388_service_unpack_stereo(
    const int16_t *interleaved,
    size_t frame_count,
    audio_frame_t *frame)
{
    size_t index;
    if (interleaved == 0 || frame == 0 ||
        frame_count != AUDIO_FRAME_SAMPLES) {
        return APP_AUDIO_ES8388_INVALID;
    }
    memset(frame, 0, sizeof(*frame));
    frame->sample_rate_hz = AUDIO_FRAME_SAMPLE_RATE_HZ;
    frame->sample_count = AUDIO_FRAME_SAMPLES;
    frame->channel_count = AUDIO_FRAME_CHANNELS;
    for (index = 0U; index < AUDIO_FRAME_SAMPLES; index++) {
        frame->mic[0][index] = interleaved[index * 2U];
        frame->mic[1][index] = interleaved[index * 2U + 1U];
    }
    return APP_AUDIO_ES8388_OK;
}

#if APP_AUDIO_ES8388_SERVICE_ENABLE

#include "es8388.h"
#include "es8388_sai.h"

#define ES8388_SERVICE_DMA_SAMPLES (AUDIO_FRAME_SAMPLES * 2U)

static app_audio_es8388_service_t *s_service;
static int16_t s_rx_dma[2][ES8388_SERVICE_DMA_SAMPLES];
static int16_t s_tx_dma[ES8388_SERVICE_DMA_SAMPLES];
static int16_t s_reference[AUDIO_FRAME_SAMPLES];
static volatile uint8_t s_rx_ready;
static uint8_t s_rx_active;
static volatile uint8_t s_tx_busy;
static uint8_t s_reference_valid;

static void rx_complete(void)
{
    s_rx_ready = (uint8_t)(s_rx_active + 1U);
}

static void tx_complete(void)
{
    s_tx_busy = 0U;
}

static void kws_sink(const audio_frame_t *frame, void *context)
{
    app_audio_es8388_service_t *service =
        (app_audio_es8388_service_t *)context;
    if (service->kws != 0) {
        app_voice_kws_feed(service->kws, frame->output,
                           AUDIO_FRAME_SAMPLES);
    }
}

static void tx_sink(const audio_frame_t *frame, void *context)
{
    app_audio_es8388_service_t *service =
        (app_audio_es8388_service_t *)context;
    size_t index;
    if (s_tx_busy != 0U) {
        service->tx_drops++;
        return;
    }
    for (index = 0U; index < AUDIO_FRAME_SAMPLES; index++) {
        s_tx_dma[index * 2U] = frame->output[index];
        s_tx_dma[index * 2U + 1U] = frame->output[index];
        s_reference[index] = frame->output[index];
    }
    if (es8388_sai_tx_dma((uint8_t *)s_tx_dma,
                          ES8388_SERVICE_DMA_SAMPLES) == 0U) {
        s_tx_busy = 1U;
        s_reference_valid = 1U;
    } else {
        service->tx_drops++;
    }
}

app_audio_es8388_status_t app_audio_es8388_service_start(
    app_audio_es8388_service_t *service)
{
    if (service == 0) {
        return APP_AUDIO_ES8388_INVALID;
    }
    if (es8388_init() != 0U ||
        es8388_sai_clock_config(AUDIO_FRAME_SAMPLE_RATE_HZ) != 0U ||
        es8388_sai_init_rx(AUDIO_FRAME_CHANNELS,
                           AUDIO_FRAME_SAMPLE_RATE_HZ, 16U) != 0U ||
        es8388_sai_init_tx(AUDIO_FRAME_CHANNELS,
                           AUDIO_FRAME_SAMPLE_RATE_HZ, 16U) != 0U) {
        return APP_AUDIO_ES8388_ERROR;
    }
    if (app_audio_pipeline_init(&service->pipeline, &service->frontend,
                                kws_sink, tx_sink, service) !=
        APP_AUDIO_PIPELINE_OK) {
        return APP_AUDIO_ES8388_ERROR;
    }
    s_service = service;
    s_rx_active = 0U;
    s_rx_ready = 0U;
    s_tx_busy = 0U;
    s_reference_valid = 0U;
    es8388_sai_rx_dma_cplt_user_cb_register(rx_complete);
    es8388_sai_tx_dma_cplt_user_cb_register(tx_complete);
    if (es8388_sai_rx_dma((uint8_t *)s_rx_dma[0],
                          ES8388_SERVICE_DMA_SAMPLES) != 0U) {
        return APP_AUDIO_ES8388_ERROR;
    }
    service->running = 1U;
    return APP_AUDIO_ES8388_OK;
}

void app_audio_es8388_service_poll(app_audio_es8388_service_t *service)
{
    audio_frame_t frame;
    uint8_t ready;
    uint8_t completed;
    if (service == 0 || service->running == 0U || s_service != service) {
        return;
    }
    ready = s_rx_ready;
    if (ready == 0U) {
        (void)app_audio_pipeline_poll(&service->pipeline);
        return;
    }
    s_rx_ready = 0U;
    completed = (uint8_t)(ready - 1U);
    if (app_audio_es8388_service_unpack_stereo(
            s_rx_dma[completed], AUDIO_FRAME_SAMPLES, &frame) !=
        APP_AUDIO_ES8388_OK) {
        return;
    }
    memcpy(frame.reference, s_reference, sizeof(s_reference));
    frame.reference_valid = s_reference_valid;
    service->frames_received++;
    if (app_audio_pipeline_submit(&service->pipeline, &frame) ==
        APP_AUDIO_PIPELINE_OK) {
        service->frames_submitted++;
    }
    s_rx_active = (uint8_t)(completed ^ 1U);
    (void)es8388_sai_rx_dma((uint8_t *)s_rx_dma[s_rx_active],
                            ES8388_SERVICE_DMA_SAMPLES);
    (void)app_audio_pipeline_poll(&service->pipeline);
}

void app_audio_es8388_service_stop(app_audio_es8388_service_t *service)
{
    if (service == 0) {
        return;
    }
    service->running = 0U;
    s_reference_valid = 0U;
    app_audio_pipeline_stop(&service->pipeline);
    if (s_service == service) {
        s_service = 0;
    }
}

#else

app_audio_es8388_status_t app_audio_es8388_service_start(
    app_audio_es8388_service_t *service)
{
    (void)service;
    return APP_AUDIO_ES8388_DISABLED;
}

void app_audio_es8388_service_poll(app_audio_es8388_service_t *service)
{
    (void)service;
}

void app_audio_es8388_service_stop(app_audio_es8388_service_t *service)
{
    if (service != 0) {
        service->running = 0U;
    }
}

#endif
