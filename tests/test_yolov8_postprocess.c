#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "app_postprocess.h"
#include "vision_models_pp.h"

enum { BOXES = 1344, CHANNELS = 5, MAX_CLASSES = 5, GUARDS = 16 };
static unsigned long long assertions;
static unsigned long long optimized_iou_calls, reference_iou_calls;
static uint32_t rng_state = 0x8120AC64U;

#define CHECK(expression) do { ++assertions; assert(expression); } while (0)

float32_t counted_optimized_iou(float32_t *a, float32_t *b)
{
    ++optimized_iou_calls;
    return vision_models_box_iou(a, b);
}

float32_t counted_reference_iou(float32_t *a, float32_t *b)
{
    ++reference_iou_calls;
    return vision_models_box_iou(a, b);
}

int32_t reference_od_yolov8_pp_reset(yolov8_pp_static_param_t *params);
int32_t reference_od_yolov8_pp_process(yolov8_pp_in_centroid_t *input,
                                      od_pp_out_t *output,
                                      yolov8_pp_static_param_t *params);

typedef struct {
    uint32_t before[GUARDS];
    float32_t raw[(4 + MAX_CLASSES) * BOXES];
    uint32_t after[GUARDS];
} guarded_input_t;

typedef struct {
    uint32_t before[GUARDS];
    od_pp_outBuffer_t boxes[BOXES];
    uint32_t after[GUARDS];
} guarded_output_t;

static guarded_input_t input_buffer;
static guarded_output_t optimized_output, reference_output;

static uint32_t random_u32(void)
{
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return rng_state;
}

static float random_unit(void)
{
    return (float)(random_u32() & 0xffffU) / 65535.0f;
}

static void prepare(void)
{
    memset(&input_buffer, 0, sizeof(input_buffer));
    memset(&optimized_output, 0x5a, sizeof(optimized_output));
    memset(&reference_output, 0x5a, sizeof(reference_output));
    for (unsigned index = 0; index < GUARDS; ++index) {
        input_buffer.before[index] = input_buffer.after[index] = 0xBAAD0000U + index;
    }
}

static void check_guards(void)
{
    const uint32_t output_guard = 0x5a5a5a5aU;
    for (unsigned index = 0; index < GUARDS; ++index) {
        CHECK(input_buffer.before[index] == 0xBAAD0000U + index);
        CHECK(input_buffer.after[index] == 0xBAAD0000U + index);
        CHECK(optimized_output.before[index] == output_guard);
        CHECK(optimized_output.after[index] == output_guard);
        CHECK(reference_output.before[index] == output_guard);
        CHECK(reference_output.after[index] == output_guard);
    }
}

static yolov8_pp_static_param_t params_for(int classes, int boxes, int limit,
                                          float threshold, float iou)
{
    const yolov8_pp_static_param_t params = {
        .nb_classes = classes, .nb_total_boxes = boxes, .max_boxes_limit = limit,
        .conf_threshold = threshold, .iou_threshold = iou
    };
    return params;
}

static void put_box(unsigned index, float x, float y, float width,
                     float height, float probability)
{
    input_buffer.raw[index] = x;
    input_buffer.raw[BOXES + index] = y;
    input_buffer.raw[2 * BOXES + index] = width;
    input_buffer.raw[3 * BOXES + index] = height;
    input_buffer.raw[4 * BOXES + index] = probability;
}

static od_pp_out_t compare_pipeline(yolov8_pp_static_param_t params)
{
    static float32_t saved_raw[(4 + MAX_CLASSES) * BOXES];
    yolov8_pp_static_param_t old_params = params;
    yolov8_pp_in_centroid_t input = { .pRaw_detections = input_buffer.raw };
    od_pp_out_t actual = { .pOutBuff = optimized_output.boxes, .nb_detect = -1 };
    od_pp_out_t expected = { .pOutBuff = reference_output.boxes, .nb_detect = -1 };
    memcpy(saved_raw, input_buffer.raw, sizeof(saved_raw));
    CHECK(od_yolov8_pp_reset(&params) == 0);
    CHECK(reference_od_yolov8_pp_reset(&old_params) == 0);
    CHECK(od_yolov8_pp_process(&input, &actual, &params) == 0);
    CHECK(reference_od_yolov8_pp_process(&input, &expected, &old_params) == 0);
    CHECK(actual.nb_detect == expected.nb_detect);
    CHECK(params.nb_detect == old_params.nb_detect);
    CHECK(actual.nb_detect >= 0 && actual.nb_detect <= params.max_boxes_limit * params.nb_classes);
    CHECK(memcmp(actual.pOutBuff, expected.pOutBuff,
                 (size_t)actual.nb_detect * sizeof(actual.pOutBuff[0])) == 0);
    CHECK(memcmp(saved_raw, input_buffer.raw, sizeof(saved_raw)) == 0);
    for (int index = 0; index < actual.nb_detect; ++index) {
        const od_pp_outBuffer_t *box = &actual.pOutBuff[index];
        CHECK(isfinite(box->x_center) && isfinite(box->y_center));
        CHECK(box->width > 0 && box->height > 0);
        CHECK(box->conf >= params.conf_threshold && box->conf <= 1);
        CHECK(box->class_index >= 0 && box->class_index < params.nb_classes);
    }
    check_guards();
    return actual;
}

static void test_channel_major_layout(void)
{
    prepare();
    put_box(BOXES - 5U, .25f, .75f, .125f, .5f, .9f);
    put_box(BOXES - 1U, .26f, .75f, .125f, .5f, .8f);
    put_box(0, .8f, .2f, .1f, .1f, .59f);
    const od_pp_out_t result = compare_pipeline(params_for(1, BOXES, 10, .6f, .3f));
    CHECK(result.nb_detect == 1);
    CHECK(result.pOutBuff[0].x_center == .25f);
    CHECK(result.pOutBuff[0].y_center == .75f);
    CHECK(result.pOutBuff[0].width == .125f);
    CHECK(result.pOutBuff[0].height == .5f);
    CHECK(result.pOutBuff[0].conf == .9f && result.pOutBuff[0].class_index == 0);
}

static void test_full_tensor_and_nms_budget(void)
{
    const unsigned candidate_counts[] = {0, 1, 10, BOXES - 1, BOXES};
    for (unsigned scenario = 0; scenario < sizeof(candidate_counts) / sizeof(candidate_counts[0]); ++scenario) {
        prepare();
        for (unsigned index = 0; index < candidate_counts[scenario]; ++index)
            put_box(index, (float)index / BOXES, .5f, .0001f, .02f, .75f);
        optimized_iou_calls = reference_iou_calls = 0;
        const od_pp_out_t result = compare_pipeline(params_for(1, BOXES, 10, .6f, .3f));
        CHECK(result.nb_detect == (int)(candidate_counts[scenario] < 10 ? candidate_counts[scenario] : 10));
        if (candidate_counts[scenario] == BOXES) {
            CHECK(reference_iou_calls == (unsigned long long)BOXES * (BOXES - 1U) / 2U);
            CHECK(optimized_iou_calls == 10U * BOXES - 55U);
            printf("YOLOv8 dense non-overlap IoU calls: original=%llu optimized=%llu (same 10 results)\n",
                   reference_iou_calls, optimized_iou_calls);
        }
    }
}

static void test_randomized_equivalence(void)
{
    const float thresholds[] = {.01f, .35f, .6f, .99f};
    const float ious[] = {0, .3f, .7f, 1};
    for (unsigned scenario = 0; scenario < 1200; ++scenario) {
        prepare();
        const int classes = 1 + (int)(scenario % MAX_CLASSES);
        const int total = (scenario % 19 == 0) ? BOXES : 1 + (int)(random_u32() % BOXES);
        const int limit = (scenario % 23 == 0) ? total : 1 + (int)(random_u32() % 10);
        const float threshold = thresholds[scenario % 4];
        for (int index = 0; index < total; ++index) {
            /* This tensor uses TOTAL as the actual plane stride. Include
             * identical/near-identical boxes, overlapping boxes and ties. */
            input_buffer.raw[index] = (scenario % 7 == 0) ? .5f : random_unit();
            input_buffer.raw[total + index] = (scenario % 7 == 0) ? .5f : random_unit();
            input_buffer.raw[2 * total + index] = (scenario % 7 == 0) ? .3f : .005f + random_unit() * .6f;
            input_buffer.raw[3 * total + index] = (scenario % 7 == 0) ? .5f : .005f + random_unit() * .6f;
            for (int class_index = 0; class_index < classes; ++class_index) {
                float probability = random_unit();
                if (scenario % 5 == 0) probability = .75f; /* exact class/score ties */
                if (index % 31 == 0) probability = threshold; /* inclusive cutoff */
                if (index % 29 == 0) probability = nextafterf(threshold, 0.0f);
                input_buffer.raw[(4 + class_index) * total + index] = probability;
            }
        }
        (void)compare_pipeline(params_for(classes, total, limit, threshold, ious[(scenario / 4) % 4]));
    }
}

static void expect_wrapper_error(void **inputs, int input_count,
                                  od_pp_out_t *output, app_postprocess_params_t *params)
{
    if (output != NULL) output->nb_detect = 77;
    CHECK(app_postprocess_run(inputs, input_count, output, params) == AI_OD_POSTPROCESS_ERROR);
    if (output != NULL) CHECK(output->nb_detect == 0);
}

static void test_app_wrapper(void)
{
    app_postprocess_params_t params;
    od_pp_out_t result = {0};
    void *inputs[] = {input_buffer.raw};
    CHECK(sizeof(params) == sizeof(yolov8_pp_static_param_t));
    CHECK(AI_OBJDETECT_YOLOV8_PP_TOTAL_BOXES == BOXES);
    CHECK(AI_OBJDETECT_YOLOV8_PP_OUTPUT_CHANNELS == CHANNELS);
    CHECK(AI_OBJDETECT_YOLOV8_PP_OUTPUT_FLOATS == CHANNELS * BOXES);
    CHECK(app_postprocess_init(NULL) == AI_OD_POSTPROCESS_ERROR);
    CHECK(app_postprocess_init(&params) == 0);
    prepare();
    put_box(BOXES - 1, .35f, .6f, .2f, .3f, params.conf_threshold);
    CHECK(app_postprocess_run(inputs, 1, &result, &params) == 0);
    CHECK(result.nb_detect == 1 && result.pOutBuff[0].x_center == .35f);
    CHECK(result.pOutBuff[0].y_center == .6f && result.pOutBuff[0].width == .2f);
    CHECK(result.pOutBuff[0].conf == params.conf_threshold);
    for (unsigned count = 0; count <= BOXES; ++count) {
        /* Every prefix size exercises the last FLOAT plane and full scratch
         * capacity; small non-overlap boxes should retain the highest 10. */
        prepare();
        for (unsigned index = 0; index < count; ++index)
            put_box(index, (float)index / BOXES, .5f, .0001f, .02f, .75f);
        CHECK(app_postprocess_run(inputs, 1, &result, &params) == 0);
        CHECK(result.nb_detect == (int)(count < 10 ? count : 10));
        for (unsigned guard = 0; guard < GUARDS; ++guard) {
            CHECK(input_buffer.before[guard] == 0xBAAD0000U + guard);
            CHECK(input_buffer.after[guard] == 0xBAAD0000U + guard);
        }
    }

    prepare();
    expect_wrapper_error(NULL, 1, &result, &params);
    inputs[0] = NULL; expect_wrapper_error(inputs, 1, &result, &params);
    inputs[0] = input_buffer.raw;
    expect_wrapper_error(inputs, 0, &result, &params);
    expect_wrapper_error(inputs, 2, &result, &params);
    expect_wrapper_error(inputs, 1, NULL, &params);
    expect_wrapper_error(inputs, 1, &result, NULL);
    const app_postprocess_params_t valid = params;
    params.nb_total_boxes = BOXES + 1; expect_wrapper_error(inputs, 1, &result, &params); params = valid;
    params.nb_total_boxes = 0; expect_wrapper_error(inputs, 1, &result, &params); params = valid;
    params.nb_classes = 2; expect_wrapper_error(inputs, 1, &result, &params); params = valid;
    params.max_boxes_limit = 11; expect_wrapper_error(inputs, 1, &result, &params); params = valid;
    params.max_boxes_limit = 0; expect_wrapper_error(inputs, 1, &result, &params); params = valid;
    params.conf_threshold = NAN; expect_wrapper_error(inputs, 1, &result, &params); params = valid;
    params.conf_threshold = 0; expect_wrapper_error(inputs, 1, &result, &params); params = valid;
    params.conf_threshold = 1.1f; expect_wrapper_error(inputs, 1, &result, &params); params = valid;
    params.iou_threshold = INFINITY; expect_wrapper_error(inputs, 1, &result, &params); params = valid;
    params.iou_threshold = -.1f; expect_wrapper_error(inputs, 1, &result, &params); params = valid;
    params.iou_threshold = 1.1f; expect_wrapper_error(inputs, 1, &result, &params); params = valid;
    put_box(BOXES - 1, .35f, .6f, .2f, .3f, .75f);
    const unsigned bad_offsets[] = {BOXES - 1, 2 * BOXES - 1, 3 * BOXES - 1, 4 * BOXES - 1, 5 * BOXES - 1};
    for (unsigned index = 0; index < sizeof(bad_offsets) / sizeof(bad_offsets[0]); ++index) {
        const float saved = input_buffer.raw[bad_offsets[index]];
        input_buffer.raw[bad_offsets[index]] = NAN;
        expect_wrapper_error(inputs, 1, &result, &params);
        input_buffer.raw[bad_offsets[index]] = INFINITY;
        expect_wrapper_error(inputs, 1, &result, &params);
        input_buffer.raw[bad_offsets[index]] = saved;
    }
    input_buffer.raw[3 * BOXES - 1] = 0; expect_wrapper_error(inputs, 1, &result, &params);
    input_buffer.raw[3 * BOXES - 1] = -.2f; expect_wrapper_error(inputs, 1, &result, &params);
    input_buffer.raw[3 * BOXES - 1] = .2f;
    input_buffer.raw[4 * BOXES - 1] = 0; expect_wrapper_error(inputs, 1, &result, &params);
    input_buffer.raw[4 * BOXES - 1] = .3f;
    input_buffer.raw[5 * BOXES - 1] = -.1f; expect_wrapper_error(inputs, 1, &result, &params);
    input_buffer.raw[5 * BOXES - 1] = 1.1f; expect_wrapper_error(inputs, 1, &result, &params);
    /* Failed input must not leave last-frame detections visible. */
    prepare();
    CHECK(app_postprocess_run(inputs, 1, &result, &params) == 0 && result.nb_detect == 0);
    check_guards();
}

static void test_real_model_outputs(const char *directory)
{
    const char *samples[] = {"black", "white", "gradient", "seeded_noise"};
    for (unsigned sample = 0; sample < sizeof(samples) / sizeof(samples[0]); ++sample) {
        char path[1024];
        CHECK(snprintf(path, sizeof(path), "%s/%s-output.f32", directory, samples[sample]) > 0);
        FILE *file = fopen(path, "rb");
        CHECK(file != NULL);
        prepare();
        CHECK(fread(input_buffer.raw, sizeof(float32_t), CHANNELS * BOXES, file) == CHANNELS * BOXES);
        CHECK(fgetc(file) == EOF && !ferror(file));
        CHECK(fclose(file) == 0);
        app_postprocess_params_t params;
        CHECK(app_postprocess_init(&params) == 0);
        od_pp_out_t result = {0};
        void *inputs[] = {input_buffer.raw};
        CHECK(app_postprocess_run(inputs, 1, &result, &params) == 0);
        /* These actual synthetic-image tensors have low scores. This checks
         * the data contract; it does not establish person-detection accuracy. */
        CHECK(result.nb_detect == 0);
        (void)compare_pipeline(params);
        if (strcmp(samples[sample], "gradient") == 0) {
            params.conf_threshold = .1f; /* Test-only probe, never a production calibration. */
            const od_pp_out_t expected = compare_pipeline(params);
            CHECK(expected.nb_detect > 0);
            CHECK(app_postprocess_run(inputs, 1, &result, &params) == 0);
            CHECK(result.nb_detect == expected.nb_detect);
            CHECK(memcmp(result.pOutBuff, expected.pOutBuff,
                         (size_t)result.nb_detect * sizeof(result.pOutBuff[0])) == 0);
            printf("Real gradient FLOAT tensor: default zero results; test-only conf=.1 -> %ld results matching frozen NMS\n", (long)result.nb_detect);
        }
    }
}

int main(int argc, char **argv)
{
    test_channel_major_layout();
    test_full_tensor_and_nms_budget();
    test_randomized_equivalence();
    test_app_wrapper();
    if (argc == 2) test_real_model_outputs(argv[1]);
    printf("YOLOv8 FLOAT [1,5,1344] tests passed: %llu assertions; 1200 randomized/multi-class/tie NMS comparisons; 1345 prefix capacities; malformed-input guards; real tensors=%s\n", assertions, argc == 2 ? "4" : "not provided");
    return 0;
}
