#include "app_input_logic.h"

app_input_action_t app_input_key_to_action(uint8_t key_code)
{
    switch (key_code)
    {
        case APP_INPUT_KEY_WKUP:
            return APP_INPUT_ACTION_TOGGLE_POWER;
        case APP_INPUT_KEY0:
            return APP_INPUT_ACTION_MODE_NEXT;
        case APP_INPUT_KEY1:
            return APP_INPUT_ACTION_TEMP_DOWN;
        case APP_INPUT_KEY2:
            return APP_INPUT_ACTION_TEMP_UP;
        default:
            return APP_INPUT_ACTION_NONE;
    }
}
