#include "app_voice_ei_adapter.h"
#include <string.h>

typedef struct { const char *label; uint16_t id; } voice_label_t;

static const voice_label_t s_labels[] = {
    {"power", 1U}, {"开关", 1U}, {"temp_up", 2U}, {"temperature_up", 2U}, {"升温", 2U},
    {"temp_down", 3U}, {"temperature_down", 3U}, {"降温", 3U},
    {"cool", 4U}, {"cooling", 4U}, {"制冷", 4U}, {"fan_up", 5U}, {"风速增加", 5U},
    {"fan_1", 6U}, {"fan_2", 7U}, {"fan_3", 8U}, {"fan_4", 9U}, {"fan_5", 10U},
    {"turbo", 11U}, {"强劲", 11U}, {"swing", 12U}, {"摆风", 12U},
    {"lamp", 13U}, {"light", 13U}, {"灯光", 13U}, {"sleep", 14U}, {"睡眠", 14U},
    {"eco", 15U}, {"节能", 15U}
};

uint16_t app_voice_ei_label_to_keyword(const char *label)
{
    size_t i;
    if (!label) { return 0U; }
    for (i = 0U; i < sizeof(s_labels) / sizeof(s_labels[0]); ++i) {
        if (strcmp(label, s_labels[i].label) == 0) { return s_labels[i].id; }
    }
    return 0U;
}
