#ifndef APP_UI_H
#define APP_UI_H

#include <stdint.h>

#include "app_climate_logic.h"
#include "app_touch.h"
#include "app_ui_port.h"
#include "app_perf_window.h"

typedef enum
{
    APP_AI_STAGE_PENDING = 0,
    APP_AI_STAGE_WAIT_CAMERA = 1,
    APP_AI_STAGE_START_PIPE = 2,
    APP_AI_STAGE_WAIT_INPUT = 3,
    APP_AI_STAGE_WAIT_OUTPUT = 4,
    APP_AI_STAGE_BIND_IO = 5,
    APP_AI_STAGE_NPU_RUN = 6,
    APP_AI_STAGE_READY = 7,
    APP_AI_STAGE_ERROR = 8,
    APP_AI_STAGE_DISABLED = 9
} app_ai_stage_t;

#define APP_AI_ERROR_NO_INPUT_BUFFER (-1100)
#define APP_AI_PP_STATUS_PENDING (-1101)
#define APP_AI_ERROR_MODEL_WEIGHTS (-1210)

typedef struct
{
    uint32_t stage;             /* NN progress; PP never overwrites this. */
    int32_t error;              /* Actual camera/IO status or named local error. */
    uint32_t nn_started;
    uint32_t nn_completed;      /* LL_ATON_RT_Main returned after an inference. */
    uint32_t pp_completed;      /* Completed PP calls, including failed calls. */
    int32_t pp_status;          /* Independent actual PP initialization/run status. */
    uint32_t pipe2_frames;
    uint32_t input_published;
    uint32_t input_dropped;
    uint32_t inference_ms;      /* Elapsed LL_ATON_RT_Main time, including preemption. */
    uint32_t isp_runs;
    uint32_t isp_vsyncs;
    uint32_t isp_last_ms;
    uint32_t isp_max_ms;
    uint32_t recovery_count;
    int32_t last_recovery_error;
    uint32_t completed_rate_x10;
    uint32_t last_completed_ms;
    uint8_t recovery_active;
    uint8_t completed_rate_valid;
} app_ai_diag_t;

#define APP_UI_MAX_DETECTIONS (10U)

typedef struct
{
    uint16_t x;
    uint16_t y;
    uint16_t width;
    uint16_t height;
    float confidence;
    int32_t class_index;
} app_ui_detection_t;

typedef struct
{
    uint32_t frame_id;
    uint32_t captured_ms;
    uint32_t completed_ms;
    uint8_t valid;
    uint8_t count;
    /* Coordinates already match the 800x480 LCD display crop. */
    app_ui_detection_t boxes[APP_UI_MAX_DETECTIONS];
} app_vision_result_t;

typedef struct
{
    uint32_t queued_bytes;
    uint32_t dropped_bytes;
    uint32_t tx_errors;
    uint32_t last_status;
} app_uart_diag_t;

/* Hardware-independent copy; diagnostic status numbers originate in DHT11. */
typedef struct
{
    uint32_t attempts;
    uint32_t successes;
    uint32_t failures;
    uint32_t consecutive_failures;
    uint32_t last_status;
    uint32_t last_attempt_ms;
    uint32_t last_success_ms;
    uint32_t retry_interval_ms;
    uint8_t has_sample;
    uint8_t last_read_ok;
} app_sensor_diag_t;

typedef enum
{
    APP_TEMPERATURE_UNAVAILABLE = 0,
    APP_TEMPERATURE_DHT11,
    APP_TEMPERATURE_SIMULATED
} app_temperature_source_t;

typedef enum
{
    APP_PERF_INPUT_WAIT,
    APP_PERF_OUTPUT_WAIT,
    APP_PERF_NN,
    APP_PERF_PP,
    APP_PERF_ISP,
    APP_PERF_UI,
    APP_PERF_DP,
    APP_PERF_FRAME_LATENCY,
    APP_PERF_TOUCH_GAP,
    APP_PERF_COUNT
} app_perf_metric_t;

typedef struct
{
    app_perf_stats_t stages[APP_PERF_COUNT];
    uint8_t cycle_clock_valid;
} app_perf_diag_t;

typedef struct
{
    int32_t people_count;
    uint8_t people_valid;
    uint32_t people_updated_ms;
    uint16_t als;
    uint8_t als_valid;
    uint32_t als_updated_ms;
    uint32_t nn_period_ms;
    int32_t camera_init_status;
    int32_t camera_start_status;
    app_camera_display_diag_t camera;
    app_lcd_display_diag_t lcd;
    float temperature;
    float humidity;
    uint8_t sensor_valid;
    uint32_t sensor_updated_ms;
    uint8_t humidity_valid;
    uint32_t humidity_updated_ms;
    app_temperature_source_t temperature_source;
    uint8_t simulated;
    uint32_t simulation_speed;
    uint8_t demo_active;
    uint8_t demo_paused;
    uint8_t demo_phase;
    uint32_t demo_elapsed_ms;
    uint8_t relay_mask;
    app_touch_state_t touch;
    app_climate_state_t climate;
    app_ai_diag_t ai;
    app_vision_result_t vision;
    app_uart_diag_t uart;
    app_sensor_diag_t dht;
    app_perf_diag_t perf;
    uint32_t ui_last_ms;
    uint32_t ui_max_ms;
} app_ui_snapshot_t;

static inline app_climate_inputs_t app_ui_snapshot_inputs(const app_ui_snapshot_t *s)
{
    app_climate_inputs_t inputs = {0};
    if (s != 0) {
        inputs.people_count = s->people_count;
        inputs.people_valid = s->people_valid;
        inputs.people_updated_ms = s->people_updated_ms;
        inputs.als = s->als;
        inputs.als_valid = s->als_valid;
        inputs.als_updated_ms = s->als_updated_ms;
        inputs.temperature = s->temperature;
        inputs.humidity = s->humidity;
        inputs.sensor_valid = s->sensor_valid;
        inputs.sensor_updated_ms = s->sensor_updated_ms;
        inputs.humidity_valid = s->humidity_valid;
        inputs.humidity_updated_ms = s->humidity_updated_ms;
    }
    return inputs;
}

typedef void (*app_ui_action_cb_t)(app_climate_action_t action);
/* Optional PC scene control. Board UI keeps live sensor inputs. */
typedef void (*app_ui_demo_cb_t)(uint8_t command);
#define APP_UI_DEMO_TOGGLE (1U)
#define APP_UI_DEMO_STOP (2U)

void app_ui_init(app_ui_action_cb_t action_cb);
void app_ui_set_demo_callback(app_ui_demo_cb_t demo_cb);
void app_ui_set_snapshot(const app_ui_snapshot_t *snapshot);
void app_ui_process(void);

#endif /* APP_UI_H */
