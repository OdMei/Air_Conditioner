$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path -Parent $PSScriptRoot
$main = Get-Content -Raw -LiteralPath (Join-Path $projectRoot 'Project/Project/Air_Conditioner/Appli/Core/Src/main.c')
$touch = Get-Content -Raw -LiteralPath (Join-Path $projectRoot 'Project/Project/Air_Conditioner/Appli/Core/Src/app_touch.c')
$ft = Get-Content -Raw -LiteralPath (Join-Path $projectRoot 'Drivers/BSP/TOUCH/ft5206.c')
$config = Get-Content -Raw -LiteralPath (Join-Path $projectRoot 'Project/Project/Air_Conditioner/Appli/Core/Inc/app_config.h')
$ui = Get-Content -Raw -LiteralPath (Join-Path $projectRoot 'Project/Project/Air_Conditioner/Appli/Core/Src/app_ui.c')
$app = Get-Content -Raw -LiteralPath (Join-Path $projectRoot 'Project/Project/Air_Conditioner/Appli/Core/Src/app.c')
$uiHeader = Get-Content -Raw -LiteralPath (Join-Path $projectRoot 'Project/Project/Air_Conditioner/Appli/Core/Inc/app_ui.h')

$bus = Get-Content -Raw -LiteralPath (Join-Path $projectRoot 'Project/Project/Air_Conditioner/Drivers/BSP/BUS/bsp_bus.c')
if ($bus -notmatch 'bsp_i2c2_mutex' -or $app -notmatch 'bsp_i2c2_init\(\)') {
    throw 'Camera and touch must use one initialized, serialized hardware I2C2 bus.'
}

if ((-not $touch.Contains('tp_dev.touchtype = 0x81U;')) -or
    (-not $touch.Contains('ft5206_init()')) -or
    (-not $touch.Contains('gt9xxx_init()'))) {
    throw 'Touch startup must configure the 800x480 capacitive controller and probe FT/GT variants.'
}

function Get-ThreadBody([string]$name) {
    $body = [regex]::Match($app, "(?s)static VOID $name\(ULONG id\)\s*\{.*?\r?\n\}")
    if (-not $body.Success) { throw "Missing thread body: $name" }
    return $body.Value
}

$inputBody = Get-ThreadBody 'input_thread_entry'
$nnBody = Get-ThreadBody 'nn_thread_entry'
$cameraBody = Get-ThreadBody 'camera_thread_entry'
$dpBody = Get-ThreadBody 'dp_thread_entry'
$startupWait = 'tx_event_flags_get\(&camera_startup,\s*APP_CAMERA_STARTUP_DONE,\s*TX_OR,\s*&startup_flags,\s*APP_CAMERA_STARTUP_WAIT_TICKS\)'
$startupSet = 'tx_event_flags_set\(&camera_startup,\s*APP_CAMERA_STARTUP_DONE,\s*TX_OR\);'
$unlock = 'tx_mutex_put\(&display\.lock\);'
$comment = '(?:/\*.*?\*/\s*)?'

if ($app -notmatch 'static TX_EVENT_FLAGS_GROUP camera_startup;' -or
    $app -notmatch 'tx_event_flags_create\(&camera_startup' -or
    $app.IndexOf('tx_event_flags_create(&camera_startup') -gt $app.IndexOf('tx_thread_create(')) {
    throw 'Create the shared camera-completion event before any thread starts.'
}
if ([regex]::Matches($app, $startupWait).Count -ne 1 -or
    $app -match 'TX_OR_CLEAR|TX_AND_CLEAR' -or
    $inputBody -notmatch "(?s)$startupWait.*?app_touch_init\(\)" -or
    $nnBody -notmatch '(?s)if \(!s_vision_running\).*?s_nn_paused = 1U;.*?continue;.*?app_bqueue_get_ready_timeout' -or
    $nnBody -match 'app_camera_nn_pipe_start') {
    throw 'Touch must observe latched completion; supervised NN must remain paused until successful pipeline startup.'
}
# Publish after both statuses, outside their success conditionals. The input
# observer can arrive late, and the supervisor may repeat startup attempts.
$modelGate = [regex]::Match($cameraBody, '(?s)    if \(model_status != APP_MODEL_IO_OK\) \{.*?\n    \}')
if (-not $modelGate.Success -or
    $cameraBody.IndexOf('app_model_check_generated_io()') -gt $cameraBody.IndexOf('app_camera_init(') -or
    $modelGate.Value -notmatch 'app_ai_set_stage\(APP_AI_STAGE_ERROR, model_status\)' -or
    $modelGate.Value -notmatch "(?s)camera_init_status = APP_CAMERA_STATUS_SKIPPED;\s*display.info.camera_start_status = APP_CAMERA_STATUS_SKIPPED;.*?$unlock\s*\(void\)$startupSet\s*return;" -or
    [regex]::Matches($modelGate.Value, $startupSet).Count -ne 1) {
    throw 'Invalid model or weights must release touch startup after publishing failure outside the display lock, before starting camera or NPU.'
}
# The model failure is a terminal pre-start path. The existing retry supervisor
# must still publish once per attempt and may not acquire any early return.
$cameraSupervisor = $cameraBody.Remove($modelGate.Index, $modelGate.Length)
if ($cameraSupervisor -match '\breturn\b' -or
    $cameraSupervisor -notmatch 'start_status = APP_CAMERA_STATUS_SKIPPED;' -or
    $cameraSupervisor -notmatch '(?s)if \(init_status == CMW_ERROR_NONE\).*?app_camera_display_pipe_start' -or
    $cameraSupervisor -notmatch "(?s)camera_init_status = init_status;\s*display.info.camera_start_status = start_status;.*?$unlock\s*$comment$startupSet\s*if \(init_status == CMW_ERROR_NONE && start_status == CMW_ERROR_NONE\)" -or
    [regex]::Matches($cameraSupervisor, $startupSet).Count -ne 1 -or
    [regex]::Matches($cameraBody, $startupSet).Count -ne 2 -or
    $cameraSupervisor -notmatch '(?s)if \(init_status == CMW_ERROR_NONE && start_status == CMW_ERROR_NONE\).*?app_camera_nn_pipe_start.*?if \(nn_status == CMW_ERROR_NONE\).*?s_vision_running = 1U;' -or
    [regex]::Matches($cameraSupervisor, 's_vision_running = 1U;').Count -ne 1) {
    throw 'Camera startup must publish completion after statuses are unlocked on success and failure.'
}
$aiCreationFailure = 'display.info.ai.stage = APP_AI_STAGE_ERROR;\s*display.info.ai.error = APP_CAMERA_STATUS_THREAD_ERROR;\s*display.ai_failed = 1U;'
if ($app -notmatch "(?s)camera_init_status = APP_CAMERA_STATUS_THREAD_ERROR;\s*display.info.camera_start_status = APP_CAMERA_STATUS_SKIPPED;\s*$aiCreationFailure\s*$unlock\s*$startupSet" -or
    $app -notmatch "(?s)#else\s*tx_mutex_get\(&display.lock, TX_WAIT_FOREVER\);\s*display.info.camera_init_status = APP_CAMERA_STATUS_SKIPPED;\s*display.info.camera_start_status = APP_CAMERA_STATUS_SKIPPED;\s*display.info.ai.stage = APP_AI_STAGE_DISABLED;\s*$unlock\s*$startupSet\s*#endif") {
    throw 'Thread-creation failure and the disabled pipeline must also release startup waiters.'
}
if ($uiHeader -notmatch 'app_camera_display_diag_t\s+camera;' -or
    $uiHeader -notmatch 'app_lcd_display_diag_t\s+lcd;' -or
    $dpBody -notmatch "(?s)ui_snapshot = display.info;\s*$unlock.*?app_ai_snapshot_counters\(&ui_snapshot.ai\);.*?app_camera_get_display_diag\(&ui_snapshot.camera\);\s*app_lcd_get_display_diag\(&ui_snapshot.lcd\);.*?app_ui_set_snapshot\(&ui_snapshot\)" -or
    $dpBody -match "(?s)tx_mutex_get\(&display.lock, TX_WAIT_FOREVER\);(?:(?!$unlock).)*app_camera_get_display_diag") {
    throw 'Publish shared state plus both complete diagnostics from the display thread, outside its mutex.'
}

if ((-not $ft.Contains('raw_x = ((uint16_t)(buf[0] & 0x0FU) << 8U) + buf[1];')) -or
    (-not $ft.Contains('raw_y = ((uint16_t)(buf[2] & 0x0FU) << 8U) + buf[3];')) -or
    $ft -notmatch '(?s)if \(\(tp_dev\.touchtype & 0x01U\) != 0U\)\s*\{\s*x\[i\] = raw_y;\s*y\[i\] = raw_x;' -or
    (-not $ft.Contains('tp_dev.x[i] = x[i];')) -or
    (-not $ft.Contains('tp_dev.y[i] = y[i];')) -or
    $ft.IndexOf('tp_dev.x[i] = x[i];') -lt $ft.IndexOf('if (first_valid == 0xFFU)')) {
    throw 'FT must decode raw coordinates, preserve horizontal mapping, and publish only a complete valid report.'
}

if (-not $config.Contains('#define APP_INPUT_POLL_TICKS                    (5U)')) {
    throw 'Touch polling must run at 5 ms to preserve short taps.'
}

if ($app -notmatch '(?s)static VOID input_thread_entry.*?while \(1\).*?app_touch_poll\(\);.*?tx_thread_sleep\(APP_INPUT_POLL_TICKS\);') {
    throw 'The input thread must yield after each touch poll.'
}

if ($ui -notmatch 'Touch %s %s X%u Y%u' -or $ui -notmatch '"READY" : "NO DEV"') {
    throw 'The debug page must retain touch readiness and coordinate diagnostics.'
}
if ($app -notmatch '#define APP_CAMERA_STARTUP_WAIT_TICKS \(5UL \* TX_TIMER_TICKS_PER_SECOND\)' -or
    $inputBody -notmatch 'startup timed out; input continues') {
    throw 'Camera startup timeout must be bounded and let local input continue.'
}
Write-Output 'PASS: shared hardware I2C2 startup is bounded and keeps input working.'
