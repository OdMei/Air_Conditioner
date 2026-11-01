#include <assert.h>
#include <stddef.h>
#include "audio_uac_output.h"

static uint32_t sent_frames;

static uint8_t send_pcm(const uint8_t *data, size_t size, void *context)
{
    (void)context;
    assert(data != NULL && size == AUDIO_UAC_OUTPUT_PACKET_BYTES);
    sent_frames++;
    return 1U;
}

int main(void)
{
    audio_uac_output_t output;
    audio_uac_output_config_t config;
    audio_uac_output_stats_t stats;
    audio_frame_t frame = {0};
    const uint8_t *descriptor;
    size_t descriptor_size = 0U;
    audio_uac_output_default_config(&config);
    assert(audio_uac_output_init(&output, &config, send_pcm, NULL) ==
           AUDIO_UAC_OUTPUT_OK);
    descriptor = audio_uac_output_descriptor(&descriptor_size);
    assert(descriptor != NULL && descriptor_size > 0U);
    frame.sample_rate_hz = AUDIO_FRAME_SAMPLE_RATE_HZ;
    frame.sample_count = AUDIO_FRAME_SAMPLES;
    frame.channel_count = AUDIO_FRAME_CHANNELS;
    assert(audio_uac_output_submit(&output, &frame) == AUDIO_UAC_OUTPUT_OK);
    for (uint8_t packet = 0U; packet < 10U; packet++)
    {
        assert(audio_uac_output_poll(&output) == AUDIO_UAC_OUTPUT_OK);
    }
    audio_uac_output_get_stats(&output, &stats);
    assert(stats.sent_frames == 1U && sent_frames == 1U);
    return 0;
}


