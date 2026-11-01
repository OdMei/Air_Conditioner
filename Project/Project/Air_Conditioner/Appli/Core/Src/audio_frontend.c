#include "audio_frontend.h"
#include <string.h>

static int32_t clamp_sample(int32_t value)
{
    if (value > INT16_MAX) {
        return INT16_MAX;
    }
    if (value < INT16_MIN) {
        return INT16_MIN;
    }
    return value;
}

static uint32_t sample_abs(int32_t value)
{
    return (value < 0) ? (uint32_t)(-value) : (uint32_t)value;
}

static uint32_t gain_limit_q8(uint8_t gain_db)
{
    uint32_t steps = (uint32_t)(gain_db / 6U);
    uint32_t gain = AUDIO_FRONTEND_Q8_ONE;
    while (steps != 0U && gain <= (UINT32_MAX / 2U)) {
        gain *= 2U;
        steps--;
    }
    return gain;
}

void audio_frontend_default_config(audio_frontend_config_t *config)
{
    if (config == 0) {
        return;
    }
    config->aec_mu_q15 = 8192U;
    config->aec_leak_q15 = 32700U;
    config->beam_delay_samples = 0;
    config->dereverb_feedback_q8 = 32U;
    config->noise_floor_initial = 500U;
    config->agc_target_peak = 10000U;
    config->agc_attack_q8 = 64U;
    config->agc_release_q8 = 8U;
    config->max_gain_db = AUDIO_FRONTEND_MAX_GAIN_DB;
    config->doa_min_energy = 2000U;
}

audio_frontend_status_code_t audio_frontend_init(
    audio_frontend_t *frontend,
    const audio_frontend_config_t *config)
{
    audio_frontend_config_t defaults;
    if (frontend == 0) {
        return AUDIO_FRONTEND_INVALID;
    }
    audio_frontend_default_config(&defaults);
    memset(frontend, 0, sizeof(*frontend));
    frontend->config = (config == 0) ? defaults : *config;
    if (frontend->config.max_gain_db > AUDIO_FRONTEND_MAX_GAIN_DB) {
        frontend->config.max_gain_db = AUDIO_FRONTEND_MAX_GAIN_DB;
    }
    if (frontend->config.agc_attack_q8 == 0U) {
        frontend->config.agc_attack_q8 = 1U;
    }
    if (frontend->config.agc_release_q8 == 0U) {
        frontend->config.agc_release_q8 = 1U;
    }
    frontend->noise_floor = frontend->config.noise_floor_initial;
    frontend->agc_gain_q8 = AUDIO_FRONTEND_Q8_ONE;
    return AUDIO_FRONTEND_OK;
}

void audio_frontend_set_dnn_callback(audio_frontend_t *frontend,
                                      audio_frontend_dnn_cb_t callback,
                                      void *context)
{
    if (frontend == 0) {
        return;
    }
    frontend->dnn_callback = callback;
    frontend->dnn_context = context;
}

static int32_t aec_sample(audio_frontend_t *frontend,
                          int32_t mic,
                          int32_t reference,
                          uint8_t update)
{
    int32_t estimate = 0;
    int32_t error;
    uint32_t denominator = 256U;
    uint32_t index;
    for (index = AUDIO_FRONTEND_AEC_ORDER - 1U; index > 0U; index--) {
        frontend->reference_history[index] =
            frontend->reference_history[index - 1U];
    }
    frontend->reference_history[0] = (int16_t)clamp_sample(reference);
    for (index = 0U; index < AUDIO_FRONTEND_AEC_ORDER; index++) {
        estimate += (frontend->aec_coeff_q15[index] *
                     frontend->reference_history[index]) >> 15;
        denominator += (uint32_t)(frontend->reference_history[index] *
                                  frontend->reference_history[index]) >> 12;
    }
    error = mic - estimate;
    if (update != 0U && reference != 0) {
        for (index = 0U; index < AUDIO_FRONTEND_AEC_ORDER; index++) {
            int32_t delta = (error * frontend->reference_history[index]);
            delta = (delta / (int32_t)denominator) *
                    (int32_t)frontend->config.aec_mu_q15;
            delta >>= 8;
            frontend->aec_coeff_q15[index] = (int16_t)clamp_sample(
                ((frontend->aec_coeff_q15[index] *
                  frontend->config.aec_leak_q15) >> 15) + delta);
        }
    }
    return error;
}

static int32_t beam_sample(const audio_frontend_t *frontend,
                           const audio_frame_t *frame,
                           uint32_t index)
{
    int32_t second_index = (int32_t)index -
                           (int32_t)frontend->config.beam_delay_samples;
    if (second_index < 0) {
        second_index = 0;
    }
    if (second_index >= (int32_t)AUDIO_FRAME_SAMPLES) {
        second_index = AUDIO_FRAME_SAMPLES - 1;
    }
    return ((int32_t)frame->mic[0][index] +
            (int32_t)frame->mic[1][(uint32_t)second_index]) / 2;
}

static int32_t suppress_noise(audio_frontend_t *frontend, int32_t value)
{
    uint32_t magnitude = sample_abs(value);
    if (magnitude < frontend->noise_floor) {
        frontend->noise_floor = (uint16_t)((frontend->noise_floor * 15U +
                                            magnitude) / 16U);
        return value / 2;
    }
    if (magnitude < ((uint32_t)frontend->noise_floor * 2U)) {
        return value / 2;
    }
    return value;
}

static void apply_agc(audio_frontend_t *frontend,
                      int16_t *samples,
                      uint32_t peak)
{
    uint32_t desired = frontend->agc_gain_q8;
    uint32_t maximum = gain_limit_q8(frontend->config.max_gain_db);
    uint32_t index;
    if (peak != 0U) {
        desired = ((uint32_t)frontend->config.agc_target_peak * 256U) /
                  peak;
        if (desired > maximum) {
            desired = maximum;
        }
        if (desired > frontend->agc_gain_q8) {
            frontend->agc_gain_q8 += (desired - frontend->agc_gain_q8) *
                                     frontend->config.agc_release_q8 / 256U;
        } else {
            frontend->agc_gain_q8 -= (frontend->agc_gain_q8 - desired) *
                                     frontend->config.agc_attack_q8 / 256U;
        }
    }
    if (frontend->agc_gain_q8 > maximum) {
        frontend->agc_gain_q8 = maximum;
    }
    for (index = 0U; index < AUDIO_FRAME_SAMPLES; index++) {
        int32_t scaled = ((int32_t)samples[index] *
                          (int32_t)frontend->agc_gain_q8) >> 8;
        samples[index] = (int16_t)clamp_sample(scaled);
    }
    frontend->status.agc_gain_db = (uint8_t)((frontend->agc_gain_q8 /
                                               256U) * 6U);
    if (frontend->status.agc_gain_db > AUDIO_FRONTEND_MAX_GAIN_DB) {
        frontend->status.agc_gain_db = AUDIO_FRONTEND_MAX_GAIN_DB;
    }
}

static void estimate_doa(audio_frontend_t *frontend,
                         const audio_frame_t *frame)
{
    int64_t best = 0;
    int64_t energy = 0;
    int32_t best_lag = 0;
    uint32_t lag;
    uint32_t index;
    for (index = 0U; index < AUDIO_FRAME_SAMPLES; index++) {
        energy += sample_abs(frame->mic[0][index]);
        energy += sample_abs(frame->mic[1][index]);
    }
    frontend->status.doa_valid = 0U;
    frontend->status.doa_degrees = 0;
    frontend->status.doa_confidence = 0U;
    if (energy < frontend->config.doa_min_energy) {
        return;
    }
    for (lag = 0U; lag <= AUDIO_FRONTEND_MAX_LAG * 2U; lag++) {
        int32_t signed_lag = (int32_t)lag - AUDIO_FRONTEND_MAX_LAG;
        int64_t correlation = 0;
        for (index = 0U; index < AUDIO_FRAME_SAMPLES; index++) {
            int32_t second = (int32_t)index + signed_lag;
            if (second >= 0 && second < (int32_t)AUDIO_FRAME_SAMPLES) {
                correlation += (int32_t)frame->mic[0][index] *
                               frame->mic[1][(uint32_t)second];
            }
        }
        if (correlation < 0) {
            correlation = -correlation;
        }
        if (correlation > best) {
            best = correlation;
            best_lag = signed_lag;
        }
    }
    if (best < energy * 1000) {
        return;
    }
    frontend->status.doa_valid = 1U;
    frontend->status.doa_degrees = (int16_t)(best_lag * 15);
    frontend->status.doa_confidence = 100U;
}

static void process_samples(audio_frontend_t *frontend,
                            audio_frame_t *frame,
                            int16_t *work)
{
    int16_t dnn_output[AUDIO_FRAME_SAMPLES];
    uint32_t index;
    uint32_t peak = 0U;
    uint8_t update_aec = (frame->reference_valid != 0U) ? 1U : 0U;
    frontend->status.aec_bypassed = (update_aec == 0U) ? 1U : 0U;
    for (index = 0U; index < AUDIO_FRAME_SAMPLES; index++) {
        int32_t sample = beam_sample(frontend, frame, index);
        if (update_aec != 0U) {
            sample = aec_sample(frontend, sample, frame->reference[index],
                                (sample_abs(sample) < 16000U) ? 1U : 0U);
        }
        sample -= ((int32_t)frontend->previous_output[index] *
                   frontend->config.dereverb_feedback_q8) >> 8;
        sample = suppress_noise(frontend, sample);
        work[index] = (int16_t)clamp_sample(sample);
        if (sample_abs(work[index]) > peak) {
            peak = sample_abs(work[index]);
        }
    }
    frontend->status.dnn_active = 0U;
    if (frontend->dnn_callback != 0) {
        if (frontend->dnn_callback(work, dnn_output, AUDIO_FRAME_SAMPLES,
                                   frontend->dnn_context) != 0U) {
            memcpy(work, dnn_output, sizeof(work[0]) * AUDIO_FRAME_SAMPLES);
            frontend->status.dnn_active = 1U;
        } else {
            frontend->status.dnn_fallback_count++;
        }
    }
    apply_agc(frontend, work, peak);
    for (index = 0U; index < AUDIO_FRAME_SAMPLES; index++) {
        frame->output[index] = work[index];
        frontend->previous_output[index] = work[index];
    }
}

audio_frontend_status_code_t audio_frontend_process(
    audio_frontend_t *frontend,
    audio_frame_t *frame)
{
    int16_t work[AUDIO_FRAME_SAMPLES];
    if (frontend == 0 || frame == 0 ||
        audio_frame_validate(frame) != AUDIO_FRAME_OK) {
        if (frontend != 0) {
            frontend->status.invalid_frames++;
        }
        return AUDIO_FRONTEND_INVALID;
    }
    process_samples(frontend, frame, work);
    estimate_doa(frontend, frame);
    frontend->status.frames_processed++;
    return (frontend->status.dnn_active != 0U ||
            frontend->dnn_callback == 0) ? AUDIO_FRONTEND_OK :
           AUDIO_FRONTEND_DNN_ERROR;
}

void audio_frontend_get_status(const audio_frontend_t *frontend,
                               audio_frontend_status_t *status)
{
    if (frontend == 0 || status == 0) {
        return;
    }
    *status = frontend->status;
}
