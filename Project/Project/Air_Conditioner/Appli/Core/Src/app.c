/**
 ****************************************************************************************************
 * @file        app.c
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2025-01-13
 * @brief       app.c文件
 * @license     Copyright (c) 2020-2032, 广州市星翼电子科技有限公司
 ****************************************************************************************************
 * @attention
 * 
 * 实验平台:正点原子 N647开发板
 * 在线视频:www.yuanzige.com
 * 技术论坛:www.openedv.com
 * 公司网址:www.alientek.com
 * 购买地址:openedv.taobao.com
 * 
 ****************************************************************************************************
 */

#include "app.h"
#include "app_config.h"
#include "app_utils.h"
#include "app_lcd.h"
#include "app_camera.h"
#include "app_bqueue.h"
#include "app_postprocess.h"
#include "app_model_contract.h"
#include "app_model_weights.h"
#include "network.h"
#include "app_input_logic.h"
#include "app_climate_logic.h"
#include "dht11.h"
#include "key.h"
#include "ap3216c.h"
#include "remote.h"
#include "app_touch.h"
#include "app_ui.h"
#include "app_ui_pixels.h"
#include "tx_api.h"
#include "cmw_camera.h"
#include "bsp_bus.h"
#include "uart.h"
#include "app_frame_rate.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "ll_aton_runtime.h"

typedef struct {
    TX_MUTEX lock;
    app_ui_snapshot_t info;
    uint8_t ai_failed;
} app_display_t;

static TX_SEMAPHORE isp_semaphore;
static volatile uint32_t s_ai_pipe2_frames;
static volatile uint32_t s_ai_input_published;
static volatile uint32_t s_ai_input_dropped;
static volatile uint32_t s_ai_isp_vsyncs;
static TX_MUTEX s_perf_lock;
static app_perf_window_t s_perf_windows[APP_PERF_COUNT];
static uint32_t s_perf_cycles_per_us;
static uint8_t s_perf_cycle_clock_valid;

typedef struct { uint32_t tick_ms; uint32_t cycles; } app_perf_stamp_t;

static void app_perf_clock_init(void)
{
    /* Leave CYCCNT intact: ThreadX execution profiling may also read it. */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    __DSB(); __ISB();
    uint32_t before = DWT->CYCCNT;
    for (volatile uint32_t i = 0U; i < 16U; i++) { __NOP(); }
    uint32_t after = DWT->CYCCNT;
    uint32_t cpu_hz = HAL_RCC_GetCpuClockFreq();
    s_perf_cycles_per_us = cpu_hz / 1000000U;
    s_perf_cycle_clock_valid = after != before && s_perf_cycles_per_us != 0U;
}

static app_perf_stamp_t app_perf_start(void)
{
    app_perf_stamp_t stamp;
    stamp.tick_ms = HAL_GetTick();
    stamp.cycles = DWT->CYCCNT;
    return stamp;
}

static uint32_t app_perf_elapsed_us(app_perf_stamp_t stamp)
{
    uint32_t cycles = DWT->CYCCNT;
    uint32_t elapsed_ms = HAL_GetTick() - stamp.tick_ms;
    /* At the configured 800 MHz the cycle counter wraps after 5.37 s.
       Short unsigned differences tolerate one wrap. Long/stopped readings
       fall back to the independent millisecond clock. */
    return app_perf_duration_us(cycles - stamp.cycles, elapsed_ms,
        s_perf_cycles_per_us, s_perf_cycle_clock_valid);
}

static void app_perf_record(app_perf_metric_t metric, uint32_t elapsed_us)
{
    tx_mutex_get(&s_perf_lock, TX_WAIT_FOREVER);
    app_perf_window_record(&s_perf_windows[metric], elapsed_us);
    tx_mutex_put(&s_perf_lock);
}

static void app_perf_snapshot(app_perf_diag_t *diag)
{
    app_perf_window_t copy;
    for (unsigned int i = 0U; i < APP_PERF_COUNT; i++) {
        tx_mutex_get(&s_perf_lock, TX_WAIT_FOREVER);
        copy = s_perf_windows[i];
        tx_mutex_put(&s_perf_lock);
        /* Sorting never holds the shared lock or blocks the input thread. */
        app_perf_window_snapshot(&copy, &diag->stages[i]);
    }
    diag->cycle_clock_valid = s_perf_cycle_clock_valid;
}

static void app_camera_display_pipe_vsync_cb(void);
static void app_camera_display_pipe_frame_cb(void);
static void app_camera_nn_pipe_frame_cb(void);

static TX_THREAD nn_thread;
static UCHAR nn_thread_stack[4096];
static TX_THREAD pp_thread;
static UCHAR pp_thread_stack[4096];
static TX_THREAD dp_thread;
static UCHAR dp_thread_stack[4096];
static TX_THREAD isp_thread;
static UCHAR isp_thread_stack[4096];
static TX_THREAD input_thread;
static UCHAR input_thread_stack[2048];
static TX_THREAD camera_thread;
static UCHAR camera_thread_stack[4096];
static TX_THREAD sensor_thread;
static UCHAR sensor_thread_stack[2048];
static TX_THREAD logger_thread;
static UCHAR logger_thread_stack[2048];
static VOID sensor_thread_entry(ULONG id);
static VOID logger_thread_entry(ULONG id);
static volatile uint8_t s_vision_running;
static volatile uint8_t s_nn_capture_enabled;
static volatile uint8_t s_display_capture_enabled;
static volatile uint8_t s_restart_requested;
static volatile uint8_t s_nn_paused = 1U;
static volatile uint8_t s_pp_paused = 1U;
static volatile uint8_t s_isp_paused = 1U;
static volatile int32_t s_restart_reason;
static uint8_t s_nn_capture_index;
typedef struct { uint32_t frame_id; uint32_t captured_ms; } app_frame_meta_t;
static app_frame_meta_t s_input_meta[2];
static app_frame_meta_t s_output_meta[2];
static volatile uint32_t s_input_poll_last_ms;
static volatile uint32_t s_input_poll_max_ms;
static volatile uint32_t s_dp_cycle_max_ms;
#define APP_AI_ERROR_INPUT_TIMEOUT (-1110)
#define APP_AI_ERROR_OUTPUT_TIMEOUT (-1111)
#define APP_AI_ERROR_QUIESCE_TIMEOUT (-1112)
#define APP_AI_ERROR_CAMERA_STALLED (-1113)
#define APP_AI_ERROR_PP_FAILED (-1114)
#define APP_AI_ERROR_DISPLAY_BUFFERS (-1115)
#define APP_AI_RETRY_LIMIT (3U)
#define APP_AI_WAIT_TICKS (100U)
#define APP_AI_STAGE_TIMEOUT_MS (1500U)
#define APP_AI_RECOVERY_SETTLE_MS (5000U)

static VOID nn_thread_entry(ULONG id);
static VOID pp_thread_entry(ULONG id);
static VOID dp_thread_entry(ULONG id);
static VOID isp_thread_entry(ULONG id);
static VOID input_thread_entry(ULONG id);
static VOID camera_thread_entry(ULONG id);

static app_display_t display;
static TX_EVENT_FLAGS_GROUP camera_startup;
#define APP_CAMERA_STARTUP_DONE (1UL)
#define APP_CAMERA_STARTUP_WAIT_TICKS (5UL * TX_TIMER_TICKS_PER_SECOND)
#define APP_THREAD_INPUT_PRIORITY (1U)
#define APP_THREAD_DP_PRIORITY (2U)
#define APP_THREAD_ISP_PRIORITY (TX_MAX_PRIORITIES - 4U)
#define APP_THREAD_NN_PRIORITY (TX_MAX_PRIORITIES - 3U)
#define APP_THREAD_PP_PRIORITY (TX_MAX_PRIORITIES - 2U)
#define APP_THREAD_CAMERA_PRIORITY (TX_MAX_PRIORITIES - 1U)
#define APP_DIAG_LOG_PERIOD_MS (1000U)

LL_ATON_DECLARE_NAMED_NN_INSTANCE_AND_INTERFACE(network);
static uint8_t nn_input_buffers[2][NN_WIDTH * NN_HEIGHT * NN_BPP] __attribute__((aligned(32))) __attribute__((section(".EXTRAM")));
static app_bqueue_t nn_input_queue;
static uint8_t nn_output_buffers[2][ALIGN_VALUE(NN_BUFFER_OUT_SIZE, 32)] __attribute__((aligned(32)));
static app_bqueue_t nn_output_queue;
_Static_assert(LL_ATON_NETWORK_IN_NUM == 1 && LL_ATON_NETWORK_OUT_NUM == 1 &&
    LL_ATON_NETWORK_USER_ALLOCATED_INPUTS == 1 && LL_ATON_NETWORK_USER_ALLOCATED_OUTPUTS == 1,
    "The camera pipeline requires one user-owned input and output");
_Static_assert(sizeof(nn_input_buffers[0]) == LL_ATON_NETWORK_IN_1_SIZE_BYTES,
    "The camera input and generated model must agree");
_Static_assert(NN_BUFFER_OUT_SIZE == LL_ATON_NETWORK_OUT_1_SIZE_BYTES &&
    NN_BUFFER_OUT_SIZE == AI_OBJDETECT_YOLOV8_PP_TOTAL_BOXES *
        (4 + AI_OBJDETECT_YOLOV8_PP_NB_CLASSES) * sizeof(float),
    "The generated output and YOLOv8 postprocess must agree");
_Static_assert(APP_UI_MAX_DETECTIONS == AI_OBJDETECT_YOLOV8_PP_MAX_BOXES_LIMIT,
    "The bounded vision snapshot must fit all configured detections");
_Static_assert(APP_MODEL_WEIGHTS_BYTES > 0U && APP_MODEL_WEIGHTS_BYTES <= 11U * 1024U * 1024U,
    "The model weights must fit their external Flash partition");

static int32_t app_model_check_generated_io(void)
{
    const LL_Buffer_InfoTypeDef *input = LL_ATON_Input_Buffers_Info_network();
    const LL_Buffer_InfoTypeDef *output = LL_ATON_Output_Buffers_Info_network();
    app_model_io_t in = {0};
    app_model_io_t out = {0};
    if (input == NULL || output == NULL || input->name == NULL || output->name == NULL ||
        input->mem_shape == NULL || output->mem_shape == NULL ||
        input->mem_ndims > 4U || output->mem_ndims > 4U ||
        !input->is_user_allocated || !output->is_user_allocated || input->per_channel ||
        input->scale == NULL || input->offset == NULL) {
        return APP_MODEL_IO_NULL;
    }
    in.dtype = input->type == DataType_UINT8 && input->nbits == 8U ? APP_MODEL_DTYPE_UINT8 : 0;
    out.dtype = output->type == DataType_FLOAT && output->nbits == 32U ? APP_MODEL_DTYPE_FLOAT32 : 0;
    in.rank = (uint8_t)input->mem_ndims;
    out.rank = (uint8_t)output->mem_ndims;
    memcpy(in.shape, input->mem_shape, in.rank * sizeof(uint32_t));
    memcpy(out.shape, output->mem_shape, out.rank * sizeof(uint32_t));
    in.bytes = LL_Buffer_len(input);
    out.bytes = LL_Buffer_len(output);
    in.quant_scale = input->scale[0];
    in.zero_point = input->offset[0];
    int32_t status = app_model_validate_io(&in, &out,
        sizeof(nn_input_buffers[0]), sizeof(nn_output_buffers[0]));
    if (status != APP_MODEL_IO_OK) { return status; }
    /* A new application with old flash weights must fail before any NPU run.
       This one-time check runs in the lowest-priority camera worker; UI/input
       remain scheduled while the matching payload is read. */
    uint32_t crc = app_model_crc32((const void *)(uintptr_t)APP_MODEL_WEIGHTS_ADDRESS,
                                   APP_MODEL_WEIGHTS_BYTES);
    return crc == APP_MODEL_WEIGHTS_CRC32 ? APP_MODEL_IO_OK : APP_AI_ERROR_MODEL_WEIGHTS;
}

static void app_input_update_sensor(float temperature,
                                    float humidity,
                                    uint8_t valid,
                                    const dht11_diag_t *diag);
static void app_input_update_touch(void);
static void app_input_apply_action(app_climate_action_t action);
static void app_climate_refresh_locked(uint32_t now);
static void app_ai_set_stage(uint32_t stage, int32_t error);
static void app_ai_snapshot_counters(app_ai_diag_t *diag);
static void app_vision_request_restart(int32_t error);
static uint8_t app_vision_wait_paused(void);

void app_run(void)
{
    app_climate_init(&display.info.climate, APP_DEFAULT_TARGET_TEMP_C);
    display.info.sensor_valid = 0U;
    display.info.humidity_valid = 0U;
    display.info.temperature_source = APP_TEMPERATURE_DHT11;
    display.info.dht.retry_interval_ms = APP_DHT11_PERIOD_MS;
    display.info.camera_init_status = APP_CAMERA_STATUS_PENDING;
    display.info.camera_start_status = APP_CAMERA_STATUS_PENDING;
    display.info.ai.stage = APP_AI_STAGE_PENDING;
    display.info.ai.pp_status = APP_AI_PP_STATUS_PENDING;
    app_lcd_init();
#if (APP_CAMERA_LCD_TEST_PATTERN != 0U)
    if (app_lcd_show_camera_test_pattern() != HAL_OK)
    {
        Error_Handler();
    }
#endif
    /* Render the UI before optional camera startup so a camera failure
       cannot leave the display without a diagnostic screen. */
    app_ui_init(app_input_apply_action);
    app_ui_process();
    app_bqueue_init(&nn_input_queue, 2, (uint8_t *[2]){nn_input_buffers[0], nn_input_buffers[1]});
    app_bqueue_init(&nn_output_queue, 2, (uint8_t *[2]){nn_output_buffers[0], nn_output_buffers[1]});
    display.info.touch.ready = 0U;
    display.info.touch.pressed = 0U;
    display.info.touch.x = 0U;
    display.info.touch.y = 0U;
    if (tx_semaphore_create(&isp_semaphore, NULL, 0) != TX_SUCCESS)
    {
        Error_Handler();
    }

    if (tx_mutex_create(&display.lock, NULL, TX_INHERIT) != TX_SUCCESS)
    {
        Error_Handler();
    }
    if (tx_mutex_create(&s_perf_lock, "Perf samples", TX_INHERIT) != TX_SUCCESS)
    { Error_Handler(); }
    app_perf_clock_init();

    if (tx_event_flags_create(&camera_startup, "Camera startup") != TX_SUCCESS)
    {
        Error_Handler();
    }

    key_init();
    uart_tx_start();
    if (tx_thread_create(&sensor_thread, "Sensor Thread", sensor_thread_entry,
                         0, sensor_thread_stack, sizeof(sensor_thread_stack),
                         15U, 15U, 10U, TX_AUTO_START) != TX_SUCCESS) { Error_Handler(); }
    if (tx_thread_create(&logger_thread, "Logger Thread", logger_thread_entry,
                         0, logger_thread_stack, sizeof(logger_thread_stack),
                         TX_MAX_PRIORITIES - 1U, TX_MAX_PRIORITIES - 1U,
                         10U, TX_AUTO_START) != TX_SUCCESS) { Error_Handler(); }
    /* Initialize the shared hardware bus once, before camera/touch threads. */
    (void)bsp_i2c2_init();

    /* Start the display and input paths before any optional vision work. */
    if (tx_thread_create(&dp_thread, "DP Thread", dp_thread_entry, 0, dp_thread_stack, sizeof(dp_thread_stack), APP_THREAD_DP_PRIORITY, APP_THREAD_DP_PRIORITY, 10, TX_AUTO_START) != TX_SUCCESS)
    {
        Error_Handler();
    }

    if (tx_thread_create(&input_thread, "Input Thread", input_thread_entry, 0, input_thread_stack, sizeof(input_thread_stack), APP_THREAD_INPUT_PRIORITY, APP_THREAD_INPUT_PRIORITY, 10, TX_AUTO_START) != TX_SUCCESS)
    {
        Error_Handler();
    }

#if (APP_CAMERA_PIPELINE_ENABLED != 0U)
    if (tx_thread_create(&nn_thread, "NN Thread", nn_thread_entry, 0, nn_thread_stack, sizeof(nn_thread_stack), APP_THREAD_NN_PRIORITY, APP_THREAD_NN_PRIORITY, 10, TX_AUTO_START) != TX_SUCCESS)
    {
        Error_Handler();
    }

    if (tx_thread_create(&pp_thread, "PP Thread", pp_thread_entry, 0, pp_thread_stack, sizeof(pp_thread_stack), APP_THREAD_PP_PRIORITY, APP_THREAD_PP_PRIORITY, 10, TX_AUTO_START) != TX_SUCCESS)
    {
        Error_Handler();
    }

    if (tx_thread_create(&isp_thread, "ISP Thread", isp_thread_entry, 0, isp_thread_stack, sizeof(isp_thread_stack), APP_THREAD_ISP_PRIORITY, APP_THREAD_ISP_PRIORITY, 10, TX_AUTO_START) != TX_SUCCESS)
    {
        Error_Handler();
    }

    if (tx_thread_create(&camera_thread, "Camera Thread", camera_thread_entry,
                         0, camera_thread_stack, sizeof(camera_thread_stack),
                         APP_THREAD_CAMERA_PRIORITY, APP_THREAD_CAMERA_PRIORITY, 10,
                         TX_AUTO_START) != TX_SUCCESS)
    {
        tx_mutex_get(&display.lock, TX_WAIT_FOREVER);
        display.info.camera_init_status = APP_CAMERA_STATUS_THREAD_ERROR;
        display.info.camera_start_status = APP_CAMERA_STATUS_SKIPPED;
        display.info.ai.stage = APP_AI_STAGE_ERROR;
        display.info.ai.error = APP_CAMERA_STATUS_THREAD_ERROR;
        display.ai_failed = 1U;
        tx_mutex_put(&display.lock);
        tx_event_flags_set(&camera_startup, APP_CAMERA_STARTUP_DONE, TX_OR);
        /* Vision is optional; keep the display and local controls alive. */
    }
#else
    tx_mutex_get(&display.lock, TX_WAIT_FOREVER);
    display.info.camera_init_status = APP_CAMERA_STATUS_SKIPPED;
    display.info.camera_start_status = APP_CAMERA_STATUS_SKIPPED;
    display.info.ai.stage = APP_AI_STAGE_DISABLED;
    tx_mutex_put(&display.lock);
    tx_event_flags_set(&camera_startup, APP_CAMERA_STARTUP_DONE, TX_OR);
#endif

}

static void app_input_update_sensor(float temperature,
                                    float humidity,
                                    uint8_t valid,
                                    const dht11_diag_t *diag)
{
    tx_mutex_get(&display.lock, TX_WAIT_FOREVER);
    display.info.sensor_valid = valid;
    display.info.humidity_valid = valid;
    if (valid != 0U)
    {
        display.info.temperature = temperature;
        display.info.humidity = humidity;
        display.info.sensor_updated_ms = diag->last_success_ms;
        display.info.humidity_updated_ms = display.info.sensor_updated_ms;
    }
    /* Preserve the last successful values for an explicitly labelled UI
       history, while any failed read still invalidates control requests. */
    display.info.dht.attempts = diag->attempts;
    display.info.dht.successes = diag->successes;
    display.info.dht.failures = diag->failures;
    display.info.dht.consecutive_failures = diag->consecutive_failures;
    display.info.dht.last_status = (uint32_t)diag->last_status;
    display.info.dht.last_attempt_ms = diag->last_attempt_ms;
    display.info.dht.last_success_ms = diag->last_success_ms;
    display.info.dht.retry_interval_ms = APP_DHT11_PERIOD_MS;
    display.info.dht.has_sample = diag->has_sample;
    display.info.dht.last_read_ok = valid;
    app_climate_refresh_locked(HAL_GetTick());
    tx_mutex_put(&display.lock);
}

static void app_input_apply_action(app_climate_action_t action)
{
    tx_mutex_get(&display.lock, TX_WAIT_FOREVER);
    app_climate_apply_action(&display.info.climate, action);
    app_climate_refresh_locked(HAL_GetTick());
    tx_mutex_put(&display.lock);
}

static void app_input_update_touch(void)
{
    tx_mutex_get(&display.lock, TX_WAIT_FOREVER);
    app_touch_get_state(&display.info.touch);
    tx_mutex_put(&display.lock);
}

/* Caller holds display.lock; hardware reads are never performed here. */
static void app_climate_refresh_locked(uint32_t now)
{
    app_climate_inputs_t inputs = app_ui_snapshot_inputs(&display.info);
    app_climate_step(&display.info.climate, &inputs, now);
    display.info.relay_mask = app_climate_output_mask(&display.info.climate);
}

static VOID input_thread_entry(ULONG id)
{
    uint32_t last_climate_tick;
    uint32_t last_poll = HAL_GetTick();
    uint8_t key_code;
    uint8_t remote_key;
    app_climate_reason_t last_reason = (app_climate_reason_t)-1;
    uint8_t last_output = 0xFFU;
    ULONG startup_flags;
    char line[160];
    UNUSED(id);
    remote_init();
    /* Initial camera programming shares I2C2; later retries use its mutex. */
    if (tx_event_flags_get(&camera_startup, APP_CAMERA_STARTUP_DONE, TX_OR,
                          &startup_flags, APP_CAMERA_STARTUP_WAIT_TICKS) != TX_SUCCESS)
    {
        tx_mutex_get(&display.lock, TX_WAIT_FOREVER);
        if (display.info.camera_init_status == APP_CAMERA_STATUS_PENDING)
        { display.info.camera_init_status = APP_CAMERA_STATUS_TIMEOUT; }
        tx_mutex_put(&display.lock);
        const char *late = "CAMERA,error,startup timed out; input continues\r\n";
        (void)uart_tx_enqueue_line(late, (uint16_t)strlen(late));
    }
    (void)app_touch_init();
    app_input_update_touch();
    last_climate_tick = HAL_GetTick();
    last_poll = last_climate_tick;
    while (1)
    {
        uint32_t now = HAL_GetTick();
        s_input_poll_last_ms = now - last_poll;
        app_perf_record(APP_PERF_TOUCH_GAP, app_perf_ms_to_us(s_input_poll_last_ms));
        if (s_input_poll_last_ms > s_input_poll_max_ms)
        { s_input_poll_max_ms = s_input_poll_last_ms; }
        last_poll = now;
        key_code = key_scan(0U);
        if (key_code != NONE_PRES)
        { app_input_apply_action(app_input_key_to_action(key_code)); }
        remote_key = remote_scan();
        if (remote_key != 0U)
        { app_input_apply_action(app_climate_remote_action(remote_key)); }
        app_touch_poll();
        app_input_update_touch();
        now = HAL_GetTick();
        if ((uint32_t)(now - last_climate_tick) >= APP_INPUT_CLIMATE_PERIOD_MS)
        {
            app_climate_reason_t reason;
            uint8_t output;
            tx_mutex_get(&display.lock, TX_WAIT_FOREVER);
            app_climate_refresh_locked(now);
            reason = display.info.climate.reason;
            output = display.info.relay_mask;
            tx_mutex_put(&display.lock);
            if (reason != last_reason || output != last_output)
            {
                int length = snprintf(line, sizeof(line),
                    "CLIMATE,%lu,%s,request=0x%02X\r\n", (unsigned long)now,
                    app_climate_reason_name(reason), (unsigned int)output);
                if (length > 0 && (size_t)length < sizeof(line))
                { (void)uart_tx_enqueue_line(line, (uint16_t)length); }
                last_reason = reason; last_output = output;
            }
            last_climate_tick = now;
        }
        tx_thread_sleep(APP_INPUT_POLL_TICKS);
    }
}

static VOID sensor_thread_entry(ULONG id)
{
    float temperature = 0.0f, humidity = 0.0f;
    uint8_t ap_ready = 0U, ap_errors = 0U;
    uint32_t last_light = 0U, last_ap_retry = 0U, last_dht = 0U;
    dht11_diag_t dht_diag;
    UNUSED(id);
    tx_thread_sleep(200U);
    ap_ready = ap3216c_init() == 0U;
    last_ap_retry = HAL_GetTick();
    /* The first DHT11 request waits a two-second, millisecond-based interval;
       AP3216C remains available during this independent startup interval. */
    last_dht = HAL_GetTick();
    while (1)
    {
        uint32_t now = HAL_GetTick();
        if (!ap_ready && (uint32_t)(now - last_ap_retry) >= 5000U)
        {
            last_ap_retry = now;
            ap_ready = ap3216c_init() == 0U;
            ap_errors = 0U;
        }
        if ((uint32_t)(now - last_light) >= APP_INPUT_CLIMATE_PERIOD_MS)
        {
            uint16_t ir = 0U, ps = 0U, als = 0U;
            uint8_t valid = ap_ready && ap3216c_read_checked(&ir, &ps, &als) == 0U;
            if (valid) { ap_errors = 0U; }
            else if (ap_ready && ++ap_errors >= 3U)
            { ap_ready = 0U; last_ap_retry = HAL_GetTick(); }
            tx_mutex_get(&display.lock, TX_WAIT_FOREVER);
            display.info.als_valid = valid;
            if (valid)
            { display.info.als = als; display.info.als_updated_ms = HAL_GetTick(); }
            tx_mutex_put(&display.lock);
            last_light = now;
        }
        if ((uint32_t)(now - last_dht) >= APP_DHT11_PERIOD_MS)
        {
            uint8_t valid = dht11_get_data(&temperature, &humidity) == 0U;
            dht11_get_diag(&dht_diag);
            app_input_update_sensor(temperature, humidity, valid, &dht_diag);
            /* Keep a bounded two-second retry after both success and
               failure, measured from completion; never burst to catch up. */
            last_dht = HAL_GetTick();
        }
        tx_thread_sleep(20U);
    }
}

static VOID logger_thread_entry(ULONG id)
{
    UNUSED(id);
    while (1) { uart_tx_process(); tx_thread_sleep(1U); }
}

static void app_camera_display_pipe_vsync_cb(void)
{
    s_ai_isp_vsyncs++;
    /* ISP needs the latest statistics, and no work during reconfiguration. */
    if (s_vision_running) { (void)tx_semaphore_ceiling_put(&isp_semaphore, 1U); }
}

static void app_vision_request_restart(int32_t error)
{
    uint32_t irq_state = __get_PRIMASK();
    __disable_irq();
    if (s_restart_requested == 0U) { s_restart_reason = error; }
    s_restart_requested = 1U;
    s_vision_running = 0U;
    s_nn_capture_enabled = 0U;
    __set_PRIMASK(irq_state);
    app_ai_set_stage(APP_AI_STAGE_ERROR, error);
}

static uint8_t app_vision_wait_paused(void)
{
    uint32_t started = HAL_GetTick();
    while (!s_nn_paused || !s_pp_paused || !s_isp_paused)
    {
        if ((uint32_t)(HAL_GetTick() - started) >= 2000U) { return 0U; }
        tx_thread_sleep(10U);
    }
    return 1U;
}

static VOID camera_thread_entry(ULONG id)
{
    uint8_t retries = 0U;
    uint8_t initial_attempt = 1U;
    uint32_t healthy_since = 0U;
    uint32_t last_display_count = 0U;
    uint32_t last_display_progress = HAL_GetTick();
    UNUSED(id);
    int32_t model_status = app_model_check_generated_io();
    if (model_status != APP_MODEL_IO_OK) {
        app_ai_set_stage(APP_AI_STAGE_ERROR, model_status);
        tx_mutex_get(&display.lock, TX_WAIT_FOREVER);
        display.info.camera_init_status = APP_CAMERA_STATUS_SKIPPED;
        display.info.camera_start_status = APP_CAMERA_STATUS_SKIPPED;
        display.info.ai.last_recovery_error = model_status;
        tx_mutex_put(&display.lock);
        (void)tx_event_flags_set(&camera_startup, APP_CAMERA_STARTUP_DONE, TX_OR);
        return;
    }
    while (1)
    {
        int32_t init_status;
        int32_t start_status = APP_CAMERA_STATUS_SKIPPED;
        int32_t nn_status;
        uint8_t *destination;
        s_vision_running = 0U;
        s_nn_capture_enabled = 0U;
        s_display_capture_enabled = 0U;
        if (!app_vision_wait_paused())
        {
            /* A running NPU cannot have its buffers reclaimed or be reset here. */
            app_ai_set_stage(APP_AI_STAGE_ERROR, APP_AI_ERROR_QUIESCE_TIMEOUT);
            tx_mutex_get(&display.lock, TX_WAIT_FOREVER);
            display.info.ai.recovery_active = 0U;
            display.info.ai.last_recovery_error = APP_AI_ERROR_QUIESCE_TIMEOUT;
            tx_mutex_put(&display.lock);
            while (1) { tx_thread_sleep(1000U); }
        }
        if (!initial_attempt)
        {
            int32_t stop_status = app_camera_deinit();
            if (stop_status != CMW_ERROR_NONE)
            {
                app_ai_set_stage(APP_AI_STAGE_ERROR, stop_status);
                break;
            }
            app_lcd_stop_bg_capture();
            retries++;
            tx_mutex_get(&display.lock, TX_WAIT_FOREVER);
            display.info.ai.recovery_count++;
            display.info.ai.recovery_active = 1U;
            tx_mutex_put(&display.lock);
            tx_thread_sleep(500U);
        }
        app_bqueue_reset(&nn_input_queue);
        app_bqueue_reset(&nn_output_queue);
        while (tx_semaphore_get(&isp_semaphore, TX_NO_WAIT) == TX_SUCCESS) {}
        memset(s_input_meta, 0, sizeof(s_input_meta));
        memset(s_output_meta, 0, sizeof(s_output_meta));
        s_restart_requested = 0U;
        init_status = app_camera_init(app_camera_display_pipe_vsync_cb,
            app_camera_display_pipe_frame_cb, NULL, app_camera_nn_pipe_frame_cb);
        if (init_status == CMW_ERROR_NONE)
        {
            if (app_lcd_restart_bg_capture()) {
                s_display_capture_enabled = 1U;
                start_status = app_camera_display_pipe_start(app_lcd_get_bg_buffer(),
                                                              CMW_MODE_CONTINUOUS);
            } else { start_status = APP_AI_ERROR_DISPLAY_BUFFERS; }
        }
        tx_mutex_get(&display.lock, TX_WAIT_FOREVER);
        display.info.camera_init_status = init_status;
        display.info.camera_start_status = start_status;
        display.info.people_valid = 0U;
        display.info.vision.valid = 0U;
        tx_mutex_put(&display.lock);
        /* Latched for input startup; both success and failure release it. */
        tx_event_flags_set(&camera_startup, APP_CAMERA_STARTUP_DONE, TX_OR);
        if (init_status == CMW_ERROR_NONE && start_status == CMW_ERROR_NONE)
        {
            app_camera_set_sensor_test_pattern();
            destination = app_bqueue_get_free(&nn_input_queue, 0U);
            s_nn_capture_index = 0U;
            s_nn_capture_enabled = 1U;
            nn_status = destination ? app_camera_nn_pipe_start(destination,
                CMW_MODE_CONTINUOUS) : APP_AI_ERROR_NO_INPUT_BUFFER;
            if (nn_status == CMW_ERROR_NONE)
            {
                tx_mutex_get(&display.lock, TX_WAIT_FOREVER);
                display.ai_failed = 0U;
                display.info.ai.recovery_active = 0U;
                display.info.ai.error = 0;
                display.info.ai.stage = APP_AI_STAGE_WAIT_INPUT;
                tx_mutex_put(&display.lock);
                __DMB();
                s_vision_running = 1U;
                healthy_since = HAL_GetTick();
                last_display_progress = healthy_since;
                last_display_count = 0U;
            }
            else { app_vision_request_restart(nn_status); }
        }
        else { app_vision_request_restart(init_status != CMW_ERROR_NONE ? init_status : start_status); }
        initial_attempt = 0U;
        while (s_vision_running && !s_restart_requested)
        {
            uint32_t now = HAL_GetTick();
            /* Only DP owns the FPS estimator; supervision reads the ISR count. */
            uint32_t frame_count = app_camera_get_display_frame_count();
            if (frame_count != last_display_count)
            { last_display_count = frame_count; last_display_progress = now; }
            if ((uint32_t)(now - last_display_progress) >= APP_AI_STAGE_TIMEOUT_MS)
            { app_vision_request_restart(APP_AI_ERROR_CAMERA_STALLED); }
            if ((uint32_t)(now - healthy_since) >= APP_AI_RECOVERY_SETTLE_MS)
            { retries = 0U; }
            tx_thread_sleep(100U);
        }
        tx_mutex_get(&display.lock, TX_WAIT_FOREVER);
        display.info.ai.last_recovery_error = s_restart_reason;
        display.info.ai.recovery_active = 1U;
        display.info.people_valid = 0U;
        display.info.vision.valid = 0U;
        tx_mutex_put(&display.lock);
        if (retries >= APP_AI_RETRY_LIMIT) { break; }
    }
    s_vision_running = 0U;
    s_nn_capture_enabled = 0U;
    s_display_capture_enabled = 0U;
    /* Finish stopping DMA if workers returned; never reset an active NPU. */
    if (app_vision_wait_paused() && app_camera_deinit() == CMW_ERROR_NONE) {
        app_lcd_stop_bg_capture();
    }
    tx_mutex_get(&display.lock, TX_WAIT_FOREVER);
    display.info.ai.recovery_active = 0U;
    tx_mutex_put(&display.lock);
}

static void app_camera_display_pipe_frame_cb(void)
{
#if (APP_CAMERA_LCD_TEST_PATTERN == 0U)
    if (!s_display_capture_enabled) { return; }
    if (app_lcd_switch_bg_buffer()) {
        int32_t status = app_camera_display_pipe_set_address(app_lcd_get_bg_buffer());
        app_lcd_bg_capture_target_result(status == HAL_OK);
    }
    /* Publish only after DMA was successfully redirected to another slot. */
    app_lcd_present_bg_buffer();
#endif
}

static void app_camera_nn_pipe_frame_cb(void)
{
    uint8_t *buffer;
    uint8_t completed_index;
    s_ai_pipe2_frames++;
    if (!s_nn_capture_enabled) { return; }
    buffer = app_bqueue_get_free(&nn_input_queue, 0U);
    if (buffer != NULL)
    {
        completed_index = s_nn_capture_index;
        s_input_meta[completed_index].frame_id = s_ai_pipe2_frames;
        s_input_meta[completed_index].captured_ms = HAL_GetTick();
        s_nn_capture_index = (buffer == nn_input_buffers[0]) ? 0U : 1U;
        app_camera_nn_pipe_set_address(buffer);
        __DMB();
        app_bqueue_put_ready(&nn_input_queue);
        s_ai_input_published++;
    }
    else { s_ai_input_dropped++; }
}

static void app_ai_set_stage(uint32_t stage, int32_t error)
{
    tx_mutex_get(&display.lock, TX_WAIT_FOREVER);
    display.info.ai.stage = stage;
    display.info.ai.error = error;
    if (stage == APP_AI_STAGE_ERROR)
    {
        display.ai_failed = 1U;
        display.info.people_valid = 0U;
        display.info.vision.valid = 0U;
    }
    tx_mutex_put(&display.lock);
}

static void app_ai_snapshot_counters(app_ai_diag_t *diag)
{
    uint32_t irq_state = __get_PRIMASK();
    __disable_irq();
    diag->pipe2_frames = s_ai_pipe2_frames;
    diag->input_published = s_ai_input_published;
    diag->input_dropped = s_ai_input_dropped;
    diag->isp_vsyncs = s_ai_isp_vsyncs;
    __set_PRIMASK(irq_state);
}

static VOID nn_thread_entry(ULONG id)
{
    uint32_t nn_in_len = LL_Buffer_len(LL_ATON_Input_Buffers_Info_network());
    uint32_t nn_out_len = LL_Buffer_len(LL_ATON_Output_Buffers_Info_network());
    uint32_t previous_started = HAL_GetTick();
    uint32_t waited_since = HAL_GetTick();
    UNUSED(id);
    while (1)
    {
        uint8_t *capture_buffer;
        uint8_t *output_buffer;
        int32_t io_status;
        if (!s_vision_running)
        { s_nn_paused = 1U; tx_thread_sleep(20U); waited_since = HAL_GetTick(); continue; }
        s_nn_paused = 0U;
        app_ai_set_stage(APP_AI_STAGE_WAIT_INPUT, 0);
        app_perf_stamp_t input_wait = app_perf_start();
        capture_buffer = app_bqueue_get_ready_timeout(&nn_input_queue, APP_AI_WAIT_TICKS);
        if (capture_buffer == NULL)
        {
            if ((uint32_t)(HAL_GetTick() - waited_since) >= APP_AI_STAGE_TIMEOUT_MS)
            { app_vision_request_restart(APP_AI_ERROR_INPUT_TIMEOUT); }
            continue;
        }
        if (!s_vision_running) { app_bqueue_put_free(&nn_input_queue); continue; }
        app_perf_record(APP_PERF_INPUT_WAIT, app_perf_elapsed_us(input_wait));
        __DMB();
        app_frame_meta_t meta = s_input_meta[capture_buffer == nn_input_buffers[0] ? 0U : 1U];
        app_ai_set_stage(APP_AI_STAGE_WAIT_OUTPUT, 0);
        app_perf_stamp_t output_wait = app_perf_start();
        output_buffer = app_bqueue_get_free_timeout(&nn_output_queue, APP_AI_WAIT_TICKS);
        if (output_buffer == NULL)
        {
            app_bqueue_put_free(&nn_input_queue);
            app_vision_request_restart(APP_AI_ERROR_OUTPUT_TIMEOUT);
            continue;
        }
        if (!s_vision_running)
        { app_bqueue_put_free(&nn_input_queue); app_bqueue_put_free(&nn_output_queue); continue; }
        app_perf_record(APP_PERF_OUTPUT_WAIT, app_perf_elapsed_us(output_wait));
        app_ai_set_stage(APP_AI_STAGE_BIND_IO, 0);
        io_status = (int32_t)LL_ATON_Set_User_Input_Buffer_network(0, capture_buffer, nn_in_len);
        if (io_status == LL_ATON_User_IO_NOERROR)
        { io_status = (int32_t)LL_ATON_Set_User_Output_Buffer_network(0, output_buffer, nn_out_len); }
        if (io_status != LL_ATON_User_IO_NOERROR)
        {
            app_bqueue_put_free(&nn_input_queue); app_bqueue_put_free(&nn_output_queue);
            app_vision_request_restart(io_status); continue;
        }
        SCB_InvalidateDCache_by_Addr(output_buffer, nn_out_len);
        uint32_t started = HAL_GetTick();
        tx_mutex_get(&display.lock, TX_WAIT_FOREVER);
        display.info.ai.stage = APP_AI_STAGE_NPU_RUN;
        display.info.ai.error = 0;
        display.info.ai.nn_started++;
        tx_mutex_put(&display.lock);
        app_perf_stamp_t nn_stamp = app_perf_start();
        LL_ATON_RT_Main(&NN_Instance_network);
        uint32_t completed = HAL_GetTick();
        app_perf_record(APP_PERF_NN, app_perf_elapsed_us(nn_stamp));
        tx_mutex_get(&display.lock, TX_WAIT_FOREVER);
        display.info.nn_period_ms = started - previous_started;
        display.info.ai.inference_ms = completed - started;
        display.info.ai.nn_completed++;
        display.info.ai.last_completed_ms = completed;
        if (s_vision_running) { display.info.ai.stage = APP_AI_STAGE_READY; }
        tx_mutex_put(&display.lock);
        previous_started = started;
        waited_since = completed;
        app_bqueue_put_free(&nn_input_queue);
        if (s_vision_running)
        {
            s_output_meta[output_buffer == nn_output_buffers[0] ? 0U : 1U] = meta;
            __DMB(); app_bqueue_put_ready(&nn_output_queue);
        }
        else { app_bqueue_put_free(&nn_output_queue); }
    }
}

static VOID pp_thread_entry(ULONG id)
{
    app_postprocess_params_t pp_params;
    uint8_t initialized = 0U;
    UNUSED(id);
    while (1)
    {
        if (!s_vision_running)
        { s_pp_paused = 1U; initialized = 0U; tx_thread_sleep(20U); continue; }
        s_pp_paused = 0U;
        if (!initialized)
        {
            int32_t status = app_postprocess_init(&pp_params);
            tx_mutex_get(&display.lock, TX_WAIT_FOREVER);
            display.info.ai.pp_status = status;
            tx_mutex_put(&display.lock);
            if (status != AI_OD_POSTPROCESS_ERROR_NO)
            { app_vision_request_restart(status); continue; }
            initialized = 1U;
        }
        uint8_t *output_buffer = app_bqueue_get_ready_timeout(&nn_output_queue, APP_AI_WAIT_TICKS);
        if (output_buffer == NULL) { continue; }
        if (!s_vision_running) { app_bqueue_put_free(&nn_output_queue); continue; }
        __DMB();
        app_frame_meta_t meta = s_output_meta[output_buffer == nn_output_buffers[0] ? 0U : 1U];
        od_pp_out_t pp_output = {0};
        app_perf_stamp_t pp_stamp = app_perf_start();
        int32_t status = app_postprocess_run((void *[]){(void *)output_buffer}, 1, &pp_output, &pp_params);
        uint32_t now = HAL_GetTick();
        app_perf_record(APP_PERF_PP, app_perf_elapsed_us(pp_stamp));
        app_vision_result_t result = {0};
        result.frame_id = meta.frame_id; result.captured_ms = meta.captured_ms; result.completed_ms = now;
        result.valid = status == AI_OD_POSTPROCESS_ERROR_NO && pp_output.nb_detect >= 0 &&
            pp_output.nb_detect <= (int32_t)APP_UI_MAX_DETECTIONS &&
            (uint32_t)(now - meta.captured_ms) < APP_AI_STALE_MS && s_vision_running;
        if (result.valid && pp_output.nb_detect > 0 && pp_output.pOutBuff == NULL) { result.valid = 0U; }
        if (result.valid)
        {
            for (int32_t index = 0; index < pp_output.nb_detect; index++)
            {
                const od_pp_outBuffer_t *box = &pp_output.pOutBuff[index];
                app_camera_rect_t rect;
                if (!isfinite(box->conf) || box->conf < 0.0f || box->conf > 1.0f ||
                    box->class_index != 0) { result.valid = 0U; break; }
                if (app_camera_map_nn_box(box->x_center, box->y_center, box->width, box->height, &rect))
                {
                    app_ui_detection_t *out = &result.boxes[result.count++];
                    out->x = rect.x; out->y = rect.y; out->width = rect.width; out->height = rect.height;
                    out->confidence = box->conf; out->class_index = box->class_index;
                }
            }
        }
        if (result.valid) {
            app_perf_record(APP_PERF_FRAME_LATENCY, app_perf_ms_to_us(now - meta.captured_ms));
        }
        tx_mutex_get(&display.lock, TX_WAIT_FOREVER);
        display.info.ai.pp_status = status;
        display.info.ai.pp_completed++;
        display.info.people_valid = result.valid && !display.ai_failed && s_vision_running;
        display.info.vision = result;
        if (display.info.people_valid)
        { display.info.people_count = pp_output.nb_detect; display.info.people_updated_ms = meta.captured_ms; }
        tx_mutex_put(&display.lock);
        app_bqueue_put_free(&nn_output_queue);
        if (status != AI_OD_POSTPROCESS_ERROR_NO) { app_vision_request_restart(status); }
    }
}

static VOID dp_thread_entry(ULONG id)
{
    /* Single-owner persistent snapshots keep DIAG's formatter/LVGL call
       stack clear. No other thread reads or writes these copies. */
    static app_ui_snapshot_t ui_snapshot;
    uint32_t ui_started_ms;
    uint32_t ui_elapsed_ms;
    app_frame_rate_t completed_rate = {0};
    static char diag_line[UART_TX_LINE_MAX];
    uart_tx_stats_t uart_stats;
    uint32_t now;
    uint32_t last_log_ms = HAL_GetTick();
    uint32_t last_perf_ms = last_log_ms;
    static app_perf_diag_t perf_snapshot;

    UNUSED(id);
    while (1)
    {
        uint32_t cycle_started = HAL_GetTick();
        app_perf_stamp_t dp_stamp = app_perf_start();
        tx_mutex_get(&display.lock, TX_WAIT_FOREVER);
        ui_snapshot = display.info;
        tx_mutex_put(&display.lock);

        if ((uint32_t)(cycle_started - last_perf_ms) >= 1000U) {
            app_perf_snapshot(&perf_snapshot);
            last_perf_ms = cycle_started;
        }
        ui_snapshot.perf = perf_snapshot;
        app_ai_snapshot_counters(&ui_snapshot.ai);
        app_frame_rate_sample(&completed_rate, ui_snapshot.ai.nn_completed,
            ui_snapshot.ai.last_completed_ms, ui_snapshot.ai.nn_completed != 0U, cycle_started);
        ui_snapshot.ai.completed_rate_x10 = completed_rate.rate_x10;
        ui_snapshot.ai.completed_rate_valid = completed_rate.valid;
        uart_tx_get_stats(&uart_stats);
        ui_snapshot.uart.queued_bytes = uart_stats.queued_bytes;
        ui_snapshot.uart.dropped_bytes = uart_stats.dropped_bytes;
        ui_snapshot.uart.tx_errors = uart_stats.tx_errors;
        ui_snapshot.uart.last_status = uart_stats.last_hal_status;
        app_camera_get_display_diag(&ui_snapshot.camera);
        app_lcd_get_display_diag(&ui_snapshot.lcd);
        ui_snapshot.relay_mask = app_climate_output_mask(&ui_snapshot.climate);
        app_ui_set_snapshot(&ui_snapshot);
        ui_started_ms = HAL_GetTick();
        app_perf_stamp_t ui_stamp = app_perf_start();
        app_ui_process();
        now = HAL_GetTick();
        app_perf_record(APP_PERF_UI, app_perf_elapsed_us(ui_stamp));
        ui_elapsed_ms = now - ui_started_ms;
        tx_mutex_get(&display.lock, TX_WAIT_FOREVER);
        display.info.ui_last_ms = ui_elapsed_ms;
        if (ui_elapsed_ms > display.info.ui_max_ms)
        {
            display.info.ui_max_ms = ui_elapsed_ms;
        }
        ui_snapshot.ui_last_ms = display.info.ui_last_ms;
        ui_snapshot.ui_max_ms = display.info.ui_max_ms;
        tx_mutex_put(&display.lock);

        if ((uint32_t)(now - last_log_ms) >= APP_DIAG_LOG_PERIOD_MS)
        {
            last_log_ms = now;
            /* One thread-context heartbeat per second, outside display.lock.
               NN stage and PP status remain separate, even without results. */
            int diag_length = snprintf(diag_line, sizeof(diag_line), "DIAG,TICKS=%lu,FPS=%lu.%lu/%u,AI=%lu/%ld,NN=%lu/%lu,"
                   "PP=%lu/%ld,P2=%lu,NMS=%lu,ISP=%lu/%lu,ISPMS=%lu/%lu,"
                   "UI=%lu/%lu,TOUCH=%u,E=%lu,R=%lu,I2C=%u/%lu/%08lX/%lu,REC=%lu,AGE=%lu,POLL=%lu/%lu,DP=%lu,UART=%lu/%lu\r\n",
                   (unsigned long)now,
                   (unsigned long)(ui_snapshot.camera.frame_rate_x10 / 10U),
                   (unsigned long)(ui_snapshot.camera.frame_rate_x10 % 10U),
                   (unsigned int)ui_snapshot.camera.frame_rate_valid,
                   (unsigned long)ui_snapshot.ai.stage,
                   (long)ui_snapshot.ai.error,
                   (unsigned long)ui_snapshot.ai.nn_started,
                   (unsigned long)ui_snapshot.ai.nn_completed,
                   (unsigned long)ui_snapshot.ai.pp_completed,
                   (long)ui_snapshot.ai.pp_status,
                   (unsigned long)ui_snapshot.ai.pipe2_frames,
                   (unsigned long)ui_snapshot.ai.inference_ms,
                   (unsigned long)ui_snapshot.ai.isp_runs,
                   (unsigned long)ui_snapshot.ai.isp_vsyncs,
                   (unsigned long)ui_snapshot.ai.isp_last_ms,
                   (unsigned long)ui_snapshot.ai.isp_max_ms,
                   (unsigned long)ui_snapshot.ui_last_ms,
                   (unsigned long)ui_snapshot.ui_max_ms,
                   (unsigned int)ui_snapshot.touch.ready,
                   (unsigned long)ui_snapshot.touch.error_count,
                   (unsigned long)ui_snapshot.touch.recovery_count,
                   (unsigned int)ui_snapshot.touch.error_kind,
                   (unsigned long)ui_snapshot.touch.last_hal_status,
                   (unsigned long)ui_snapshot.touch.last_hal_error,
                   (unsigned long)ui_snapshot.touch.last_elapsed_ms,
                   (unsigned long)ui_snapshot.ai.recovery_count,
                   (unsigned long)(now - ui_snapshot.vision.captured_ms),
                   (unsigned long)s_input_poll_last_ms, (unsigned long)s_input_poll_max_ms,
                   (unsigned long)s_dp_cycle_max_ms,
                   (unsigned long)uart_stats.dropped_bytes, (unsigned long)uart_stats.tx_errors);
            if (diag_length > 0 && (size_t)diag_length < sizeof(diag_line))
            { (void)uart_tx_enqueue_line(diag_line, (uint16_t)diag_length); }
            diag_length = snprintf(diag_line, sizeof(diag_line),
                "SENSOR,DHT=PG3,A=%lu,OK=%lu,FAIL=%lu,STREAK=%lu,S=%lu,HAS=%u,VALID=%u,AGE=%lu,PERIOD=%lu\r\n",
                (unsigned long)ui_snapshot.dht.attempts,
                (unsigned long)ui_snapshot.dht.successes,
                (unsigned long)ui_snapshot.dht.failures,
                (unsigned long)ui_snapshot.dht.consecutive_failures,
                (unsigned long)ui_snapshot.dht.last_status,
                (unsigned int)ui_snapshot.dht.has_sample,
                (unsigned int)ui_snapshot.dht.last_read_ok,
                (unsigned long)(ui_snapshot.dht.has_sample ?
                    now - ui_snapshot.dht.last_success_ms : 0U),
                (unsigned long)ui_snapshot.dht.retry_interval_ms);
            if (diag_length > 0 && (size_t)diag_length < sizeof(diag_line))
            { (void)uart_tx_enqueue_line(diag_line, (uint16_t)diag_length); }
            diag_length = snprintf(diag_line, sizeof(diag_line),
                "BG,F=%lu,SHOW=%lu,DROP=%lu,IDX=%u/%u/%u,PEND=%u,ERR=%lu,DMAERR=%lu,S=%ld\r\n",
                (unsigned long)ui_snapshot.lcd.background_frame_id,
                (unsigned long)ui_snapshot.lcd.background_presented,
                (unsigned long)ui_snapshot.lcd.background_dropped,
                (unsigned int)ui_snapshot.lcd.display_index,
                (unsigned int)ui_snapshot.lcd.fill_index,
                (unsigned int)ui_snapshot.lcd.pending_index,
                (unsigned int)ui_snapshot.lcd.background_reload_pending,
                (unsigned long)ui_snapshot.lcd.background_reload_errors,
                (unsigned long)ui_snapshot.lcd.background_target_errors,
                (long)ui_snapshot.lcd.background_last_status);
            if (diag_length > 0 && (size_t)diag_length < sizeof(diag_line))
            { (void)uart_tx_enqueue_line(diag_line, (uint16_t)diag_length); }
            /* Separate bounded line keeps the existing DIAG heartbeat within
               UART_TX_LINE_MAX. Times are wall durations, not utilization. */
            diag_length = snprintf(diag_line, sizeof(diag_line),
                "PERF,CLK=%s,PIX=%s,NN=%lu/%lu,PP=%lu/%lu,UI=%lu/%lu,FRAME=%lu/%lu,IN=%lu,DROP=%lu\r\n",
                perf_snapshot.cycle_clock_valid ? "DWT" : "TICK",
                app_ui_pixels_uses_helium() ? "MVE" : "SCALAR",
                (unsigned long)(perf_snapshot.stages[APP_PERF_NN].mean_x10_us / 10U),
                (unsigned long)perf_snapshot.stages[APP_PERF_NN].p95_us,
                (unsigned long)(perf_snapshot.stages[APP_PERF_PP].mean_x10_us / 10U),
                (unsigned long)perf_snapshot.stages[APP_PERF_PP].p95_us,
                (unsigned long)(perf_snapshot.stages[APP_PERF_UI].mean_x10_us / 10U),
                (unsigned long)perf_snapshot.stages[APP_PERF_UI].p95_us,
                (unsigned long)(perf_snapshot.stages[APP_PERF_FRAME_LATENCY].mean_x10_us / 10U),
                (unsigned long)perf_snapshot.stages[APP_PERF_FRAME_LATENCY].p95_us,
                (unsigned long)ui_snapshot.ai.input_published,
                (unsigned long)ui_snapshot.ai.input_dropped);
            if (diag_length > 0 && (size_t)diag_length < sizeof(diag_line))
            { (void)uart_tx_enqueue_line(diag_line, (uint16_t)diag_length); }
        }
        uint32_t cycle_ms = HAL_GetTick() - cycle_started;
        if (cycle_ms > s_dp_cycle_max_ms) { s_dp_cycle_max_ms = cycle_ms; }
        app_perf_record(APP_PERF_DP, app_perf_elapsed_us(dp_stamp));
        tx_thread_sleep(10U);
    }
}

static VOID isp_thread_entry(ULONG id)
{
    uint32_t started_ms;
    uint32_t elapsed_ms;

    UNUSED(id);
    while (1)
    {
        if (!s_vision_running)
        { s_isp_paused = 1U; tx_thread_sleep(20U); continue; }
        s_isp_paused = 0U;
        if (tx_semaphore_get(&isp_semaphore, APP_AI_WAIT_TICKS) != TX_SUCCESS) { continue; }
        if (!s_vision_running) { continue; }

        started_ms = HAL_GetTick();
        app_perf_stamp_t isp_stamp = app_perf_start();
        app_camera_isp_update();
        elapsed_ms = HAL_GetTick() - started_ms;
        app_perf_record(APP_PERF_ISP, app_perf_elapsed_us(isp_stamp));
        tx_mutex_get(&display.lock, TX_WAIT_FOREVER);
        display.info.ai.isp_runs++;
        display.info.ai.isp_last_ms = elapsed_ms;
        if (elapsed_ms > display.info.ai.isp_max_ms)
        {
            display.info.ai.isp_max_ms = elapsed_ms;
        }
        tx_mutex_put(&display.lock);
        /* A saturated ISP stream must still let NN/PP/camera threads run. */
        tx_thread_sleep(1U);
    }
}
