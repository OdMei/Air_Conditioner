#include <assert.h>
#include <stddef.h>
#include "app_voice_kws.h"

static uint8_t infer_temp_up(const int16_t *samples, size_t count,
                             uint16_t *id, uint8_t *confidence)
{
    assert(samples != NULL && count == APP_VOICE_WINDOW_SAMPLES);
    *id = 2U;
    *confidence = 90U;
    return 1U;
}

int main(void)
{
    app_climate_state_t climate;
    app_voice_kws_t kws;
    int16_t samples[APP_VOICE_WINDOW_SAMPLES] = {0};
    app_climate_init(&climate, 26U);
    app_voice_kws_init(&kws, &climate, infer_temp_up);
    app_voice_kws_feed(&kws, samples, APP_VOICE_WINDOW_SAMPLES);
    assert(kws.windows_processed == 1U);
    assert(kws.commands_accepted == 1U);
    assert(climate.target_temperature == 27U);
    return 0;
}
