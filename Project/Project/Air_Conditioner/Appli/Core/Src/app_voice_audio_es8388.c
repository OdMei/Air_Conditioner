#include "app_voice_audio_es8388.h"

#if APP_VOICE_ES8388_ENABLE

#include "es8388.h"
#include "es8388_sai.h"

#define ES8388_SAMPLE_RATE (16000U)
#define ES8388_DMA_SAMPLES (1600U)

static app_voice_kws_t *s_kws;
static int16_t s_dma_samples[2][ES8388_DMA_SAMPLES];
static volatile uint8_t s_dma_ready;
static uint8_t s_dma_active;

static void rx_complete(void)
{
    /* ISR contract: publish the completed buffer only. */
    s_dma_ready = (uint8_t)(s_dma_active + 1U);
}

uint8_t app_voice_audio_es8388_start(app_voice_kws_t *kws)
{
    if (kws == NULL || es8388_init() != 0U ||
        es8388_sai_clock_config(ES8388_SAMPLE_RATE) != 0U ||
        es8388_sai_init_rx(2U, ES8388_SAMPLE_RATE, 16U) != 0U) {
        return 1U;
    }
    s_kws = kws;
    s_dma_active = 0U;
    s_dma_ready = 0U;
    es8388_sai_rx_dma_cplt_user_cb_register(rx_complete);
    return es8388_sai_rx_dma((uint8_t *)s_dma_samples[0], ES8388_DMA_SAMPLES);
}

void app_voice_audio_es8388_poll(void)
{
    uint8_t ready = s_dma_ready;
    uint8_t next;
    if (ready == 0U || s_kws == NULL) { return; }
    s_dma_ready = 0U;
    next = (uint8_t)(ready - 1U);
    app_voice_kws_feed(s_kws, s_dma_samples[next], ES8388_DMA_SAMPLES);
    s_dma_active = (uint8_t)(next ^ 1U);
    (void)es8388_sai_rx_dma((uint8_t *)s_dma_samples[s_dma_active], ES8388_DMA_SAMPLES);
}

void app_voice_audio_es8388_dma_complete(void)
{
    es8388_sai_rx_dma_cplt_handler();
}

#else

uint8_t app_voice_audio_es8388_start(app_voice_kws_t *kws)
{
    (void)kws;
    return 1U;
}

void app_voice_audio_es8388_dma_complete(void) {}
void app_voice_audio_es8388_poll(void) {}

#endif
