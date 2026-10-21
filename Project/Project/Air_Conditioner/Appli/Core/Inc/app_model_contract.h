#ifndef APP_MODEL_CONTRACT_H
#define APP_MODEL_CONTRACT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    APP_MODEL_DTYPE_UINT8 = 1,
    APP_MODEL_DTYPE_FLOAT32 = 2
} app_model_dtype_t;

/* This describes the generated tensor, not a buffer allocation. The caller
 * supplies the actual owned-buffer capacities separately. */
typedef struct {
    app_model_dtype_t dtype;
    uint8_t rank;
    uint32_t shape[4];
    uint32_t bytes;
    float quant_scale;
    int32_t zero_point;
} app_model_io_t;

typedef enum {
    APP_MODEL_IO_OK = 0,
    APP_MODEL_IO_NULL = -1200,
    APP_MODEL_IO_INPUT_TYPE = -1201,
    APP_MODEL_IO_INPUT_SHAPE = -1202,
    APP_MODEL_IO_INPUT_SIZE = -1203,
    APP_MODEL_IO_INPUT_QUANT = -1204,
    APP_MODEL_IO_INPUT_CAPACITY = -1205,
    APP_MODEL_IO_OUTPUT_TYPE = -1206,
    APP_MODEL_IO_OUTPUT_SHAPE = -1207,
    APP_MODEL_IO_OUTPUT_SIZE = -1208,
    APP_MODEL_IO_OUTPUT_CAPACITY = -1209
} app_model_io_result_t;

/* Accept a single RGB NHWC input, with an optional leading batch of one.
 * For planar output, unit dimensions may be removed; the remaining ordered
 * dimensions must be [channels, boxes]. Quantization belongs to UINT8 input;
 * the FLOAT32 output is already dequantized and has no quantization contract. */
app_model_io_result_t app_model_validate_io(const app_model_io_t *input,
                                           const app_model_io_t *output,
                                           uint32_t input_capacity,
                                           uint32_t output_capacity);

/* Standard reflected IEEE CRC-32 (polynomial 0xEDB88320, initial/final XOR
 * 0xFFFFFFFF), compatible with zlib.crc32(data). Empty or NULL data returns 0;
 * this function cannot validate whether an arbitrary address is readable. */
uint32_t app_model_crc32(const void *data, size_t length);

#ifdef __cplusplus
}
#endif

#endif
