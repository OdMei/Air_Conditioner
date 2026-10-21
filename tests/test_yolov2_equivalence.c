#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "od_yolov2_pp_if.h"

/* The reference is an unoptimized v6 source snapshot compiled separately. */
int32_t v6_od_yolov2_pp_reset(yolov2_pp_static_param_t *params);
int32_t v6_od_yolov2_pp_process(yolov2_pp_in_t *input, od_pp_out_t *output,
                              yolov2_pp_static_param_t *params);

enum { GRID = 7, ANCHORS = 5, CANDIDATES = GRID * GRID * ANCHORS,
       MAX_CLASSES = 3, MAX_STRIDE = 5 + MAX_CLASSES, GUARD_WORDS = 16 };
enum { RANDOM_SIGNAL, DENSE_SEPARATE, DENSE_OVERLAP, TIED_SCORES,
       THRESHOLD_SIGNAL, EMPTY_SIGNAL };

typedef struct {
    uint32_t before[GUARD_WORDS];
    float raw[CANDIDATES * MAX_STRIDE];
    uint32_t after[GUARD_WORDS];
} guarded_input_t;
typedef struct {
    uint32_t before[GUARD_WORDS];
    od_pp_outBuffer_t boxes[CANDIDATES];
    uint32_t after[GUARD_WORDS];
} guarded_output_t;

static guarded_input_t optimized_input, reference_input;
static guarded_output_t optimized_output, reference_output;
static uint32_t random_state = UINT32_C(0xB16B00B5);
static unsigned int checked_cases;
static unsigned int observed_score_rounding;

static uint32_t random_u32(void)
{
    random_state ^= random_state << 13;
    random_state ^= random_state >> 17;
    random_state ^= random_state << 5;
    return random_state;
}

static float random_float(float minimum, float maximum)
{
    return minimum + (maximum - minimum) * ((random_u32() >> 8) / 16777216.0f);
}

static void check_guard(const uint32_t *guard)
{
    for (unsigned int index = 0; index < GUARD_WORDS; index++)
    {
        assert(guard[index] == UINT32_C(0xA55AA55A));
    }
}

static void initialize_signal(int classes, unsigned int candidates, int kind)
{
    const unsigned int stride = 5U + (unsigned int)classes;
    memset(&optimized_input, 0x5A, sizeof(optimized_input));
    memset(&optimized_output, 0x5A, sizeof(optimized_output));
    for (unsigned int index = 0; index < GUARD_WORDS; index++)
    {
        optimized_input.before[index] = optimized_input.after[index] = UINT32_C(0xA55AA55A);
        optimized_output.before[index] = optimized_output.after[index] = UINT32_C(0xA55AA55A);
    }
    for (unsigned int index = 0; index < CANDIDATES; index++)
    {
        float *box = &optimized_input.raw[index * stride];
        for (unsigned int element = 0; element < stride; element++) { box[element] = 0.0f; }
        box[4] = index < candidates ? 8.0f : -12.0f;
        if (kind == RANDOM_SIGNAL)
        {
            box[0] = random_float(-3.0f, 3.0f); box[1] = random_float(-3.0f, 3.0f);
            box[2] = random_float(-3.0f, 2.0f); box[3] = random_float(-3.0f, 2.0f);
            box[4] = index < candidates ? random_float(-3.0f, 7.0f) : -12.0f;
            for (int class_index = 0; class_index < classes; class_index++)
            { box[5 + class_index] = random_float(-4.0f, 4.0f); }
        }
        else if (kind == DENSE_SEPARATE || kind == TIED_SCORES)
        {
            box[2] = -5.0f; box[3] = -5.0f;
            /* Equal scores exercise the real qsort tie behavior. */
            box[4] = index < candidates ? (kind == TIED_SCORES ? 5.0f : 8.0f - 0.015f * index) : -12.0f;
            if (classes > 1) { box[5 + index % (unsigned int)classes] = 4.0f; }
        }
        else if (kind == DENSE_OVERLAP)
        {
            box[2] = 3.0f; box[3] = 3.0f;
            box[4] = index < candidates ? 8.0f - 0.015f * index : -12.0f;
            if (classes > 1) { box[5 + index % (unsigned int)classes] = 4.0f; }
        }
        else if (kind == THRESHOLD_SIGNAL)
        {
            /* Symmetric objectness cases below/equal/above 0.5.  A zero
             * one-class logit makes reference softmax exactly one. */
            box[2] = -5.0f; box[3] = -5.0f;
            box[4] = index < candidates ? ((int)(index % 3U) - 1) * 0.002f : -12.0f;
        }
        else if (kind == EMPTY_SIGNAL) { box[4] = -12.0f; }
    }
    reference_input = optimized_input; reference_output = optimized_output;
}

static void assert_same_box(const od_pp_outBuffer_t *actual, const od_pp_outBuffer_t *expected,
                            unsigned int index, int classes)
{
    if (actual->class_index != expected->class_index || actual->x_center != expected->x_center ||
        actual->y_center != expected->y_center || actual->width != expected->width ||
        actual->height != expected->height || fabsf(actual->conf - expected->conf) > 4.0f * FLT_EPSILON)
    {
        fprintf(stderr, "YOLOv2 difference case=%u box=%u actual=(%g,%g,%g,%g,%g,%ld) reference=(%g,%g,%g,%g,%g,%ld)\n",
            checked_cases, index, actual->x_center, actual->y_center, actual->width, actual->height,
            actual->conf, (long)actual->class_index, expected->x_center, expected->y_center,
            expected->width, expected->height, expected->conf, (long)expected->class_index);
        abort();
    }
    /* The multi-class path has no numerical simplification. */
    if (classes > 1) { assert(actual->conf == expected->conf); }
    if (actual->conf != expected->conf) { observed_score_rounding++; }
}

static void run_case(int classes, unsigned int candidates, int kind, int max_boxes,
                     float threshold, float iou, uint8_t in_place)
{
    const float anchors[ANCHORS * 2] = {1, 1, 2, 2, 3, 3, 4, 4, 5, 5};
    const unsigned int used_floats = CANDIDATES * (5U + (unsigned int)classes);
    yolov2_pp_static_param_t params = {
        .nb_classes = classes, .nb_anchors = ANCHORS, .grid_width = GRID, .grid_height = GRID,
        .nb_input_boxes = CANDIDATES, .max_boxes_limit = max_boxes, .conf_threshold = threshold,
        .iou_threshold = iou, .pAnchors = anchors
    };
    yolov2_pp_static_param_t old_params = params;
    yolov2_pp_in_t input = {.pRaw_detections = optimized_input.raw};
    yolov2_pp_in_t old_input = {.pRaw_detections = reference_input.raw};
    od_pp_out_t output = {.pOutBuff = in_place ? NULL : optimized_output.boxes, .nb_detect = -1};
    od_pp_out_t old_output = {.pOutBuff = in_place ? NULL : reference_output.boxes, .nb_detect = -1};
    initialize_signal(classes, candidates, kind);
    assert(od_yolov2_pp_reset(&params) == v6_od_yolov2_pp_reset(&old_params));
    assert(od_yolov2_pp_process(&input, &output, &params) ==
           v6_od_yolov2_pp_process(&old_input, &old_output, &old_params));
    assert(params.nb_detect == old_params.nb_detect);
    if (output.nb_detect != old_output.nb_detect)
    {
        fprintf(stderr, "YOLOv2 count difference case=%u classes=%d kind=%d max=%d actual=%ld reference=%ld\n",
            checked_cases, classes, kind, max_boxes, (long)output.nb_detect, (long)old_output.nb_detect);
        abort();
    }
    assert(output.nb_detect >= 0 && output.nb_detect <= CANDIDATES);
    for (int32_t index = 0; index < output.nb_detect; index++)
    { assert_same_box(&output.pOutBuff[index], &old_output.pOutBuff[index], (unsigned int)index, classes); }
    check_guard(optimized_input.before); check_guard(optimized_input.after);
    check_guard(reference_input.before); check_guard(reference_input.after);
    check_guard(optimized_output.before); check_guard(optimized_output.after);
    check_guard(reference_output.before); check_guard(reference_output.after);
    /* Detect writes beyond a smaller single-/two-class tensor as well. */
    for (unsigned int index = used_floats; index < CANDIDATES * MAX_STRIDE; index++)
    {
        uint32_t actual_word, reference_word;
        memcpy(&actual_word, &optimized_input.raw[index], sizeof(actual_word));
        memcpy(&reference_word, &reference_input.raw[index], sizeof(reference_word));
        assert(actual_word == UINT32_C(0x5A5A5A5A) && reference_word == UINT32_C(0x5A5A5A5A));
    }
    if (!in_place)
    {
        const unsigned char *tail = (const unsigned char *)&optimized_output.boxes[output.nb_detect];
        const unsigned char *old_tail = (const unsigned char *)&reference_output.boxes[old_output.nb_detect];
        size_t tail_length = (CANDIDATES - (size_t)output.nb_detect) * sizeof(output.pOutBuff[0]);
        for (size_t index = 0; index < tail_length; index++)
        { assert(tail[index] == 0x5A && old_tail[index] == 0x5A); }
    }
    checked_cases++;
}

static void verify_one_class_rounding_boundary(void)
{
    const float anchors[ANCHORS * 2] = {1, 1, 2, 2, 3, 3, 4, 4, 5, 5};
    yolov2_pp_static_param_t params = {
        .nb_classes = 1, .nb_anchors = ANCHORS, .grid_width = GRID, .grid_height = GRID,
        .nb_input_boxes = CANDIDATES, .max_boxes_limit = 10, .conf_threshold = 0.6f,
        .iou_threshold = 1.0f, .pAnchors = anchors
    };
    initialize_signal(1, 1, DENSE_SEPARATE);
    optimized_input.raw[4] = logf(0.6f / 0.4f);
    while (1.0f / (1.0f + expf(-optimized_input.raw[4])) < params.conf_threshold)
    { optimized_input.raw[4] = nextafterf(optimized_input.raw[4], INFINITY); }
    for (unsigned int trial = 0; trial < 10000; trial++)
    {
        const float class_logit = -5.0f + trial / 1000.0f;
        const float exponent = expf(class_logit);
        const float old_probability = exponent * (1.0f / exponent);
        if (old_probability < 1.0f)
        {
            optimized_input.raw[5] = class_logit;
            reference_input = optimized_input;
            yolov2_pp_static_param_t old_params = params;
            yolov2_pp_in_t input = {.pRaw_detections = optimized_input.raw};
            yolov2_pp_in_t old_input = {.pRaw_detections = reference_input.raw};
            od_pp_out_t output = {.pOutBuff = optimized_output.boxes};
            od_pp_out_t old_output = {.pOutBuff = reference_output.boxes};
            assert(od_yolov2_pp_process(&input, &output, &params) == 0);
            assert(v6_od_yolov2_pp_process(&old_input, &old_output, &old_params) == 0);
            /* An intentional mathematical correction, not an equivalence
             * assertion hidden inside a floating point tolerance: one-class
             * probability is one, so >= threshold must retain this box. */
            assert(output.nb_detect == 1 && old_output.nb_detect == 0);
            assert(output.pOutBuff[0].conf == params.conf_threshold);
            printf("Explicit single-class boundary correction passed: class_logit=%g old_probability=%.9g optimized_count=%ld v6_count=%ld at threshold 0.6.\n",
                class_logit, old_probability, (long)output.nb_detect, (long)old_output.nb_detect);

            /* The same old rounding may artificially distinguish equal
             * objectness scores.  The corrected result must retain both
             * geometries and give them equal confidence; tie order belongs
             * to qsort, not to an unstable one-class softmax rounding. */
            initialize_signal(1, 2, DENSE_SEPARATE);
            optimized_input.raw[4] = 5.0f;
            optimized_input.raw[5] = class_logit;
            optimized_input.raw[10] = 5.0f;
            optimized_input.raw[11] = 0.0f;
            reference_input = optimized_input;
            old_params = params;
            output.nb_detect = old_output.nb_detect = -1;
            assert(od_yolov2_pp_process(&input, &output, &params) == 0);
            assert(v6_od_yolov2_pp_process(&old_input, &old_output, &old_params) == 0);
            assert(output.nb_detect == 2 && old_output.nb_detect == 2);
            assert(output.pOutBuff[0].conf == output.pOutBuff[1].conf);
            assert(old_output.pOutBuff[0].conf > old_output.pOutBuff[1].conf);
            assert((output.pOutBuff[0].width == old_output.pOutBuff[0].width &&
                    output.pOutBuff[1].width == old_output.pOutBuff[1].width) ||
                   (output.pOutBuff[0].width == old_output.pOutBuff[1].width &&
                    output.pOutBuff[1].width == old_output.pOutBuff[0].width));
            printf("Explicit single-class tie correction passed: old scores differ by rounding; corrected scores tie; same two geometries (order changed=%u).\n",
                output.pOutBuff[0].width != old_output.pOutBuff[0].width);
            return;
        }
    }
    abort();
}

static double benchmark(uint8_t optimized, int kind)
{
    const float anchors[ANCHORS * 2] = {1, 1, 2, 2, 3, 3, 4, 4, 5, 5};
    const unsigned int repetitions = 1000;
    const clock_t started = clock();
    for (unsigned int repeat = 0; repeat < repetitions; repeat++)
    {
        initialize_signal(1, CANDIDATES, kind);
        yolov2_pp_static_param_t params = {
            .nb_classes = 1, .nb_anchors = ANCHORS, .grid_width = GRID, .grid_height = GRID,
            .nb_input_boxes = CANDIDATES, .max_boxes_limit = 10, .conf_threshold = 0.6f,
            .iou_threshold = 0.3f, .pAnchors = anchors
        };
        yolov2_pp_in_t input = {.pRaw_detections = optimized_input.raw};
        od_pp_out_t output = {.pOutBuff = optimized_output.boxes};
        if (optimized) { assert(od_yolov2_pp_process(&input, &output, &params) == 0); }
        else { assert(v6_od_yolov2_pp_process(&input, &output, &params) == 0); }
    }
    return 1000.0 * (clock() - started) / CLOCKS_PER_SEC;
}

int main(void)
{
    const unsigned int candidate_counts[] = {0, 1, 10, 244, 245};
    const int limits[] = {-1, 0, 1, 10, 245, 246};
    const float ious[] = {0.0f, 0.3f, 1.0f};
    for (int classes = 1; classes <= MAX_CLASSES; classes++)
    {
        for (unsigned int count = 0; count < sizeof(candidate_counts) / sizeof(candidate_counts[0]); count++)
        {
            for (int kind = DENSE_SEPARATE; kind <= EMPTY_SIGNAL; kind++)
            {
                for (unsigned int limit = 0; limit < sizeof(limits) / sizeof(limits[0]); limit++)
                {
                    for (unsigned int iou = 0; iou < sizeof(ious) / sizeof(ious[0]); iou++)
                    {
                        for (uint8_t in_place = 0; in_place <= 1; in_place++)
                        {
                            run_case(classes, candidate_counts[count], kind, limits[limit],
                                kind == THRESHOLD_SIGNAL ? 0.5f : 0.6f, ious[iou], in_place);
                        }
                    }
                }
            }
        }
    }
    for (unsigned int repeat = 0; repeat < 2000; repeat++)
    {
        const int classes = 1 + (int)(repeat % MAX_CLASSES);
        const unsigned int candidates = random_u32() % (CANDIDATES + 1U);
        run_case(classes, candidates, RANDOM_SIGNAL, limits[repeat % 6U],
            repeat % 2U ? 0.6f : 0.25f, ious[repeat % 3U], (uint8_t)(repeat % 2U));
    }
    printf("YOLOv2 equivalence passed: %u cases; v6 snapshot; 1/2/3 classes; 0/1/10/244/245 candidates; limits -1/0/1/10/245/246; random/dense/overlap/ties/threshold; ordered boxes; guards; in/out-place.\n", checked_cases);
    printf("Single-class confidence differences within 4 FLT_EPSILON: %u (softmax rounding eliminated).\n", observed_score_rounding);
    verify_one_class_rounding_boundary();
    printf("Host benchmark (1000 cases, includes fixture setup; not board timing): dense v6=%.1fms optimized=%.1fms; overlap v6=%.1fms optimized=%.1fms\n",
        benchmark(0, DENSE_SEPARATE), benchmark(1, DENSE_SEPARATE),
        benchmark(0, DENSE_OVERLAP), benchmark(1, DENSE_OVERLAP));
    return 0;
}
