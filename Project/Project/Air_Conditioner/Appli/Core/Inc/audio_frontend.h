#ifndef AUDIO_FRONTEND_H
#define AUDIO_FRONTEND_H

#include <stddef.h>
#include <stdint.h>
#include "audio_frame.h"
#include "audio_frontend_config.h"

typedef enum {
    AUDIO_FRONTEND_OK = 0,
    AUDIO_FRONTEND_INVALID = 1,
    AUDIO_FRONTEND_DNN_ERROR = 2
} audio_frontend_status_code_t;

typedef uint8_t (*audio_frontend_dnn_cb_t)(const int16_t *input,
                                           int16_t *output,
                                           size_t sample_count,
                                           void *context);

typedef struct {
    uint8_t aec_bypassed;
    uint8_t dnn_active;
    uint8_t doa_valid;
    int16_t doa_degrees;
    uint8_t doa_confidence;
    uint8_t agc_gain_db;
    uint32_t frames_processed;
    uint32_t invalid_frames;
    uint32_t dnn_fallback_count;
} audio_frontend_status_t;

typedef struct {
    audio_frontend_config_t config;
    audio_frontend_status_t status;
    audio_frontend_dnn_cb_t dnn_callback;
    void *dnn_context;
    int16_t aec_coeff_q15[AUDIO_FRONTEND_AEC_ORDER];
    int16_t reference_history[AUDIO_FRONTEND_AEC_ORDER];
    int16_t previous_output[AUDIO_FRAME_SAMPLES];
    uint16_t noise_floor;
    uint32_t agc_gain_q8;
} audio_frontend_t;

void audio_frontend_default_config(audio_frontend_config_t *config);
audio_frontend_status_code_t audio_frontend_init(
    audio_frontend_t *frontend,
    const audio_frontend_config_t *config);
void audio_frontend_set_dnn_callback(audio_frontend_t *frontend,
                                      audio_frontend_dnn_cb_t callback,
                                      void *context);
audio_frontend_status_code_t audio_frontend_process(
    audio_frontend_t *frontend,
    audio_frame_t *frame);
void audio_frontend_get_status(const audio_frontend_t *frontend,
                               audio_frontend_status_t *status);

#endif
