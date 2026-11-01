#include "app_voice_control.h"
#include <string.h>

/* IDs are stable contracts for a small offline keyword model. */
enum {
    VOICE_POWER = 1, VOICE_TEMP_UP, VOICE_TEMP_DOWN, VOICE_COOL,
    VOICE_FAN_UP, VOICE_FAN_1, VOICE_FAN_2, VOICE_FAN_3,
    VOICE_FAN_4, VOICE_FAN_5, VOICE_TURBO, VOICE_SWING,
    VOICE_LAMP, VOICE_SLEEP, VOICE_ECO
};

static app_climate_action_t id_action(uint16_t id)
{
    switch (id) {
    case VOICE_POWER: return APP_ACTION_POWER;
    case VOICE_TEMP_UP: return APP_ACTION_TEMP_UP;
    case VOICE_TEMP_DOWN: return APP_ACTION_TEMP_DOWN;
    case VOICE_COOL: return APP_ACTION_MODE_NEXT;
    case VOICE_FAN_UP: return APP_ACTION_FAN_NEXT;
    case VOICE_FAN_1: return APP_ACTION_FAN_1;
    case VOICE_FAN_2: return APP_ACTION_FAN_2;
    case VOICE_FAN_3: return APP_ACTION_FAN_3;
    case VOICE_FAN_4: return APP_ACTION_FAN_4;
    case VOICE_FAN_5: return APP_ACTION_FAN_5;
    case VOICE_TURBO: return APP_ACTION_TURBO;
    case VOICE_SWING: return APP_ACTION_SWING_LR;
    case VOICE_LAMP: return APP_ACTION_LAMP;
    case VOICE_SLEEP: return APP_ACTION_SLEEP_TOGGLE;
    case VOICE_ECO: return APP_ACTION_ECO_TOGGLE;
    default: return APP_ACTION_NONE;
    }
}

static uint16_t phrase_id(const char *p)
{
    char ascii[160];
    size_t i;
    if (!p) return 0U;
    for (i = 0U; i + 1U < sizeof(ascii) && p[i] != '\0'; ++i) {
        unsigned char c = (unsigned char)p[i];
        ascii[i] = (c >= 'A' && c <= 'Z') ? (char)(c + ('a' - 'A')) : (char)c;
    }
    ascii[i] = '\0';
    if (strstr(p, "开机") || strstr(p, "打开空调") || strstr(ascii, "turn on") || strstr(ascii, "power on")) return VOICE_POWER;
    if (strstr(p, "关机") || strstr(p, "关闭空调") || strstr(ascii, "turn off") || strstr(ascii, "power off")) return VOICE_POWER;
    if (strstr(p, "升高") || strstr(p, "调高") || strstr(p, "升温") || strstr(ascii, "temperature up") || strstr(ascii, "warmer")) return VOICE_TEMP_UP;
    if (strstr(p, "降低") || strstr(p, "调低") || strstr(p, "降温") || strstr(ascii, "temperature down") || strstr(ascii, "cooler")) return VOICE_TEMP_DOWN;
    if (strstr(p, "制冷") || strstr(p, "冷气") || strstr(ascii, "cooling") || strstr(ascii, "cool mode")) return VOICE_COOL;
    if ((strstr(p, "风速") && (strstr(p, "增加") || strstr(p, "大一点"))) || strstr(ascii, "fan speed up") || strstr(ascii, "more fan")) return VOICE_FAN_UP;
    if (strstr(p, "一档") || strstr(ascii, "fan one") || strstr(ascii, "fan speed one")) return VOICE_FAN_1;
    if (strstr(p, "二档") || strstr(ascii, "fan two") || strstr(ascii, "fan speed two")) return VOICE_FAN_2;
    if (strstr(p, "三档") || strstr(ascii, "fan three") || strstr(ascii, "fan speed three")) return VOICE_FAN_3;
    if (strstr(p, "四档") || strstr(ascii, "fan four") || strstr(ascii, "fan speed four")) return VOICE_FAN_4;
    if (strstr(p, "五档") || strstr(ascii, "fan five") || strstr(ascii, "fan speed five")) return VOICE_FAN_5;
    if (strstr(p, "强劲") || strstr(ascii, "turbo") || strstr(ascii, "powerful")) return VOICE_TURBO;
    if (strstr(p, "摆风") || strstr(ascii, "swing")) return VOICE_SWING;
    if (strstr(p, "灯光") || strstr(ascii, "light") || strstr(ascii, "lamp")) return VOICE_LAMP;
    if (strstr(p, "睡眠") || strstr(ascii, "sleep")) return VOICE_SLEEP;
    if (strstr(p, "节能") || strstr(ascii, "eco") || strstr(ascii, "energy saving")) return VOICE_ECO;
    return 0U;
}

app_voice_status_t app_voice_to_action(const app_voice_result_t *result,
                                       app_climate_action_t *action)
{
    uint16_t id;
    app_climate_action_t mapped;
    if (!result || !action) return APP_VOICE_UNKNOWN;
    *action = APP_ACTION_NONE;
    if (result->confidence_percent < APP_VOICE_MIN_CONFIDENCE) return APP_VOICE_REJECTED_LOW_CONFIDENCE;
    id = result->keyword_id ? result->keyword_id : phrase_id(result->phrase);
    mapped = id_action(id);
    if (mapped == APP_ACTION_NONE) return APP_VOICE_UNKNOWN;
    *action = mapped;
    return APP_VOICE_ACCEPTED;
}

app_voice_status_t app_voice_apply_result(app_climate_state_t *state,
                                           const app_voice_result_t *result)
{
    app_climate_action_t action;
    app_voice_status_t status = app_voice_to_action(result, &action);
    if (status == APP_VOICE_ACCEPTED) {
        app_climate_apply_action(state, action);
    }
    return status;
}
