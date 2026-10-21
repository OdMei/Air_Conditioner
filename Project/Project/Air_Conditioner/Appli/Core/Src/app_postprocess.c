 /**
 ******************************************************************************
 * @file    app_postprocess.c
 * @author  GPM Application Team
 *
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2023 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */


#include "app_postprocess.h"
#include "app_config.h"
#include <assert.h>
#include <math.h>

#if POSTPROCESS_TYPE == POSTPROCESS_OD_YOLO_V5_UU
static od_pp_outBuffer_t out_detections[AI_OBJDETECT_YOLOV5_PP_TOTAL_BOXES];
#elif POSTPROCESS_TYPE == POSTPROCESS_OD_YOLO_V8_UF || POSTPROCESS_TYPE == POSTPROCESS_OD_YOLO_V8_UI
static od_pp_outBuffer_t out_detections[AI_OBJDETECT_YOLOV8_PP_TOTAL_BOXES];
#elif POSTPROCESS_TYPE == POSTPROCESS_OD_ST_SSD_UF
static od_pp_outBuffer_t out_detections[AI_OD_SSD_ST_PP_TOTAL_DETECTIONS];
#elif POSTPROCESS_TYPE == POSTPROCESS_MPE_YOLO_V8_UF
static mpe_pp_outBuffer_t out_detections[AI_MPE_YOLOV8_PP_TOTAL_BOXES];
static mpe_pp_keyPoints_t out_keyPoints[AI_MPE_YOLOV8_PP_TOTAL_BOXES * AI_POSE_PP_POSE_KEYPOINTS_NB];
#elif POSTPROCESS_TYPE == POSTPROCESS_MPE_PD_UF
/* Must be in app code */
#include "pd_anchors.c"
/* post process algo will not write more than AI_PD_MODEL_PP_MAX_BOXES_LIMIT */
static pd_pp_box_t out_detections[AI_PD_MODEL_PP_MAX_BOXES_LIMIT];
static pd_pp_point_t out_keyPoints[AI_PD_MODEL_PP_MAX_BOXES_LIMIT][AI_PD_MODEL_PP_NB_KEYPOINTS];
#elif POSTPROCESS_TYPE == POSTPROCESS_SPE_MOVENET_UF
static spe_pp_outBuffer_t out_detections[AI_POSE_PP_POSE_KEYPOINTS_NB];
#elif POSTPROCESS_TYPE == POSTPROCESS_ISEG_YOLO_V8_UI
uint8_t _iseg_mask[AI_YOLOV8_SEG_PP_MASK_SIZE * AI_YOLOV8_SEG_PP_MASK_SIZE * AI_YOLOV8_SEG_PP_MAX_BOXES_LIMIT];
iseg_postprocess_outBuffer_t out_detections[AI_YOLOV8_SEG_PP_MAX_BOXES_LIMIT];
iseg_postprocess_scratchBuffer_s8_t scratch_detections[AI_YOLOV8_SEG_PP_TOTAL_BOXES];
float32_t _out_buf_mask[AI_YOLOV8_SEG_PP_MASK_NB];
int8_t _out_buf_mask_s8[AI_YOLOV8_SEG_PP_MASK_NB * AI_YOLOV8_SEG_PP_TOTAL_BOXES];
#elif POSTPROCESS_TYPE == POSTPROCESS_SSEG_DEEPLAB_V3_UF
static uint8_t out_sseg_map[AI_SSEG_DEEPLABV3_PP_WIDTH * AI_SSEG_DEEPLABV3_PP_HEIGHT];
#endif

#if POSTPROCESS_TYPE == POSTPROCESS_OD_YOLO_V8_UF || POSTPROCESS_TYPE == POSTPROCESS_OD_YOLO_V8_UI
/* The vendor decoder writes every passing candidate before NMS, so its
 * scratch capacity is TOTAL_BOXES, rather than the public result limit. */
_Static_assert(AI_OBJDETECT_YOLOV8_PP_NB_CLASSES > 0, "YOLOv8 needs at least one class");
_Static_assert(AI_OBJDETECT_YOLOV8_PP_TOTAL_BOXES > 0, "YOLOv8 scratch capacity cannot be zero");
_Static_assert(AI_OBJDETECT_YOLOV8_PP_MAX_BOXES_LIMIT > 0 &&
               AI_OBJDETECT_YOLOV8_PP_MAX_BOXES_LIMIT <= AI_OBJDETECT_YOLOV8_PP_TOTAL_BOXES,
               "YOLOv8 public result limit exceeds scratch capacity");

static int app_yolov8_params_valid(const yolov8_pp_static_param_t *params)
{
  return params != NULL &&
         params->nb_classes == AI_OBJDETECT_YOLOV8_PP_NB_CLASSES &&
         params->nb_total_boxes == AI_OBJDETECT_YOLOV8_PP_TOTAL_BOXES &&
         params->max_boxes_limit > 0 &&
         params->max_boxes_limit <= AI_OBJDETECT_YOLOV8_PP_MAX_BOXES_LIMIT &&
         isfinite(params->conf_threshold) && params->conf_threshold > 0.0f &&
         params->conf_threshold <= 1.0f && isfinite(params->iou_threshold) &&
         params->iou_threshold >= 0.0f && params->iou_threshold <= 1.0f;
}
#endif

#if POSTPROCESS_TYPE == POSTPROCESS_OD_YOLO_V8_UF
/* Reject malformed tensor data before qsort/NMS can see NaNs or invalid
 * areas. Coordinates may extend outside the image: clipping belongs to the
 * display mapper, while probabilities must already be sigmoid output. */
static int app_yolov8_float_input_valid(const float32_t *raw,
                                       const yolov8_pp_static_param_t *params)
{
  const int32_t boxes = params->nb_total_boxes;
  for (int32_t index = 0; index < boxes; ++index)
  {
    float32_t best = 0.0f;
    for (int32_t class_index = 0; class_index < params->nb_classes; ++class_index)
    {
      const float32_t probability = raw[(4 + class_index) * boxes + index];
      if (!isfinite(probability) || probability < 0.0f || probability > 1.0f)
        return 0;
      if (probability > best) best = probability;
    }
    if (best >= params->conf_threshold)
    {
      if (!isfinite(raw[index]) || !isfinite(raw[boxes + index]) ||
          !isfinite(raw[2 * boxes + index]) || !isfinite(raw[3 * boxes + index]) ||
          raw[2 * boxes + index] <= 0.0f || raw[3 * boxes + index] <= 0.0f)
        return 0;
    }
  }
  return 1;
}
#endif

int32_t app_postprocess_init(void *params_postprocess)
{
  if (params_postprocess == NULL) return AI_OD_POSTPROCESS_ERROR;
#if POSTPROCESS_TYPE == POSTPROCESS_OD_YOLO_V2_UF
  int32_t error = AI_OD_POSTPROCESS_ERROR_NO;
  yolov2_pp_static_param_t *params = (yolov2_pp_static_param_t *) params_postprocess;
  params->conf_threshold = AI_OBJDETECT_YOLOV2_PP_CONF_THRESHOLD;
  params->iou_threshold = AI_OBJDETECT_YOLOV2_PP_IOU_THRESHOLD;
  params->nb_anchors = AI_OBJDETECT_YOLOV2_PP_NB_ANCHORS;
  params->nb_classes = AI_OBJDETECT_YOLOV2_PP_NB_CLASSES;
  params->grid_height = AI_OBJDETECT_YOLOV2_PP_GRID_HEIGHT;
  params->grid_width = AI_OBJDETECT_YOLOV2_PP_GRID_WIDTH;
  params->nb_input_boxes = AI_OBJDETECT_YOLOV2_PP_NB_INPUT_BOXES;
  params->pAnchors = AI_OBJDETECT_YOLOV2_PP_ANCHORS;
  params->max_boxes_limit = AI_OBJDETECT_YOLOV2_PP_MAX_BOXES_LIMIT;
  error = od_yolov2_pp_reset(params);
#elif POSTPROCESS_TYPE == POSTPROCESS_OD_YOLO_V5_UU
  int32_t error = AI_OD_POSTPROCESS_ERROR_NO;
  yolov5_pp_static_param_t *params = (yolov5_pp_static_param_t *) params_postprocess;
  params->nb_classes = AI_OBJDETECT_YOLOV5_PP_NB_CLASSES;
  params->nb_total_boxes = AI_OBJDETECT_YOLOV5_PP_TOTAL_BOXES;
  params->max_boxes_limit = AI_OBJDETECT_YOLOV5_PP_MAX_BOXES_LIMIT;
  params->conf_threshold = AI_OBJDETECT_YOLOV5_PP_CONF_THRESHOLD;
  params->iou_threshold = AI_OBJDETECT_YOLOV5_PP_IOU_THRESHOLD;
  params->raw_output_scale = AI_OBJDETECT_YOLOV5_PP_SCALE;
  params->raw_output_zero_point = AI_OBJDETECT_YOLOV5_PP_ZERO_POINT;
  error = od_yolov5_pp_reset(params);
#elif POSTPROCESS_TYPE == POSTPROCESS_OD_YOLO_V8_UF || POSTPROCESS_TYPE == POSTPROCESS_OD_YOLO_V8_UI
  int32_t error = AI_OD_POSTPROCESS_ERROR_NO;
  yolov8_pp_static_param_t *params = (yolov8_pp_static_param_t *) params_postprocess;
  params->nb_classes = AI_OBJDETECT_YOLOV8_PP_NB_CLASSES;
  params->nb_total_boxes = AI_OBJDETECT_YOLOV8_PP_TOTAL_BOXES;
  params->max_boxes_limit = AI_OBJDETECT_YOLOV8_PP_MAX_BOXES_LIMIT;
  params->conf_threshold = AI_OBJDETECT_YOLOV8_PP_CONF_THRESHOLD;
  params->iou_threshold = AI_OBJDETECT_YOLOV8_PP_IOU_THRESHOLD;
#if POSTPROCESS_TYPE == POSTPROCESS_OD_YOLO_V8_UI
  params->raw_output_scale = AI_OBJDETECT_YOLOV8_PP_SCALE;
  params->raw_output_zero_point = AI_OBJDETECT_YOLOV8_PP_ZERO_POINT;
#else
  params->raw_output_scale = 0;
  params->raw_output_zero_point = 0;
#endif
  if (!app_yolov8_params_valid(params)) return AI_OD_POSTPROCESS_ERROR;
  error = od_yolov8_pp_reset(params);
#elif POSTPROCESS_TYPE == POSTPROCESS_OD_ST_YOLOX_UF
  int32_t error = AI_OD_POSTPROCESS_ERROR_NO;
  st_yolox_pp_static_param_t *params = (st_yolox_pp_static_param_t *) params_postprocess;
  params->nb_classes = AI_OBJDETECT_YOLOVX_PP_NB_CLASSES;
  params->nb_anchors = AI_OBJDETECT_YOLOVX_PP_NB_ANCHORS;
  params->grid_width_L = AI_OBJDETECT_YOLOVX_PP_L_GRID_WIDTH;
  params->grid_height_L = AI_OBJDETECT_YOLOVX_PP_L_GRID_HEIGHT;
  params->grid_width_M = AI_OBJDETECT_YOLOVX_PP_M_GRID_WIDTH;
  params->grid_height_M = AI_OBJDETECT_YOLOVX_PP_M_GRID_HEIGHT;
  params->grid_width_S = AI_OBJDETECT_YOLOVX_PP_S_GRID_WIDTH;
  params->grid_height_S = AI_OBJDETECT_YOLOVX_PP_S_GRID_HEIGHT;
  params->pAnchors_L = AI_OBJDETECT_YOLOVX_PP_L_ANCHORS;
  params->pAnchors_M = AI_OBJDETECT_YOLOVX_PP_M_ANCHORS;
  params->pAnchors_S = AI_OBJDETECT_YOLOVX_PP_S_ANCHORS;
  params->max_boxes_limit = AI_OBJDETECT_YOLOVX_PP_MAX_BOXES_LIMIT;
  params->conf_threshold = AI_OBJDETECT_YOLOVX_PP_CONF_THRESHOLD;
  params->iou_threshold = AI_OBJDETECT_YOLOVX_PP_IOU_THRESHOLD;
  error = od_st_yolox_pp_reset(params);
#elif POSTPROCESS_TYPE == POSTPROCESS_OD_ST_SSD_UF
  int32_t error = AI_OD_POSTPROCESS_ERROR_NO;
  ssd_st_pp_static_param_t *params = (ssd_st_pp_static_param_t *) params_postprocess;
  params->nb_classes = AI_OD_SSD_ST_PP_NB_CLASSES;
  params->nb_detections = AI_OD_SSD_ST_PP_TOTAL_DETECTIONS;
  params->max_boxes_limit = AI_OD_SSD_ST_PP_MAX_BOXES_LIMIT;
  params->conf_threshold = AI_OD_SSD_ST_PP_CONF_THRESHOLD;
  params->iou_threshold = AI_OD_SSD_ST_PP_IOU_THRESHOLD;
  error = od_ssd_st_pp_reset(params);
#elif POSTPROCESS_TYPE == POSTPROCESS_MPE_YOLO_V8_UF
  int32_t error = AI_MPE_PP_ERROR_NO;
  mpe_yolov8_pp_static_param_t *params = (mpe_yolov8_pp_static_param_t *) params_postprocess;
  params->nb_classes = AI_MPE_YOLOV8_PP_NB_CLASSES;
  params->nb_total_boxes = AI_MPE_YOLOV8_PP_TOTAL_BOXES;
  params->max_boxes_limit = AI_MPE_YOLOV8_PP_MAX_BOXES_LIMIT;
  params->conf_threshold = AI_MPE_YOLOV8_PP_CONF_THRESHOLD;
  params->iou_threshold = AI_MPE_YOLOV8_PP_IOU_THRESHOLD;
  params->nb_keypoints = AI_POSE_PP_POSE_KEYPOINTS_NB;
  for (int i = 0; i < AI_MPE_YOLOV8_PP_TOTAL_BOXES; i++) {
    out_detections[i].pKeyPoints = &out_keyPoints[i * AI_POSE_PP_POSE_KEYPOINTS_NB];
  }
  error = mpe_yolov8_pp_reset(params);
#elif POSTPROCESS_TYPE == POSTPROCESS_MPE_PD_UF
  int32_t error;
  pd_model_pp_static_param_t *params = (pd_model_pp_static_param_t *) params_postprocess;
  params->width = AI_PD_MODEL_PP_WIDTH;
  params->height = AI_PD_MODEL_PP_HEIGHT;
  params->nb_keypoints = AI_PD_MODEL_PP_NB_KEYPOINTS;
  params->conf_threshold = AI_PD_MODEL_PP_CONF_THRESHOLD;
  params->iou_threshold = AI_PD_MODEL_PP_IOU_THRESHOLD;
  params->nb_total_boxes = AI_PD_MODEL_PP_TOTAL_DETECTIONS;
  params->max_boxes_limit = AI_PD_MODEL_PP_MAX_BOXES_LIMIT;
  params->pAnchors = g_Anchors;
  for (int i = 0; i < AI_PD_MODEL_PP_MAX_BOXES_LIMIT; i++) {
    out_detections[i].pKps = &out_keyPoints[i][0];
  }
  error = pd_model_pp_reset(params);
#elif POSTPROCESS_TYPE == POSTPROCESS_SPE_MOVENET_UF
  int32_t error = AI_SPE_POSTPROCESS_ERROR_NO;
  spe_movenet_pp_static_param_t *params = (spe_movenet_pp_static_param_t *) params_postprocess;
  params->heatmap_width = AI_SPE_MOVENET_POSTPROC_HEATMAP_WIDTH;
  params->heatmap_height = AI_SPE_MOVENET_POSTPROC_HEATMAP_HEIGHT;
  params->nb_keypoints = AI_POSE_PP_POSE_KEYPOINTS_NB;
  error = spe_movenet_pp_reset(params);
#elif POSTPROCESS_TYPE == POSTPROCESS_ISEG_YOLO_V8_UI
  int32_t error = AI_ISEG_POSTPROCESS_ERROR_NO;
  yolov8_seg_pp_static_param_t *params = (yolov8_seg_pp_static_param_t *) params_postprocess;
  params->nb_classes = AI_YOLOV8_SEG_PP_NB_CLASSES;
  params->nb_total_boxes = AI_YOLOV8_SEG_PP_TOTAL_BOXES;
  params->max_boxes_limit = AI_YOLOV8_SEG_PP_MAX_BOXES_LIMIT;
  params->conf_threshold = AI_YOLOV8_SEG_PP_CONF_THRESHOLD;
  params->iou_threshold = AI_YOLOV8_SEG_PP_IOU_THRESHOLD;
  params->size_masks = AI_YOLOV8_SEG_PP_MASK_SIZE;
  params->raw_output_scale = AI_YOLOV8_SEG_SCALE;
  params->raw_output_zero_point = AI_YOLOV8_SEG_ZERO_POINT;
  params->nb_masks = AI_YOLOV8_SEG_PP_MASK_NB;
  params->mask_raw_output_zero_point = AI_YOLOV8_SEG_MASK_ZERO_POINT;
  params->mask_raw_output_scale = AI_YOLOV8_SEG_MASK_SCALE;
  params->pMask = _out_buf_mask;
  params->pTmpBuff = scratch_detections;
  for (size_t i = 0; i < AI_YOLOV8_SEG_PP_TOTAL_BOXES; i++) {
    scratch_detections[i].pMask = &_out_buf_mask_s8[i * AI_YOLOV8_SEG_PP_MASK_NB];
  }
  for (size_t i = 0; i < AI_YOLOV8_SEG_PP_MAX_BOXES_LIMIT; i++) {
    out_detections[i].pMask = &_iseg_mask[i * AI_YOLOV8_SEG_PP_MASK_SIZE * AI_YOLOV8_SEG_PP_MASK_SIZE];
  }
  error = iseg_yolov8_pp_reset(params);
#elif POSTPROCESS_TYPE == POSTPROCESS_SSEG_DEEPLAB_V3_UF
  int32_t error = AI_SSEG_POSTPROCESS_ERROR_NO;
  sseg_deeplabv3_pp_static_param_t *params = (sseg_deeplabv3_pp_static_param_t *) params_postprocess;
  params->nb_classes = AI_SSEG_DEEPLABV3_PP_NB_CLASSES;
  params->width = AI_SSEG_DEEPLABV3_PP_WIDTH;
  params->height = AI_SSEG_DEEPLABV3_PP_HEIGHT;
  params->type = AI_SSEG_DATA_UINT8;
  error = sseg_deeplabv3_pp_reset(params);
#else
  #error "PostProcessing type not supported"
#endif

  return error;
}

int32_t app_postprocess_run(void *pInput[], int nb_input, void *pOutput, void *pInput_param)
{
#if POSTPROCESS_TYPE == POSTPROCESS_OD_YOLO_V8_UF || POSTPROCESS_TYPE == POSTPROCESS_OD_YOLO_V8_UI
  od_pp_out_t *checked_output = (od_pp_out_t *) pOutput;
  if (checked_output == NULL) return AI_OD_POSTPROCESS_ERROR;
  checked_output->nb_detect = 0;
  checked_output->pOutBuff = out_detections;
  if (nb_input != 1 || pInput == NULL || pInput[0] == NULL ||
      !app_yolov8_params_valid((yolov8_pp_static_param_t *)pInput_param))
    return AI_OD_POSTPROCESS_ERROR;
#if POSTPROCESS_TYPE == POSTPROCESS_OD_YOLO_V8_UF
  if (!app_yolov8_float_input_valid((float32_t *)pInput[0],
                                   (yolov8_pp_static_param_t *)pInput_param))
    return AI_OD_POSTPROCESS_ERROR;
#else
  const yolov8_pp_static_param_t *checked_params = (yolov8_pp_static_param_t *)pInput_param;
  if (!isfinite(checked_params->raw_output_scale) || checked_params->raw_output_scale <= 0.0f)
    return AI_OD_POSTPROCESS_ERROR;
#endif
#endif
#if POSTPROCESS_TYPE == POSTPROCESS_OD_YOLO_V2_UF
  assert(nb_input == 1);
  int32_t error = AI_OD_POSTPROCESS_ERROR_NO;
  yolov2_pp_in_t pp_input = {
    .pRaw_detections = (float32_t *) pInput[0]
  };
  error = od_yolov2_pp_process(&pp_input, (od_pp_out_t *) pOutput,
                               (yolov2_pp_static_param_t *) pInput_param);
#elif POSTPROCESS_TYPE == POSTPROCESS_OD_YOLO_V5_UU
  assert(nb_input == 1);
  int32_t error = AI_OD_POSTPROCESS_ERROR_NO;
  od_pp_out_t *pObjDetOutput = (od_pp_out_t *) pOutput;
  pObjDetOutput->pOutBuff = out_detections;
  yolov5_pp_in_centroid_uint8_t pp_input = {
      .pRaw_detections = (uint8_t *) pInput[0]
  };
  error = od_yolov5_pp_process_uint8(&pp_input, pObjDetOutput,
                                     (yolov5_pp_static_param_t *) pInput_param);
#elif POSTPROCESS_TYPE == POSTPROCESS_OD_YOLO_V8_UF
  assert(nb_input == 1);
  int32_t error = AI_OD_POSTPROCESS_ERROR_NO;
  od_pp_out_t *pObjDetOutput = (od_pp_out_t *) pOutput;
  pObjDetOutput->pOutBuff = out_detections;
  yolov8_pp_in_centroid_t pp_input = {
      .pRaw_detections = (float32_t *) pInput[0]
  };
  error = od_yolov8_pp_process(&pp_input, pObjDetOutput,
                               (yolov8_pp_static_param_t *) pInput_param);
#elif POSTPROCESS_TYPE == POSTPROCESS_OD_YOLO_V8_UI
  assert(nb_input == 1);
  int32_t error = AI_OD_POSTPROCESS_ERROR_NO;
  od_pp_out_t *pObjDetOutput = (od_pp_out_t *) pOutput;
  pObjDetOutput->pOutBuff = out_detections;
  yolov8_pp_in_centroid_int8_t pp_input = {
      .pRaw_detections = (int8_t *) pInput[0]
  };
  error = od_yolov8_pp_process_int8(&pp_input, pObjDetOutput,
                                    (yolov8_pp_static_param_t *) pInput_param);
#elif POSTPROCESS_TYPE == POSTPROCESS_OD_ST_YOLOX_UF
  assert(nb_input == 3);
  int32_t error = AI_OD_POSTPROCESS_ERROR_NO;
  od_pp_out_t *pObjDetOutput = (od_pp_out_t *) pOutput;
  st_yolox_pp_in_t pp_input = {
      .pRaw_detections_S = (float32_t *) pInput[0],
      .pRaw_detections_L = (float32_t *) pInput[1],
      .pRaw_detections_M = (float32_t *) pInput[2],
  };
  error = od_st_yolox_pp_process(&pp_input, pObjDetOutput,
                                 (st_yolox_pp_static_param_t *) pInput_param);
#elif POSTPROCESS_TYPE == POSTPROCESS_OD_ST_SSD_UF
  assert(nb_input == 3);
  int32_t error = AI_OD_POSTPROCESS_ERROR_NO;
  od_pp_out_t *pObjDetOutput = (od_pp_out_t *) pOutput;
  pObjDetOutput->pOutBuff = out_detections;
  float32_t **inputArray = (float32_t **)pInput;
  ssd_st_pp_in_centroid_t pp_input =
  {
      .pAnchors = (float32_t *) inputArray[2],
      .pBoxes = (float32_t *) inputArray[1],
      .pScores = (float32_t *) inputArray[0],
  };
  error = od_ssd_st_pp_process(&pp_input, pObjDetOutput,
                              (ssd_st_pp_static_param_t *) pInput_param);
#elif POSTPROCESS_TYPE == POSTPROCESS_MPE_YOLO_V8_UF
  assert(nb_input == 1);
  int32_t error = AI_MPE_PP_ERROR_NO;
  mpe_pp_out_t *pPoseOutput = (mpe_pp_out_t *) pOutput;
  pPoseOutput->pOutBuff = out_detections;
  mpe_yolov8_pp_in_centroid_t pp_input =
  {
      .pRaw_detections = (float32_t *) pInput[0]
  };
  error = mpe_yolov8_pp_process(&pp_input, pPoseOutput,
                                (mpe_yolov8_pp_static_param_t *) pInput_param);
#elif POSTPROCESS_TYPE == POSTPROCESS_MPE_PD_UF
  assert(nb_input == 2);
  pd_postprocess_out_t *pPdOutput = (pd_postprocess_out_t *) pOutput;
  pd_model_pp_in_t pp_input = {
    .pProbs = (float32_t *) pInput[0],
    .pBoxes = (float32_t *) pInput[1],
  };
  int32_t error;
  pPdOutput->pOutData = out_detections;
  error = pd_model_pp_process(&pp_input, pPdOutput, 
                              (pd_model_pp_static_param_t *) pInput_param);
#elif POSTPROCESS_TYPE == POSTPROCESS_SPE_MOVENET_UF
  assert(nb_input == 1);
  int32_t error = AI_SPE_POSTPROCESS_ERROR_NO;
  spe_pp_out_t *pPoseOutput = (spe_pp_out_t *) pOutput;
  pPoseOutput->pOutBuff = out_detections;
  spe_movenet_pp_in_t pp_input =
  {
      .inBuff = (float32_t *) pInput[0]
  };
  error = spe_movenet_pp_process(&pp_input, pPoseOutput,
                                 (spe_movenet_pp_static_param_t *) pInput_param);
#elif POSTPROCESS_TYPE == POSTPROCESS_ISEG_YOLO_V8_UI
  assert(nb_input == 2);
  int32_t error = AI_ISEG_POSTPROCESS_ERROR_NO;
  iseg_postprocess_out_t *pSegOutput = (iseg_postprocess_out_t *) pOutput;
  pSegOutput->pOutBuff = out_detections;
  yolov8_seg_pp_in_centroid_int8_t pp_input =
  {
      .pRaw_detections = (int8_t *) pInput[0],
      .pRaw_masks = (int8_t *) pInput[1]
  };
  error = iseg_yolov8_pp_process(&pp_input, pOutput,
                                 (yolov8_seg_pp_static_param_t *) pInput_param);
#elif POSTPROCESS_TYPE == POSTPROCESS_SSEG_DEEPLAB_V3_UF
  assert(nb_input == 1);
  int32_t error = AI_SSEG_POSTPROCESS_ERROR_NO;
  sseg_pp_out_t *pSsegOutput = (sseg_pp_out_t *) pOutput;
  pSsegOutput->pOutBuff = out_sseg_map;
  sseg_deeplabv3_pp_in_t pp_input = {
    .pRawData = (float32_t *) pInput[0]
  };
  error = sseg_deeplabv3_pp_process(&pp_input, (sseg_pp_out_t *) pOutput,
                                    (sseg_deeplabv3_pp_static_param_t *) pInput_param);
#else
  #error "PostProcessing type not supported"
#endif

  return error;
}
