#ifndef AUDIO_FRONTEND_CONFIG_H
#define AUDIO_FRONTEND_CONFIG_H

#include <stdint.h>

#define AUDIO_FRONTEND_AEC_ORDER (16U)
#define AUDIO_FRONTEND_MAX_LAG (8U)
#define AUDIO_FRONTEND_MAX_GAIN_DB (60U)
#define AUDIO_FRONTEND_Q8_ONE (256U)

typedef struct {
    uint16_t aec_mu_q15;
    uint16_t aec_leak_q15;
    int8_t beam_delay_samples;
    uint8_t dereverb_feedback_q8;
    uint16_t noise_floor_initial;
    uint16_t agc_target_peak;
    uint8_t agc_attack_q8;
    uint8_t agc_release_q8;
    uint8_t max_gain_db;
    uint16_t doa_min_energy;
} audio_frontend_config_t;

#endif
