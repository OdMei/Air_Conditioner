#include <assert.h>
#include <stddef.h>
#include "app_audio_es8388_service.h"

int main(void)
{
    app_audio_es8388_service_t service;
    audio_frame_t frame = {0};
    const int16_t stereo[] = {100, -100, 200, -200, 300, -300, 400, -400};
    assert(app_audio_es8388_service_init(&service, NULL) ==
           APP_AUDIO_ES8388_OK);
    assert(app_audio_es8388_service_unpack_stereo(stereo, 4U, &frame) ==
           APP_AUDIO_ES8388_OK);
    assert(frame.mic[0][0] == 100 && frame.mic[1][0] == -100);
    assert(frame.mic[0][3] == 400 && frame.mic[1][3] == -400);
    assert(app_audio_es8388_service_unpack_stereo(NULL, 4U, &frame) ==
           APP_AUDIO_ES8388_INVALID);
    assert(app_audio_es8388_service_start(&service) ==
           APP_AUDIO_ES8388_DISABLED);
    return 0;
}
