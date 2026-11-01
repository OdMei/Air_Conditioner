#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include "audio_frame.h"

int main(void)
{
    audio_frame_t frame = {0};
    frame.sample_rate_hz = AUDIO_FRAME_SAMPLE_RATE_HZ;
    frame.sample_count = AUDIO_FRAME_SAMPLES;
    frame.channel_count = AUDIO_FRAME_CHANNELS;
    frame.reference_valid = 1U;
    assert(audio_frame_validate(&frame) == AUDIO_FRAME_OK);
    frame.sample_count = AUDIO_FRAME_SAMPLES - 1U;
    assert(audio_frame_validate(&frame) == AUDIO_FRAME_INVALID);
    assert(audio_frame_validate(NULL) == AUDIO_FRAME_INVALID);
    return 0;
}


