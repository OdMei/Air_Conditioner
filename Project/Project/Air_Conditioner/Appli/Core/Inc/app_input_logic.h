#ifndef APP_INPUT_LOGIC_H
#define APP_INPUT_LOGIC_H

#include "app_climate_logic.h"

/* Values are the public key codes returned by the BSP key driver. */
#define APP_INPUT_KEY_NONE       (0U)
#define APP_INPUT_KEY_WKUP       (1U)
#define APP_INPUT_KEY0           (2U)
#define APP_INPUT_KEY1           (3U)
#define APP_INPUT_KEY2           (4U)

app_climate_action_t app_input_key_to_action(uint8_t key_code);

#endif /* APP_INPUT_LOGIC_H */
