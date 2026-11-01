#ifndef APP_VOICE_AUDIO_ES8388_H
#define APP_VOICE_AUDIO_ES8388_H

#include <stdint.h>
#include "app_voice_kws.h"

/* Keep disabled until the board's SAI1 GPIO/MSP configuration is confirmed. */
#ifndef APP_VOICE_ES8388_ENABLE
#define APP_VOICE_ES8388_ENABLE (0U)
#endif

uint8_t app_voice_audio_es8388_start(app_voice_kws_t *kws);
/* Call from a normal input thread; never run KWS inference in the DMA ISR. */
void app_voice_audio_es8388_poll(void);
void app_voice_audio_es8388_dma_complete(void);

#endif
