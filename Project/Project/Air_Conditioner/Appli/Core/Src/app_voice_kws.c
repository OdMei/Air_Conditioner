#include "app_voice_kws.h"
#include <string.h>

static void process_window(app_voice_kws_t *kws)
{
    uint16_t keyword_id = 0U;
    uint8_t confidence = 0U;
    app_voice_result_t result;
    app_voice_status_t status;
    if (!kws->infer) { return; }
    kws->windows_processed++;
    if (!kws->infer(kws->window, APP_VOICE_WINDOW_SAMPLES,
                    &keyword_id, &confidence)) {
        kws->commands_rejected++;
        return;
    }
    result.keyword_id = keyword_id;
    result.confidence_percent = confidence;
    result.phrase = NULL;
    status = app_voice_apply_result(kws->climate, &result);
    if (status == APP_VOICE_ACCEPTED) {
        kws->commands_accepted++;
    } else {
        kws->commands_rejected++;
    }
}

void app_voice_kws_init(app_voice_kws_t *kws,
                        app_climate_state_t *climate,
                        app_voice_kws_infer_cb_t infer)
{
    if (!kws) { return; }
    memset(kws, 0, sizeof(*kws));
    kws->climate = climate;
    kws->infer = infer;
}

void app_voice_kws_feed(app_voice_kws_t *kws,
                        const int16_t *samples,
                        size_t sample_count)
{
    size_t copy_count;
    if (!kws || !samples || !kws->infer) { return; }
    while (sample_count != 0U) {
        copy_count = APP_VOICE_WINDOW_SAMPLES - kws->filled;
        if (copy_count > sample_count) { copy_count = sample_count; }
        memcpy(&kws->window[kws->filled], samples, copy_count * sizeof(int16_t));
        kws->filled += copy_count;
        samples += copy_count;
        sample_count -= copy_count;
        if (kws->filled == APP_VOICE_WINDOW_SAMPLES) {
            process_window(kws);
            memmove(kws->window,
                    &kws->window[APP_VOICE_HOP_SAMPLES],
                    (APP_VOICE_WINDOW_SAMPLES - APP_VOICE_HOP_SAMPLES) * sizeof(int16_t));
            kws->filled = APP_VOICE_WINDOW_SAMPLES - APP_VOICE_HOP_SAMPLES;
        }
    }
}
