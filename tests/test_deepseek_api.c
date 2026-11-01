#include <assert.h>
#include <stddef.h>
#include <string.h>
#include "deepseek_api.h"

int main(void)
{
    deepseek_api_request_t request = {
        "deepseek-chat", "You control an air conditioner.",
        "打开空调", 32U
    };
    char body[512];
    size_t length = 0U;
    assert(deepseek_api_build_request(&request, body, sizeof(body),
                                      &length) == DEEPSEEK_API_OK);
    assert(length > 0U && strstr(body, "deepseek-chat") != NULL);
    assert(deepseek_api_build_request(&request, body, 8U,
                                      &length) == DEEPSEEK_API_BUFFER_TOO_SMALL);
    assert(deepseek_api_build_request(NULL, body, sizeof(body),
                                      &length) == DEEPSEEK_API_INVALID);
    return 0;
}
