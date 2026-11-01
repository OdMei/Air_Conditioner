#ifndef AUDIO_FRAME_H
#define AUDIO_FRAME_H

#include <stdint.h>

#define AUDIO_FRAME_SAMPLE_RATE_HZ (16000U)
#define AUDIO_FRAME_SAMPLES (160U)
#define AUDIO_FRAME_CHANNELS (2U)

typedef enum {
    AUDIO_FRAME_OK = 0,
    AUDIO_FRAME_INVALID = 1
} audio_frame_status_t;

typedef struct {
    uint32_t timestamp_ms;
    uint32_t sample_rate_hz;
    uint16_t sample_count;
    uint8_t channel_count;
    uint8_t reference_valid;
    int16_t mic[AUDIO_FRAME_CHANNELS][AUDIO_FRAME_SAMPLES];
    int16_t reference[AUDIO_FRAME_SAMPLES];
    int16_t output[AUDIO_FRAME_SAMPLES];
} audio_frame_t;

audio_frame_status_t audio_frame_validate(const audio_frame_t *frame);

#endif
