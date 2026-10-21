#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "od_yolov2_pp_if.h"

enum { GRID = 7, ANCHORS = 5, CANDIDATES = GRID * GRID * ANCHORS,
       STRIDE = 6, MAX_RESULTS = 10, GUARD_FLOATS = 1024 };

typedef struct
{
    uint32_t before[16];
    float32_t raw[CANDIDATES * STRIDE];
    float32_t after[GUARD_FLOATS];
} guarded_input_t;

typedef struct
{
    uint32_t before[16];
    od_pp_outBuffer_t boxes[MAX_RESULTS];
    uint32_t after[16];
} guarded_output_t;

static void run_boundary(unsigned int candidate_count, uint8_t in_place)
{
    static guarded_input_t guarded;
    static guarded_output_t result;
    const float32_t anchors[ANCHORS * 2] = {1, 1, 2, 2, 3, 3, 4, 4, 5, 5};
    yolov2_pp_static_param_t params = {
        .nb_classes = 1, .nb_anchors = ANCHORS,
        .grid_width = GRID, .grid_height = GRID,
        .nb_input_boxes = CANDIDATES, .max_boxes_limit = MAX_RESULTS,
        .conf_threshold = 0.6f, .iou_threshold = 1.0f, .pAnchors = anchors
    };
    yolov2_pp_in_t input = {.pRaw_detections = guarded.raw};
    od_pp_out_t output = {.pOutBuff = in_place ? NULL : result.boxes, .nb_detect = -1};
    uint32_t input_prefix[16], output_prefix[16], output_suffix[16];
    float32_t input_suffix[GUARD_FLOATS];

    memset(&guarded, 0, sizeof(guarded));
    memset(&result, 0xA5, sizeof(result));
    for (unsigned int index = 0; index < 16U; index++)
    {
        guarded.before[index] = 0xC0FFEE00U + index;
    }
    /* A readable high-confidence suffix makes a one-past candidate visible:
     * old <= grid loops mutate it even without an address sanitizer. */
    for (unsigned int index = 0; index < GUARD_FLOATS; index++)
    {
        guarded.after[index] = 1.0f;
    }
    for (unsigned int index = 0; index < CANDIDATES; index++)
    {
        guarded.raw[index * STRIDE + 4U] = index < candidate_count ? 10.0f : -10.0f;
    }
    memcpy(input_prefix, guarded.before, sizeof(input_prefix));
    memcpy(input_suffix, guarded.after, sizeof(input_suffix));
    memcpy(output_prefix, result.before, sizeof(output_prefix));
    memcpy(output_suffix, result.after, sizeof(output_suffix));

    assert(od_yolov2_pp_reset(&params) == AI_OD_POSTPROCESS_ERROR_NO);
    assert(od_yolov2_pp_process(&input, &output, &params) == AI_OD_POSTPROCESS_ERROR_NO);
    assert(params.nb_detect == (int32_t)candidate_count);
    assert(output.nb_detect == (int32_t)(candidate_count < MAX_RESULTS ? candidate_count : MAX_RESULTS));
    assert(output.pOutBuff == (in_place ? (od_pp_outBuffer_t *)guarded.raw : result.boxes));
    assert(memcmp(input_prefix, guarded.before, sizeof(input_prefix)) == 0);
    assert(memcmp(input_suffix, guarded.after, sizeof(input_suffix)) == 0);
    assert(memcmp(output_prefix, result.before, sizeof(output_prefix)) == 0);
    assert(memcmp(output_suffix, result.after, sizeof(output_suffix)) == 0);
    for (int32_t index = 0; index < output.nb_detect; index++)
    {
        const od_pp_outBuffer_t *box = &output.pOutBuff[index];
        assert(box->class_index == 0 && isfinite(box->conf) && box->conf >= 0.6f);
        assert(box->x_center >= 0.0f && box->x_center < 1.0f);
        assert(box->y_center >= 0.0f && box->y_center < 1.0f);
        assert(box->width > 0.0f && box->height > 0.0f);
    }
}

int main(void)
{
    const unsigned int counts[] = {0U, 1U, 10U, 244U, 245U};
    for (unsigned int index = 0U; index < sizeof(counts) / sizeof(counts[0]); index++)
    {
        run_boundary(counts[index], 0U);
        run_boundary(counts[index], 1U);
    }
    puts("YOLOv2 production bounds tests passed: 0/1/10/244/245 candidates, input/output guards, in-place/output modes");
    return 0;
}
