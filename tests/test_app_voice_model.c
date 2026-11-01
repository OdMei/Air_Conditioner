#include <assert.h>
#include <stddef.h>
#include "app_voice_model.h"

int main(void)
{
    uint16_t id = 99U;
    uint8_t confidence = 99U;
    int16_t samples[APP_VOICE_WINDOW_SAMPLES] = {0};
    assert(app_voice_model_infer(samples, APP_VOICE_WINDOW_SAMPLES,
                                 &id, &confidence) == 0U);
    assert(id == 0U && confidence == 0U);
    return 0;
}
