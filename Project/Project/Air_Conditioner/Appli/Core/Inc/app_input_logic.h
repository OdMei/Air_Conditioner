#ifndef APP_INPUT_LOGIC_H
#define APP_INPUT_LOGIC_H

#include <stdint.h>

typedef enum
{
    APP_INPUT_ACTION_NONE = 0,
    APP_INPUT_ACTION_TOGGLE_POWER,
    APP_INPUT_ACTION_MODE_NEXT,
    APP_INPUT_ACTION_TEMP_DOWN,
    APP_INPUT_ACTION_TEMP_UP
} app_input_action_t;

/* Values are the public key codes returned by the BSP key driver. */
#define APP_INPUT_KEY_NONE       (0U)
#define APP_INPUT_KEY_WKUP       (1U)
#define APP_INPUT_KEY0           (2U)
#define APP_INPUT_KEY1           (3U)
#define APP_INPUT_KEY2           (4U)

app_input_action_t app_input_key_to_action(uint8_t key_code);

#endif /* APP_INPUT_LOGIC_H */
