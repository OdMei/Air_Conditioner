#include "audio_uac_output.h"
#include <string.h>

static const uint8_t s_uac_descriptor[] = {
    0x09U, 0x04U, 0x00U, 0x00U, 0x00U, 0x01U, 0x01U, 0x00U, 0x00U,
    0x09U, 0x24U, 0x01U, 0x00U, 0x01U, 0x1EU, 0x00U, 0x01U, 0x01U,
    0x0CU, 0x24U, 0x02U, 0x01U, 0x01U, 0x02U, 0x00U, 0x01U, 0x00U,
    0x00U, 0x00U, 0x00U, 0x09U, 0x24U, 0x03U, 0x02U, 0x01U, 0x01U,
    0x00U, 0x01U, 0x00U, 0x09U, 0x04U, 0x01U, 0x00U, 0x00U, 0x01U,
    0x02U, 0x00U, 0x00U, 0x09U, 0x04U, 0x01U, 0x01U, 0x01U, 0x01U,
    0x02U, 0x00U, 0x00U, 0x07U, 0x24U, 0x01U, 0x02U, 0x01U,
    0x01U, 0x00U,
    0x0BU, 0x24U, 0x02U, 0x01U, 0x01U, 0x02U, 0x10U, 0x01U,
    0x80U, 0x3EU, 0x00U, 0x09U, 0x05U, 0x81U, 0x05U, 0x20U, 0x00U,
    0x01U, 0x00U, 0x00U,
    0x07U, 0x25U, 0x01U, 0x01U, 0x00U,
    0x00U, 0x00U
};

void audio_uac_output_default_config(audio_uac_output_config_t *config)
{
    if (config == 0) {
        return;
    }
    config->sample_rate_hz = AUDIO_FRAME_SAMPLE_RATE_HZ;
    config->channels = 1U;
    config->bits_per_sample = 16U;
    config->endpoint_address = 0x81U;
    config->max_packet_size = AUDIO_UAC_OUTPUT_PACKET_BYTES;
}

audio_uac_output_status_t audio_uac_output_init(
    audio_uac_output_t *output,
    const audio_uac_output_config_t *config,
    audio_uac_output_send_cb_t send,
    void *send_context)
{
    audio_uac_output_config_t defaults;
    if (output == 0 || send == 0) {
        return AUDIO_UAC_OUTPUT_INVALID;
    }
    audio_uac_output_default_config(&defaults);
    memset(output, 0, sizeof(*output));
    output->config = (config == 0) ? defaults : *config;
    if (output->config.sample_rate_hz != AUDIO_FRAME_SAMPLE_RATE_HZ ||
        output->config.channels != 1U ||
        output->config.bits_per_sample != 16U ||
        output->config.max_packet_size != AUDIO_UAC_OUTPUT_PACKET_BYTES) {
        return AUDIO_UAC_OUTPUT_INVALID;
    }
    output->send = send;
    output->send_context = send_context;
    output->running = 1U;
    return AUDIO_UAC_OUTPUT_OK;
}

void audio_uac_output_stop(audio_uac_output_t *output)
{
    if (output == 0) {
        return;
    }
    output->running = 0U;
    output->queued_frames = 0U;
    output->read_index = 0U;
    output->write_index = 0U;
    output->frame_active = 0U;
    output->packet_offset = 0U;
}

static void advance_index(uint8_t *index)
{
    *index = (uint8_t)((*index + 1U) % AUDIO_UAC_OUTPUT_QUEUE_DEPTH);
}

audio_uac_output_status_t audio_uac_output_submit(
    audio_uac_output_t *output,
    const audio_frame_t *frame)
{
    if (output == 0 || frame == 0 || output->send == 0) {
        return AUDIO_UAC_OUTPUT_INVALID;
    }
    if (output->running == 0U) {
        return AUDIO_UAC_OUTPUT_STOPPED;
    }
    if (audio_frame_validate(frame) != AUDIO_FRAME_OK) {
        return AUDIO_UAC_OUTPUT_INVALID;
    }
    if (output->queued_frames >= AUDIO_UAC_OUTPUT_QUEUE_DEPTH) {
        advance_index(&output->read_index);
        output->queued_frames--;
        output->dropped_frames++;
    }
    memcpy(&output->queue[output->write_index], frame,
           sizeof(audio_frame_t));
    advance_index(&output->write_index);
    output->queued_frames++;
    output->submitted_frames++;
    return AUDIO_UAC_OUTPUT_OK;
}

audio_uac_output_status_t audio_uac_output_poll(
    audio_uac_output_t *output)
{
    uint8_t packet[AUDIO_UAC_OUTPUT_PACKET_BYTES];
    audio_frame_t *frame;
    size_t index;
    if (output == 0 || output->send == 0) {
        return AUDIO_UAC_OUTPUT_INVALID;
    }
    if (output->running == 0U) {
        return AUDIO_UAC_OUTPUT_STOPPED;
    }
    if (output->frame_active == 0U && output->queued_frames == 0U) {
        return AUDIO_UAC_OUTPUT_EMPTY;
    }
    if (output->frame_active == 0U) {
        frame = &output->queue[output->read_index];
        output->current_frame = *frame;
        output->read_index = (uint8_t)((output->read_index + 1U) %
                                        AUDIO_UAC_OUTPUT_QUEUE_DEPTH);
        output->queued_frames--;
        output->packet_offset = 0U;
        output->frame_active = 1U;
    }
    frame = &output->current_frame;
    for (index = 0U; index < AUDIO_UAC_OUTPUT_PACKET_BYTES / 2U; index++) {
        uint32_t sample_index = (output->packet_offset / 2U) + index;
        packet[index * 2U] = (uint8_t)(frame->output[sample_index] & 0xFF);
        packet[index * 2U + 1U] = (uint8_t)
            (((uint16_t)frame->output[sample_index] >> 8) & 0xFFU);
    }
    if (output->send(packet, sizeof(packet), output->send_context) == 0U) {
        output->send_errors++;
        return AUDIO_UAC_OUTPUT_BUSY;
    }
    output->packet_offset = (uint16_t)(output->packet_offset +
                                       AUDIO_UAC_OUTPUT_PACKET_BYTES);
    if (output->packet_offset >= AUDIO_FRAME_SAMPLES * 2U) {
        output->frame_active = 0U;
        output->sent_frames++;
    }
    return AUDIO_UAC_OUTPUT_OK;
}

const uint8_t *audio_uac_output_descriptor(size_t *size)
{
    if (size == 0) {
        return 0;
    }
    *size = sizeof(s_uac_descriptor);
    return s_uac_descriptor;
}

void audio_uac_output_get_stats(const audio_uac_output_t *output,
                                audio_uac_output_stats_t *stats)
{
    if (output == 0 || stats == 0) {
        return;
    }
    stats->submitted_frames = output->submitted_frames;
    stats->sent_frames = output->sent_frames;
    stats->dropped_frames = output->dropped_frames;
    stats->send_errors = output->send_errors;
    stats->queued_frames = output->queued_frames;
    stats->running = output->running;
}
