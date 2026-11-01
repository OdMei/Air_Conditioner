#include <assert.h>
#include "app_voice_ei_adapter.h"

int main(void)
{
    assert(app_voice_ei_label_to_keyword("temp_up") == 2U);
    assert(app_voice_ei_label_to_keyword("升温") == 2U);
    assert(app_voice_ei_label_to_keyword("fan_3") == 8U);
    assert(app_voice_ei_label_to_keyword("unknown") == 0U);
    return 0;
}
