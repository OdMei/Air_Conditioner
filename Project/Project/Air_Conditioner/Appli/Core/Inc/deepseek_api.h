#ifndef DEEPSEEK_API_H
#define DEEPSEEK_API_H

#include <stddef.h>
#include <stdint.h>

typedef enum {
    DEEPSEEK_API_OK = 0,
    DEEPSEEK_API_INVALID = 1,
    DEEPSEEK_API_BUFFER_TOO_SMALL = 2,
    DEEPSEEK_API_TRANSPORT_ERROR = 3
} deepseek_api_status_t;

typedef struct {
    const char *model;
    const char *system_prompt;
    const char *user_text;
    uint16_t max_tokens;
} deepseek_api_request_t;

typedef uint8_t (*deepseek_api_transport_cb_t)(
    const uint8_t *request_body,
    size_t request_size,
    uint8_t *response_body,
    size_t response_capacity,
    size_t *response_size,
    void *context);

typedef struct {
    deepseek_api_transport_cb_t transport;
    void *transport_context;
    uint8_t *response_body;
    size_t response_capacity;
} deepseek_api_client_t;

deepseek_api_status_t deepseek_api_build_request(
    const deepseek_api_request_t *request,
    char *output,
    size_t output_capacity,
    size_t *output_size);
deepseek_api_status_t deepseek_api_init(
    deepseek_api_client_t *client,
    deepseek_api_transport_cb_t transport,
    void *transport_context,
    uint8_t *response_body,
    size_t response_capacity);
deepseek_api_status_t deepseek_api_submit(
    deepseek_api_client_t *client,
    const deepseek_api_request_t *request,
    size_t *response_size);

#endif
