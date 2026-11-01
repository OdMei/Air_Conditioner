#ifndef AUDIO_UAC_OUTPUT_H
#define AUDIO_UAC_OUTPUT_H

#include <stddef.h>
#include <stdint.h>
#include "audio_frame.h"

#define AUDIO_UAC_OUTPUT_QUEUE_DEPTH (2U)
#define AUDIO_UAC_OUTPUT_PACKET_BYTES (32U)

typedef enum {
    AUDIO_UAC_OUTPUT_OK = 0,
    AUDIO_UAC_OUTPUT_INVALID = 1,
    AUDIO_UAC_OUTPUT_EMPTY = 2,
    AUDIO_UAC_OUTPUT_STOPPED = 3,
    AUDIO_UAC_OUTPUT_BUSY = 4
} audio_uac_output_status_t;

typedef struct {
    uint32_t sample_rate_hz;
    uint8_t channels;
    uint8_t bits_per_sample;
    uint8_t endpoint_address;
    uint16_t max_packet_size;
} audio_uac_output_config_t;

typedef uint8_t (*audio_uac_output_send_cb_t)(
    const uint8_t *data, size_t size, void *context);

typedef struct {
    uint32_t submitted_frames;
    uint32_t sent_frames;
    uint32_t dropped_frames;
    uint32_t send_errors;
    uint8_t queued_frames;
    uint8_t running;
} audio_uac_output_stats_t;

typedef struct {
    audio_uac_output_config_t config;
    audio_uac_output_send_cb_t send;
    void *send_context;
    audio_frame_t queue[AUDIO_UAC_OUTPUT_QUEUE_DEPTH];
    audio_frame_t current_frame;
    uint8_t read_index;
    uint8_t write_index;
    uint8_t queued_frames;
    uint8_t frame_active;
    uint16_t packet_offset;
    uint8_t running;
    uint32_t submitted_frames;
    uint32_t sent_frames;
    uint32_t dropped_frames;
    uint32_t send_errors;
} audio_uac_output_t;

void audio_uac_output_default_config(audio_uac_output_config_t *config);
audio_uac_output_status_t audio_uac_output_init(
    audio_uac_output_t *output,
    const audio_uac_output_config_t *config,
    audio_uac_output_send_cb_t send,
    void *send_context);
void audio_uac_output_stop(audio_uac_output_t *output);
audio_uac_output_status_t audio_uac_output_submit(
    audio_uac_output_t *output,
    const audio_frame_t *frame);
audio_uac_output_status_t audio_uac_output_poll(
    audio_uac_output_t *output);
const uint8_t *audio_uac_output_descriptor(size_t *size);
void audio_uac_output_get_stats(const audio_uac_output_t *output,
                                audio_uac_output_stats_t *stats);

#endif
