#include "audio_frame.h"

audio_frame_status_t audio_frame_validate(const audio_frame_t *frame)
{
    if (frame == 0) {
        return AUDIO_FRAME_INVALID;
    }
    if (frame->sample_rate_hz != AUDIO_FRAME_SAMPLE_RATE_HZ ||
        frame->sample_count != AUDIO_FRAME_SAMPLES ||
        frame->channel_count != AUDIO_FRAME_CHANNELS) {
        return AUDIO_FRAME_INVALID;
    }
    return AUDIO_FRAME_OK;
}
