/*---------------------------------------------------------------------------------------------
#  * Copyright (c) 2023 STMicroelectronics.
#  * All rights reserved.
#  *
#  * This software is licensed under terms that can be found in the LICENSE file in
#  * the root directory of this software component.
#  * If no LICENSE file comes with this software, it is provided AS-IS.
#  *--------------------------------------------------------------------------------------------*/

/* ---------------    Generated code    ----------------- */
#ifndef __POSTPROCESS_CONF_H__
#define __POSTPROCESS_CONF_H__

#include "arm_math.h"
#include "app_model_config.h"

/* I/O configuration */
#define AI_OBJDETECT_YOLOV2_PP_NB_CLASSES        (1)
#define AI_OBJDETECT_YOLOV2_PP_NB_ANCHORS        (5)
#define AI_OBJDETECT_YOLOV2_PP_GRID_WIDTH        (7)
#define AI_OBJDETECT_YOLOV2_PP_GRID_HEIGHT       (7)
#define AI_OBJDETECT_YOLOV2_PP_NB_INPUT_BOXES    (AI_OBJDETECT_YOLOV2_PP_GRID_WIDTH * AI_OBJDETECT_YOLOV2_PP_GRID_HEIGHT)

/* Anchor boxes */
static const float32_t AI_OBJDETECT_YOLOV2_PP_ANCHORS[2*AI_OBJDETECT_YOLOV2_PP_NB_ANCHORS] = {
    0.9883000000f,     3.3606000000f,
    2.1194000000f,     5.3759000000f,
    3.0520000000f,     9.1336000000f,
    5.5517000000f,     9.3066000000f,
    9.7260000000f,     11.1422000000f,
  };

/* ST YOLOv8n person model: decoded, normalized centroid coordinates followed
 * by class probabilities. The FLOAT output is channel-major [1, 5, 1344]; it
 * contains no YOLOv2 objectness channel or anchor-grid values to decode. */
#define AI_OBJDETECT_YOLOV8_PP_INPUT_WIDTH       APP_MODEL_INPUT_WIDTH
#define AI_OBJDETECT_YOLOV8_PP_INPUT_HEIGHT      APP_MODEL_INPUT_HEIGHT
#define AI_OBJDETECT_YOLOV8_PP_NB_CLASSES        APP_MODEL_CLASSES
#define AI_OBJDETECT_YOLOV8_PP_TOTAL_BOXES       APP_MODEL_OUTPUT_BOXES
#define AI_OBJDETECT_YOLOV8_PP_OUTPUT_CHANNELS   APP_MODEL_OUTPUT_CHANNELS
#define AI_OBJDETECT_YOLOV8_PP_OUTPUT_FLOATS     (AI_OBJDETECT_YOLOV8_PP_OUTPUT_CHANNELS * AI_OBJDETECT_YOLOV8_PP_TOTAL_BOXES)

#endif      /* __POSTPROCESS_CONF_H__  */

