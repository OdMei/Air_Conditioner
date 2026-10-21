$ErrorActionPreference = 'Stop'
$pipelineTestRoot = Split-Path -Parent $PSScriptRoot
$pipelineSource = Get-Content -Raw -LiteralPath (Join-Path $pipelineTestRoot 'Project/Project/Air_Conditioner/Appli/Core/Src/app.c')
$pipelineConfig = Get-Content -Raw -LiteralPath (Join-Path $pipelineTestRoot 'Project/Project/Air_Conditioner/Appli/Core/Inc/app_config.h')
function Read-PipelineFunction([string]$name) {
    $body = [regex]::Match($pipelineSource, "(?ms)^static (?:VOID|void|uint8_t) $name\([^;]*?\)\s*\{.*?^\}")
    if (-not $body.Success) { throw "Missing runtime function $name" }
    return $body.Value
}
$input = Read-PipelineFunction 'input_thread_entry'
$sensor = Read-PipelineFunction 'sensor_thread_entry'
$logger = Read-PipelineFunction 'logger_thread_entry'
$display = Read-PipelineFunction 'dp_thread_entry'
$camera = Read-PipelineFunction 'camera_thread_entry'
$nn = Read-PipelineFunction 'nn_thread_entry'
$pp = Read-PipelineFunction 'pp_thread_entry'
$isp = Read-PipelineFunction 'isp_thread_entry'
$pause = Read-PipelineFunction 'app_vision_wait_paused'
$restart = Read-PipelineFunction 'app_vision_request_restart'
$capture = Read-PipelineFunction 'app_camera_nn_pipe_frame_cb'

if ($pipelineSource -notmatch 'tx_thread_create\(&sensor_thread, "Sensor Thread"' -or
    $pipelineSource -notmatch 'tx_thread_create\(&logger_thread, "Logger Thread"' -or
    $sensor -notmatch 'dht11_get_data' -or $sensor -notmatch 'ap3216c_read_checked' -or
    $logger -notmatch 'uart_tx_process\(\);\s*tx_thread_sleep\(1U\)' -or
    ($input + $display) -match '\b(?:dht11_init|dht11_get_data|ap3216c_init|ap3216c_read_checked|HAL_UART_Transmit|uart_tx_process|printf)\s*\(') {
    throw 'Sensor transactions and UART transmission must run in dedicated workers, away from input/UI.'
}
if ($input -notmatch 'uart_tx_enqueue_line' -or $display -notmatch 'uart_tx_enqueue_line' -or
    $sensor -notmatch 'last_ap_retry\) >= 5000U' -or
    $sensor -notmatch '\+\+ap_errors >= 3U') {
    throw 'Hot threads must queue bounded logs; the light sensor needs bounded retry and reinitialization.'
}
if ($pipelineConfig -notmatch '#define APP_DHT11_PERIOD_MS\s+\(2000U\)' -or
    $sensor -notmatch 'last_dht\) >= APP_DHT11_PERIOD_MS' -or
    $sensor -notmatch '(?s)dht11_get_data.*?dht11_get_diag.*?app_input_update_sensor.*?last_dht = HAL_GetTick\(\)' -or
    $sensor -match 'dht_period|dht_errors') {
    throw 'DHT must retain fixed two-second polling after success or failure, publish real diagnostics and avoid long backoff or catch-up bursts.'
}
if ($pipelineSource -notmatch '#define APP_AI_WAIT_TICKS \(100U\)' -or
    $nn -notmatch 'app_bqueue_get_ready_timeout\(&nn_input_queue, APP_AI_WAIT_TICKS\)' -or
    $nn -notmatch 'app_bqueue_get_free_timeout\(&nn_output_queue, APP_AI_WAIT_TICKS\)' -or
    $pp -notmatch 'app_bqueue_get_ready_timeout\(&nn_output_queue, APP_AI_WAIT_TICKS\)' -or
    $isp -notmatch 'tx_semaphore_get\(&isp_semaphore, APP_AI_WAIT_TICKS\)' -or
    ($nn + $pp) -match '\bapp_bqueue_get_ready\(' -or
    $nn -notmatch 'app_vision_request_restart\(APP_AI_ERROR_INPUT_TIMEOUT\)' -or
    $nn -notmatch 'app_vision_request_restart\(APP_AI_ERROR_OUTPUT_TIMEOUT\)') {
    throw 'Every vision queue wait must be finite and input/output starvation must request recovery.'
}
if ($pause -notmatch 'while \(!s_nn_paused \|\| !s_pp_paused \|\| !s_isp_paused\)' -or
    $pause -notmatch 'HAL_GetTick\(\) - started\) >= 2000U.*?return 0U' -or
    $pause -notmatch 'tx_thread_sleep\(10U\)' -or
    $camera -notmatch '(?s)if \(!app_vision_wait_paused\(\)\).*?APP_AI_ERROR_QUIESCE_TIMEOUT.*?while \(1\).*?tx_thread_sleep\(1000U\).*?app_bqueue_reset' -or
    $camera.IndexOf('app_vision_wait_paused()') -gt $camera.IndexOf('app_camera_deinit()') -or
    $camera.IndexOf('app_vision_wait_paused()') -gt $camera.IndexOf('app_bqueue_reset(') -or
    $camera -notmatch 'retries >= APP_AI_RETRY_LIMIT' -or
    $pipelineSource -notmatch '#define APP_AI_RETRY_LIMIT \(3U\)' -or
    $pipelineSource -match '\b(?:tx_thread_terminate|tx_thread_delete|tx_thread_suspend|LL_ATON_RT_ResetNetwork|__HAL_RCC_NPU_FORCE_RESET)\s*\(') {
    throw 'Recovery must await all worker acknowledgements, bound retries, and never reclaim active NPU buffers.'
}
foreach ($worker in @(@{Text=$nn;Flag='s_nn_paused'}, @{Text=$pp;Flag='s_pp_paused'}, @{Text=$isp;Flag='s_isp_paused'})) {
    if ($worker.Text -notmatch ('(?s)if \(!s_vision_running\).*?' + $worker.Flag + ' = 1U;.*?continue;')) {
        throw ('Stopped worker must acknowledge quiescence: ' + $worker.Flag)
    }
}
if ($restart -notmatch '(?s)__get_PRIMASK\(\).*?__disable_irq\(\).*?s_vision_running = 0U;.*?__set_PRIMASK\(irq_state\)' -or
    $capture -notmatch 's_input_meta\[completed_index\]\.frame_id = s_ai_pipe2_frames' -or
    $capture -notmatch 's_input_meta\[completed_index\]\.captured_ms = HAL_GetTick\(\)' -or
    $nn -notmatch 's_input_meta\[capture_buffer == nn_input_buffers\[0\]' -or
    $nn -notmatch '(?s)s_output_meta\[output_buffer == nn_output_buffers\[0\].*?= meta;.*?__DMB\(\);.*?app_bqueue_put_ready' -or
    $pp -notmatch 'result.frame_id = meta.frame_id; result.captured_ms = meta.captured_ms; result.completed_ms = now;' -or
    $pp -notmatch '\(uint32_t\)\(now - meta.captured_ms\) < APP_AI_STALE_MS' -or
    $pp -notmatch 'display.info.people_updated_ms = meta.captured_ms') {
    throw 'Frame identity and capture age must survive DMA, NN and PP; completion time cannot refresh an old observation.'
}
Write-Output 'PASS: supervised pipeline keeps input/UI nonblocking, propagates capture metadata and recovers only after quiescence.'
