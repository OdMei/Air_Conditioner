#include "app_model_contract.h"
#include "app_model_config.h"

#include <math.h>

_Static_assert(sizeof(float) == 4U, "Model output requires 32-bit float");

static uint8_t valid_input_shape(const app_model_io_t *input)
{
    uint8_t offset;
    if (input->rank == 4U) {
        if (input->shape[0] != 1U) {
            return 0U;
        }
        offset = 1U;
    } else if (input->rank == 3U) {
        offset = 0U;
    } else {
        return 0U;
    }
    return input->shape[offset] == APP_MODEL_INPUT_HEIGHT &&
           input->shape[offset + 1U] == APP_MODEL_INPUT_WIDTH &&
           input->shape[offset + 2U] == APP_MODEL_INPUT_CHANNELS;
}

static uint8_t valid_output_shape(const app_model_io_t *output)
{
    uint32_t nonunit[2];
    uint8_t count = 0U;
    if (output->rank == 0U || output->rank > 4U) {
        return 0U;
    }
    for (uint8_t i = 0U; i < output->rank; ++i) {
        uint32_t dimension = output->shape[i];
        if (dimension == 1U) {
            continue;
        }
        if (dimension == 0U || count == 2U) {
            return 0U;
        }
        nonunit[count++] = dimension;
    }
    return count == 2U && nonunit[0] == APP_MODEL_OUTPUT_CHANNELS &&
           nonunit[1] == APP_MODEL_OUTPUT_BOXES;
}

app_model_io_result_t app_model_validate_io(const app_model_io_t *input,
                                           const app_model_io_t *output,
                                           uint32_t input_capacity,
                                           uint32_t output_capacity)
{
    const float scale = APP_MODEL_INPUT_QUANT_SCALE;
    const float tolerance = scale * 0.00001f + 0.00000001f;
    if (input == NULL || output == NULL) {
        return APP_MODEL_IO_NULL;
    }
    if (input->dtype != APP_MODEL_DTYPE_UINT8) {
        return APP_MODEL_IO_INPUT_TYPE;
    }
    if (!valid_input_shape(input)) {
        return APP_MODEL_IO_INPUT_SHAPE;
    }
    if (input->bytes != APP_MODEL_INPUT_BYTES) {
        return APP_MODEL_IO_INPUT_SIZE;
    }
    if (!isfinite(input->quant_scale) || input->quant_scale <= 0.0f ||
        fabsf(input->quant_scale - scale) > tolerance ||
        input->zero_point != APP_MODEL_INPUT_ZERO_POINT) {
        return APP_MODEL_IO_INPUT_QUANT;
    }
    if (input_capacity < APP_MODEL_INPUT_BYTES) {
        return APP_MODEL_IO_INPUT_CAPACITY;
    }
    if (output->dtype != APP_MODEL_DTYPE_FLOAT32) {
        return APP_MODEL_IO_OUTPUT_TYPE;
    }
    if (!valid_output_shape(output)) {
        return APP_MODEL_IO_OUTPUT_SHAPE;
    }
    if (output->bytes != APP_MODEL_OUTPUT_BYTES) {
        return APP_MODEL_IO_OUTPUT_SIZE;
    }
    if (output_capacity < APP_MODEL_OUTPUT_BYTES) {
        return APP_MODEL_IO_OUTPUT_CAPACITY;
    }
    return APP_MODEL_IO_OK;
}

uint32_t app_model_crc32(const void *data, size_t length)
{
    static const uint32_t nibble_table[16] = {
        0x00000000U, 0x1DB71064U, 0x3B6E20C8U, 0x26D930ACU,
        0x76DC4190U, 0x6B6B51F4U, 0x4DB26158U, 0x5005713CU,
        0xEDB88320U, 0xF00F9344U, 0xD6D6A3E8U, 0xCB61B38CU,
        0x9B64C2B0U, 0x86D3D2D4U, 0xA00AE278U, 0xBDBDF21CU
    };
    const uint8_t *bytes = (const uint8_t *)data;
    uint32_t crc = 0xFFFFFFFFU;
    if (bytes == NULL) {
        return 0U;
    }
    for (size_t i = 0U; i < length; ++i) {
        crc ^= bytes[i];
        crc = (crc >> 4U) ^ nibble_table[crc & 0x0FU];
        crc = (crc >> 4U) ^ nibble_table[crc & 0x0FU];
    }
    return crc ^ 0xFFFFFFFFU;
}
