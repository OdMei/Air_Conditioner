#include <assert.h>
#include <stdint.h>

#include "app_input_logic.h"

static void test_key_actions(void)
{
    assert(app_input_key_to_action(APP_INPUT_KEY_WKUP) ==
           APP_ACTION_POWER);
    assert(app_input_key_to_action(APP_INPUT_KEY0) ==
           APP_ACTION_MODE_NEXT);
    assert(app_input_key_to_action(APP_INPUT_KEY1) ==
           APP_ACTION_TEMP_DOWN);
    assert(app_input_key_to_action(APP_INPUT_KEY2) ==
           APP_ACTION_TEMP_UP);
    assert(app_input_key_to_action(APP_INPUT_KEY_NONE) ==
           APP_ACTION_NONE);
    assert(app_input_key_to_action(5U) == APP_ACTION_NONE);
}

int main(void)
{
    test_key_actions();
    return 0;
}
