#ifndef APP_MODEL_CONFIG_H
#define APP_MODEL_CONFIG_H

/* The camera, generated-network contract, postprocess and diagnostics share
 * these values. Changing dimensions also requires generating matching weights. */
#define APP_MODEL_NAME "YOLOv8n"
#define APP_MODEL_DESCRIPTION "YOLOv8n 256 INT8 person"
#define APP_MODEL_INPUT_WIDTH (256U)
#define APP_MODEL_INPUT_HEIGHT (256U)
#define APP_MODEL_INPUT_CHANNELS (3U)
#define APP_MODEL_INPUT_BYTES (APP_MODEL_INPUT_WIDTH * APP_MODEL_INPUT_HEIGHT * APP_MODEL_INPUT_CHANNELS)
#define APP_MODEL_CLASSES (1U)
#define APP_MODEL_OUTPUT_CHANNELS (4U + APP_MODEL_CLASSES)
#define APP_MODEL_OUTPUT_BOXES ((APP_MODEL_INPUT_WIDTH / 8U) * (APP_MODEL_INPUT_HEIGHT / 8U) + \
                               (APP_MODEL_INPUT_WIDTH / 16U) * (APP_MODEL_INPUT_HEIGHT / 16U) + \
                               (APP_MODEL_INPUT_WIDTH / 32U) * (APP_MODEL_INPUT_HEIGHT / 32U))
#define APP_MODEL_OUTPUT_BYTES (APP_MODEL_OUTPUT_CHANNELS * APP_MODEL_OUTPUT_BOXES * 4U)
#define APP_MODEL_MAX_DETECTIONS (10U)

/* Candidate defaults, pending calibration on the user's real indoor scenes. */
#define APP_MODEL_CONF_THRESHOLD (0.5f)
#define APP_MODEL_IOU_THRESHOLD (0.45f)
#define APP_MODEL_INPUT_QUANT_SCALE (1.0f / 255.0f)
#define APP_MODEL_INPUT_ZERO_POINT (0)

#endif
