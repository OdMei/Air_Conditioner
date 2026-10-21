#include "app_model_contract.h"
#include "app_model_config.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static unsigned checks;

#define CHECK(condition) do { ++checks; assert(condition); } while (0)

static app_model_io_t good_input(void)
{
    const app_model_io_t io = {
        APP_MODEL_DTYPE_UINT8, 4U,
        {1U, APP_MODEL_INPUT_HEIGHT, APP_MODEL_INPUT_WIDTH, APP_MODEL_INPUT_CHANNELS},
        APP_MODEL_INPUT_BYTES, APP_MODEL_INPUT_QUANT_SCALE, APP_MODEL_INPUT_ZERO_POINT
    };
    return io;
}

static app_model_io_t good_output(void)
{
    const app_model_io_t io = {
        APP_MODEL_DTYPE_FLOAT32, 3U,
        {1U, APP_MODEL_OUTPUT_CHANNELS, APP_MODEL_OUTPUT_BOXES, 0U},
        APP_MODEL_OUTPUT_BYTES, 0.0f, 0
    };
    return io;
}

static app_model_io_result_t validate(const app_model_io_t *input,
                                       const app_model_io_t *output)
{
    return app_model_validate_io(input, output,
                                 APP_MODEL_INPUT_BYTES, APP_MODEL_OUTPUT_BYTES);
}

static void test_shape_order_and_rank(void)
{
    app_model_io_t input = good_input();
    app_model_io_t output = good_output();
    CHECK(validate(&input, &output) == APP_MODEL_IO_OK);
    input.rank = 3U;
    input.shape[0] = APP_MODEL_INPUT_HEIGHT;
    input.shape[1] = APP_MODEL_INPUT_WIDTH;
    input.shape[2] = APP_MODEL_INPUT_CHANNELS;
    input.shape[3] = UINT32_MAX; /* Dimensions beyond rank are not read. */
    CHECK(validate(&input, &output) == APP_MODEL_IO_OK);
    input = good_input();
    input.shape[0] = 2U;
    CHECK(validate(&input, &output) == APP_MODEL_IO_INPUT_SHAPE);
    input = good_input();
    input.shape[1] = APP_MODEL_INPUT_CHANNELS;
    input.shape[3] = APP_MODEL_INPUT_HEIGHT; /* NCHW must not pass. */
    CHECK(validate(&input, &output) == APP_MODEL_IO_INPUT_SHAPE);
    input = good_input();
    for (unsigned rank = 0U; rank <= 255U; ++rank) {
        input.rank = (uint8_t)rank;
        if (rank != 4U) {
            CHECK(validate(&input, &output) == APP_MODEL_IO_INPUT_SHAPE);
        }
    }
    input = good_input();
    for (unsigned dimension = 0U; dimension < 4U; ++dimension) {
        uint32_t original = input.shape[dimension];
        input.shape[dimension] = 0U;
        CHECK(validate(&input, &output) == APP_MODEL_IO_INPUT_SHAPE);
        input.shape[dimension] = UINT32_MAX;
        CHECK(validate(&input, &output) == APP_MODEL_IO_INPUT_SHAPE);
        input.shape[dimension] = original;
    }
    const uint32_t allowed[6][4] = {
        {APP_MODEL_OUTPUT_CHANNELS, APP_MODEL_OUTPUT_BOXES, 1U, 1U},
        {APP_MODEL_OUTPUT_CHANNELS, 1U, APP_MODEL_OUTPUT_BOXES, 1U},
        {APP_MODEL_OUTPUT_CHANNELS, 1U, 1U, APP_MODEL_OUTPUT_BOXES},
        {1U, APP_MODEL_OUTPUT_CHANNELS, APP_MODEL_OUTPUT_BOXES, 1U},
        {1U, APP_MODEL_OUTPUT_CHANNELS, 1U, APP_MODEL_OUTPUT_BOXES},
        {1U, 1U, APP_MODEL_OUTPUT_CHANNELS, APP_MODEL_OUTPUT_BOXES}
    };
    output.rank = 4U;
    for (unsigned shape = 0U; shape < 6U; ++shape) {
        memcpy(output.shape, allowed[shape], sizeof(output.shape));
        CHECK(validate(&input, &output) == APP_MODEL_IO_OK);
        for (unsigned a = 0U; a < 4U; ++a) {
            for (unsigned b = a + 1U; b < 4U; ++b) {
                if (output.shape[a] == APP_MODEL_OUTPUT_CHANNELS &&
                    output.shape[b] == APP_MODEL_OUTPUT_BOXES) {
                    output.shape[a] = APP_MODEL_OUTPUT_BOXES;
                    output.shape[b] = APP_MODEL_OUTPUT_CHANNELS;
                    CHECK(validate(&input, &output) == APP_MODEL_IO_OUTPUT_SHAPE);
                    memcpy(output.shape, allowed[shape], sizeof(output.shape));
                }
            }
        }
    }
    output = good_output();
    for (unsigned rank = 0U; rank <= 255U; ++rank) {
        output.rank = (uint8_t)rank;
        if (rank != 3U) {
            CHECK(validate(&input, &output) == APP_MODEL_IO_OUTPUT_SHAPE);
        }
    }
    output = good_output();
    output.rank = 2U;
    output.shape[0] = APP_MODEL_OUTPUT_CHANNELS;
    output.shape[1] = APP_MODEL_OUTPUT_BOXES;
    CHECK(validate(&input, &output) == APP_MODEL_IO_OK);
    output.shape[0] = APP_MODEL_OUTPUT_BOXES;
    output.shape[1] = APP_MODEL_OUTPUT_CHANNELS;
    CHECK(validate(&input, &output) == APP_MODEL_IO_OUTPUT_SHAPE);
    output = good_output();
    for (unsigned dimension = 0U; dimension < output.rank; ++dimension) {
        uint32_t original = output.shape[dimension];
        output.shape[dimension] = 0U;
        CHECK(validate(&input, &output) == APP_MODEL_IO_OUTPUT_SHAPE);
        output.shape[dimension] = UINT32_MAX;
        CHECK(validate(&input, &output) == APP_MODEL_IO_OUTPUT_SHAPE);
        output.shape[dimension] = original;
    }
}

static void test_types_bytes_quantization_and_capacity(void)
{
    app_model_io_t input = good_input();
    app_model_io_t output = good_output();
    CHECK(validate(NULL, &output) == APP_MODEL_IO_NULL);
    CHECK(validate(&input, NULL) == APP_MODEL_IO_NULL);
    CHECK(validate(NULL, NULL) == APP_MODEL_IO_NULL);
    const int invalid_types[] = {-1, 0, 3, 4, 1000};
    for (unsigned i = 0U; i < sizeof(invalid_types) / sizeof(invalid_types[0]); ++i) {
        input.dtype = (app_model_dtype_t)invalid_types[i];
        CHECK(validate(&input, &output) == APP_MODEL_IO_INPUT_TYPE);
        input = good_input();
        output.dtype = (app_model_dtype_t)invalid_types[i];
        CHECK(validate(&input, &output) == APP_MODEL_IO_OUTPUT_TYPE);
        output = good_output();
    }
    input.dtype = APP_MODEL_DTYPE_FLOAT32;
    CHECK(validate(&input, &output) == APP_MODEL_IO_INPUT_TYPE);
    input = good_input();
    output.dtype = APP_MODEL_DTYPE_UINT8;
    CHECK(validate(&input, &output) == APP_MODEL_IO_OUTPUT_TYPE);
    output = good_output();
    const uint32_t bad_input_bytes[] = {0U, 1U, APP_MODEL_INPUT_BYTES - 1U,
                                       APP_MODEL_INPUT_BYTES + 1U, UINT32_MAX};
    for (unsigned i = 0U; i < sizeof(bad_input_bytes) / sizeof(bad_input_bytes[0]); ++i) {
        input.bytes = bad_input_bytes[i];
        CHECK(validate(&input, &output) == APP_MODEL_IO_INPUT_SIZE);
    }
    input = good_input();
    const uint32_t bad_output_bytes[] = {0U, 1U, APP_MODEL_OUTPUT_BYTES - 1U,
                                        APP_MODEL_OUTPUT_BYTES + 1U, UINT32_MAX};
    for (unsigned i = 0U; i < sizeof(bad_output_bytes) / sizeof(bad_output_bytes[0]); ++i) {
        output.bytes = bad_output_bytes[i];
        CHECK(validate(&input, &output) == APP_MODEL_IO_OUTPUT_SIZE);
    }
    output = good_output();
    CHECK(app_model_validate_io(&input, &output, 0U, APP_MODEL_OUTPUT_BYTES) == APP_MODEL_IO_INPUT_CAPACITY);
    CHECK(app_model_validate_io(&input, &output, APP_MODEL_INPUT_BYTES - 1U,
                                APP_MODEL_OUTPUT_BYTES) == APP_MODEL_IO_INPUT_CAPACITY);
    CHECK(app_model_validate_io(&input, &output, APP_MODEL_INPUT_BYTES, 0U) == APP_MODEL_IO_OUTPUT_CAPACITY);
    CHECK(app_model_validate_io(&input, &output, APP_MODEL_INPUT_BYTES,
                                APP_MODEL_OUTPUT_BYTES - 1U) == APP_MODEL_IO_OUTPUT_CAPACITY);
    CHECK(app_model_validate_io(&input, &output, UINT32_MAX, UINT32_MAX) == APP_MODEL_IO_OK);
    const float invalid_scales[] = {0.0f, -0.0f, -APP_MODEL_INPUT_QUANT_SCALE,
                                   1.0f / 256.0f, 1.0f, NAN, INFINITY, -INFINITY};
    for (unsigned i = 0U; i < sizeof(invalid_scales) / sizeof(invalid_scales[0]); ++i) {
        input.quant_scale = invalid_scales[i];
        CHECK(validate(&input, &output) == APP_MODEL_IO_INPUT_QUANT);
    }
    input = good_input();
    input.quant_scale = nextafterf(APP_MODEL_INPUT_QUANT_SCALE, 0.0f);
    CHECK(validate(&input, &output) == APP_MODEL_IO_OK);
    input.quant_scale = nextafterf(APP_MODEL_INPUT_QUANT_SCALE, INFINITY);
    CHECK(validate(&input, &output) == APP_MODEL_IO_OK);
    input.quant_scale = APP_MODEL_INPUT_QUANT_SCALE * 1.0001f;
    CHECK(validate(&input, &output) == APP_MODEL_IO_INPUT_QUANT);
    input = good_input();
    const int32_t invalid_zeros[] = {INT32_MIN, -128, -1, 1, 128, 255, INT32_MAX};
    for (unsigned i = 0U; i < sizeof(invalid_zeros) / sizeof(invalid_zeros[0]); ++i) {
        input.zero_point = invalid_zeros[i];
        CHECK(validate(&input, &output) == APP_MODEL_IO_INPUT_QUANT);
    }
    input = good_input();
    output.quant_scale = NAN;
    output.zero_point = INT32_MAX;
    CHECK(validate(&input, &output) == APP_MODEL_IO_OK); /* FLOAT output is not quantized. */
}

/* Bit-by-bit oracle uses neither the production table nor its nibble step. */
static uint32_t crc_oracle(const uint8_t *data, size_t length)
{
    uint32_t crc = UINT32_MAX;
    for (size_t i = 0U; i < length; ++i) {
        crc ^= data[i];
        for (unsigned bit = 0U; bit < 8U; ++bit) {
            crc = (crc & 1U) ? (crc >> 1U) ^ 0xEDB88320U : crc >> 1U;
        }
    }
    return crc ^ UINT32_MAX;
}

static uint32_t random_word(uint32_t *state)
{
    *state ^= *state << 13U;
    *state ^= *state >> 17U;
    *state ^= *state << 5U;
    return *state;
}

static void test_crc_independent_oracle(void)
{
    static uint8_t data[4096];
    uint32_t state = 0x4D4F444CU;
    CHECK(app_model_crc32("123456789", 9U) == 0xCBF43926U);
    CHECK(app_model_crc32("", 0U) == 0U);
    CHECK(app_model_crc32(NULL, 0U) == 0U);
    CHECK(app_model_crc32(NULL, 12U) == 0U);
    for (unsigned value = 0U; value < 256U; ++value) {
        data[0] = (uint8_t)value;
        CHECK(app_model_crc32(data, 1U) == crc_oracle(data, 1U));
    }
    for (unsigned i = 0U; i < sizeof(data); ++i) {
        data[i] = (uint8_t)random_word(&state);
    }
    for (unsigned length = 0U; length <= sizeof(data); ++length) {
        CHECK(app_model_crc32(data, length) == crc_oracle(data, length));
    }
    for (unsigned trial = 0U; trial < 8192U; ++trial) {
        size_t offset = random_word(&state) % sizeof(data);
        size_t length = random_word(&state) % (sizeof(data) - offset + 1U);
        CHECK(app_model_crc32(data + offset, length) == crc_oracle(data + offset, length));
    }
    memset(data, 0U, sizeof(data));
    CHECK(app_model_crc32(data, sizeof(data)) == crc_oracle(data, sizeof(data)));
    memset(data, 0xFF, sizeof(data));
    CHECK(app_model_crc32(data, sizeof(data)) == crc_oracle(data, sizeof(data)));
}

int main(void)
{
    test_shape_order_and_rank();
    test_types_bytes_quantization_and_capacity();
    test_crc_independent_oracle();
    printf("PASS model contract / IEEE CRC32: %u assertions\n", checks);
    return 0;
}
