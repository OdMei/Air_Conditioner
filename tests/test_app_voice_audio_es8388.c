#include <assert.h>
#include "app_voice_audio_es8388.h"

int main(void)
{
    assert(app_voice_audio_es8388_start(NULL) != 0U);
    app_voice_audio_es8388_dma_complete();
    return 0;
}
