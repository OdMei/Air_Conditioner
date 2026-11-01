#include "app_input_logic.h"

app_climate_action_t app_input_key_to_action(uint8_t key_code)
{
    switch (key_code)
    {
        case APP_INPUT_KEY_WKUP:
            return APP_ACTION_POWER;
        case APP_INPUT_KEY0:
            return APP_ACTION_MODE_NEXT;
        case APP_INPUT_KEY1:
            return APP_ACTION_TEMP_DOWN;
        case APP_INPUT_KEY2:
            return APP_ACTION_TEMP_UP;
        default:
            return APP_ACTION_NONE;
    }
}

app_voice_status_t app_input_voice_to_action(const app_voice_result_t *result,
                                             app_climate_action_t *action)
{
    return app_voice_to_action(result, action);
}
