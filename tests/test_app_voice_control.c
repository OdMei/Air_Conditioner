#include <assert.h>
#include "app_voice_control.h"

int main(void)
{
    app_climate_action_t action;
    app_climate_state_t state;
    app_climate_init(&state, 26U);
    app_voice_result_t r = {0U, 90U, "请把温度调高"};
    assert(app_voice_to_action(&r, &action) == APP_VOICE_ACCEPTED);
    assert(action == APP_ACTION_TEMP_UP);
    assert(app_voice_apply_result(&state, &r) == APP_VOICE_ACCEPTED);
    assert(state.target_temperature == 27U);
    r.phrase = "请打开强劲模式";
    assert(app_voice_to_action(&r, &action) == APP_VOICE_ACCEPTED && action == APP_ACTION_TURBO);
    r.phrase = "Turn the temperature up";
    assert(app_voice_to_action(&r, &action) == APP_VOICE_ACCEPTED && action == APP_ACTION_TEMP_UP);
    r.phrase = "SET FAN SPEED THREE";
    assert(app_voice_to_action(&r, &action) == APP_VOICE_ACCEPTED && action == APP_ACTION_FAN_3);
    r.keyword_id = 8U; r.phrase = 0;
    assert(app_voice_to_action(&r, &action) == APP_VOICE_ACCEPTED && action == APP_ACTION_FAN_3);
    r.keyword_id = 0U; r.confidence_percent = 40U;
    assert(app_voice_to_action(&r, &action) == APP_VOICE_REJECTED_LOW_CONFIDENCE);
    r.confidence_percent = 90U; r.phrase = "播放音乐";
    assert(app_voice_to_action(&r, &action) == APP_VOICE_UNKNOWN);
    return 0;
}
