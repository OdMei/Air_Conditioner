#include "deepseek_api.h"
#include <string.h>

static uint8_t append_char(char *output, size_t capacity,
                           size_t *index, char value)
{
    if (*index + 1U >= capacity) {
        return 0U;
    }
    output[*index] = value;
    (*index)++;
    return 1U;
}

static uint8_t append_text(char *output, size_t capacity,
                           size_t *index, const char *text)
{
    size_t length = strlen(text);
    if (*index + length >= capacity) {
        return 0U;
    }
    memcpy(&output[*index], text, length);
    *index += length;
    return 1U;
}

static uint8_t append_uint(char *output, size_t capacity,
                           size_t *index, uint16_t value)
{
    char digits[5];
    size_t count = 0U;
    size_t position;
    do {
        digits[count++] = (char)('0' + (value % 10U));
        value = (uint16_t)(value / 10U);
    } while (value != 0U && count < sizeof(digits));
    for (position = count; position > 0U; position--) {
        if (append_char(output, capacity, index, digits[position - 1U]) == 0U) {
            return 0U;
        }
    }
    return 1U;
}

static uint8_t append_json_string(char *output, size_t capacity,
                                  size_t *index, const char *text)
{
    size_t position;
    if (append_char(output, capacity, index, '"') == 0U) {
        return 0U;
    }
    for (position = 0U; text[position] != '\0'; position++) {
        char value = text[position];
        if (value == '"' || value == '\\') {
            if (append_char(output, capacity, index, '\\') == 0U ||
                append_char(output, capacity, index, value) == 0U) {
                return 0U;
            }
        } else if (value == '\n') {
            if (append_text(output, capacity, index, "\\n") == 0U) {
                return 0U;
            }
        } else if ((unsigned char)value < 0x20U) {
            if (append_char(output, capacity, index, ' ') == 0U) {
                return 0U;
            }
        } else if (append_char(output, capacity, index, value) == 0U) {
            return 0U;
        }
    }
    return append_char(output, capacity, index, '"');
}

deepseek_api_status_t deepseek_api_build_request(
    const deepseek_api_request_t *request,
    char *output,
    size_t output_capacity,
    size_t *output_size)
{
    size_t index = 0U;
    if (request == 0 || output == 0 || output_size == 0U ||
        request->model == 0 || request->system_prompt == 0 ||
        request->user_text == 0 || output_capacity == 0U) {
        return DEEPSEEK_API_INVALID;
    }
    if (append_text(output, output_capacity, &index,
                    "{\"model\":") == 0U ||
        append_json_string(output, output_capacity, &index,
                           request->model) == 0U ||
        append_text(output, output_capacity, &index,
                    ",\"messages\":[{\"role\":\"system\",\"content\":") == 0U ||
        append_json_string(output, output_capacity, &index,
                           request->system_prompt) == 0U ||
        append_text(output, output_capacity, &index,
                    "},{\"role\":\"user\",\"content\":") == 0U ||
        append_json_string(output, output_capacity, &index,
                           request->user_text) == 0U ||
        append_text(output, output_capacity, &index,
                    "}],\"max_tokens\":") == 0U ||
        append_uint(output, output_capacity, &index,
                    request->max_tokens) == 0U ||
        append_char(output, output_capacity, &index, '}') == 0U) {
        return DEEPSEEK_API_BUFFER_TOO_SMALL;
    }
    output[index] = '\0';
    *output_size = index;
    return DEEPSEEK_API_OK;
}

deepseek_api_status_t deepseek_api_init(
    deepseek_api_client_t *client,
    deepseek_api_transport_cb_t transport,
    void *transport_context,
    uint8_t *response_body,
    size_t response_capacity)
{
    if (client == 0 || transport == 0 || response_body == 0 ||
        response_capacity == 0U) {
        return DEEPSEEK_API_INVALID;
    }
    client->transport = transport;
    client->transport_context = transport_context;
    client->response_body = response_body;
    client->response_capacity = response_capacity;
    return DEEPSEEK_API_OK;
}

deepseek_api_status_t deepseek_api_submit(
    deepseek_api_client_t *client,
    const deepseek_api_request_t *request,
    size_t *response_size)
{
    char body[768];
    size_t body_size = 0U;
    if (client == 0 || request == 0 || response_size == 0U ||
        client->transport == 0 || client->response_body == 0) {
        return DEEPSEEK_API_INVALID;
    }
    if (deepseek_api_build_request(request, body, sizeof(body),
                                   &body_size) != DEEPSEEK_API_OK) {
        return DEEPSEEK_API_BUFFER_TOO_SMALL;
    }
    if (client->transport((const uint8_t *)body, body_size,
                          client->response_body, client->response_capacity,
                          response_size, client->transport_context) == 0U) {
        return DEEPSEEK_API_TRANSPORT_ERROR;
    }
    return DEEPSEEK_API_OK;
}
