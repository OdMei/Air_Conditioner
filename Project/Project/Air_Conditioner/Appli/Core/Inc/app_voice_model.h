#ifndef APP_VOICE_MODEL_H
#define APP_VOICE_MODEL_H

#include <stddef.h>
#include <stdint.h>
#include "app_voice_kws.h"

/*
 * Board application hook for the keyword model.  The weak default keeps the
 * audio path usable before a generated DNN is linked; a product model can
 * provide the same symbol without changing the ThreadX audio service.
 */
uint8_t app_voice_model_infer(const int16_t *samples,
                              size_t sample_count,
                              uint16_t *keyword_id,
                              uint8_t *confidence_percent);

#endif
