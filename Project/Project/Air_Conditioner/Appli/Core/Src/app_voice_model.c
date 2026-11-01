#include "app_voice_model.h"

#if defined(__GNUC__)
__attribute__((weak))
#endif
uint8_t app_voice_model_infer(const int16_t *samples,
                              size_t sample_count,
                              uint16_t *keyword_id,
                              uint8_t *confidence_percent)
{
    (void)samples;
    (void)sample_count;
    if (keyword_id != NULL) {
        *keyword_id = 0U;
    }
    if (confidence_percent != NULL) {
        *confidence_percent = 0U;
    }
    return 0U;
}
