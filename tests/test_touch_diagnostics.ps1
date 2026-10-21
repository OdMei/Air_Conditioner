$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path -Parent $PSScriptRoot
$touchHeader = Get-Content -Raw -LiteralPath (Join-Path $projectRoot 'Project/Project/Air_Conditioner/Appli/Core/Inc/app_touch.h')
$touchSource = Get-Content -Raw -LiteralPath (Join-Path $projectRoot 'Project/Project/Air_Conditioner/Appli/Core/Src/app_touch.c')
$appSource = Get-Content -Raw -LiteralPath (Join-Path $projectRoot 'Project/Project/Air_Conditioner/Appli/Core/Src/app.c')
$uiSource = Get-Content -Raw -LiteralPath (Join-Path $projectRoot 'Project/Project/Air_Conditioner/Appli/Core/Src/app_ui.c')

if ($touchHeader -notmatch '(?s)typedef struct\s*\{.*?app_touch_state_t') {
    throw 'Touch diagnostics require a public app_touch_state_t snapshot.'
}

if ($touchHeader -notmatch 'app_touch_get_state') {
    throw 'Touch diagnostics require an app_touch_get_state accessor.'
}

foreach ($field in @('ft_probe', 'gt_probe', 'ft_ack', 'gt_ack',
                     'scl_level', 'sda_level', 'reset_level',
                     'interrupt_level', 'scan_count', 'finger_count')) {
    if (($field -eq 'scan_count' -and
         $touchHeader -notmatch "uint32_t\s+$field\s*;") -or
        ($field -ne 'scan_count' -and
         $touchHeader -notmatch "uint8_t\s+$field\s*;")) {
        throw "Touch diagnostics require the $field field."
    }
}

if ($touchSource -notmatch 's_touch_state') {
    throw 'Touch polling must retain a diagnostic state snapshot.'
}

if (($touchSource -notmatch 's_touch_state\.ft_probe') -or
    ($touchSource -notmatch 's_touch_state\.gt_probe')) {
    throw 'Touch initialization must retain both controller probe results.'
}

if (($touchSource -notmatch 's_touch_state\.scan_count\+\+') -or
    ($touchSource -notmatch 's_touch_state\.finger_count')) {
    throw 'Touch polling must retain scan count and raw finger diagnostics.'
}

if (($touchSource -notmatch 'ct_iic_probe\(FT5206_CMD_WR\)') -or
    ($touchSource -notmatch 'ct_iic_probe\(GT9XXX_CMD_WR\)')) {
    throw 'Touch initialization must retain both controller address acknowledgements.'
}

if (($touchSource -notmatch 'HAL_GPIO_ReadPin\(GPIOD, GPIO_PIN_14\)') -or
    ($touchSource -notmatch 'HAL_GPIO_ReadPin\(GPIOD, GPIO_PIN_4\)') -or
    ($touchSource -notmatch 'HAL_GPIO_ReadPin\(GPIOD, GPIO_PIN_10\)') -or
    ($touchSource -notmatch 'HAL_GPIO_ReadPin\(GPIOB, GPIO_PIN_3\)')) {
    throw 'Touch initialization must capture all bus, reset, and interrupt levels.'
}

if ($appSource -notmatch 'app_input_update_touch') {
    throw 'The input thread must publish touch diagnostics to the display.'
}

if ($uiSource -notmatch 'lv_indev_set_read_cb') {
    throw 'The LVGL port must register a touch input reader.'
}

if ($uiSource -notmatch 'LV_INDEV_STATE_PRESSED') {
    throw 'The LVGL port must publish pressed touch state.'
}

if ($uiSource -notmatch 'LV_INDEV_STATE_RELEASED') {
    throw 'The LVGL port must publish released touch state.'
}

$userUpdate = [regex]::Match($uiSource, '(?s)static void app_ui_update_user\(void\).*?\n\}\n\nstatic void app_ui_update_debug')
$debugUpdate = [regex]::Match($uiSource, '(?s)static void app_ui_update_debug\(void\).*?\n\}\n\nvoid app_ui_init')
if (-not $userUpdate.Success -or -not $debugUpdate.Success) { throw 'Both page update paths must exist.' }
if ($userUpdate.Value -match 'touch->|snapshot->touch|FT%u|GT%u|SCL%u|SDA%u') {
    throw 'GPIO and touch diagnostics belong on the debug page.'
}
foreach ($field in @('ready', 'pressed', 'x', 'y', 'ft_probe', 'gt_probe', 'ft_ack', 'gt_ack',
                     'scl_level', 'sda_level', 'reset_level', 'interrupt_level', 'scan_count', 'finger_count')) {
    if ($debugUpdate.Value -notmatch ('snapshot->touch\.' + $field + '\b')) {
        throw "Debug page must retain touch diagnostic $field."
    }
}
if ($touchSource -notmatch '(?s)ct_iic_last_error\(\).*?app_touch_record_failure\(now\)' -or
    $touchSource -notmatch '(?s)static void app_touch_record_failure.*?app_touch_release_pointer\(\)' -or
    $touchSource -notmatch '(?s)static void app_touch_release_pointer.*?s_touch_state\.pressed = 0U' -or
    $uiSource -notmatch 's_last_update = app_ui_tick\(\)') {
    throw 'Bus failures must release touch, and the UI must use its portable clock.'
}
Write-Output 'PASS: Touch snapshots drive LVGL; diagnostics are retained on DEBUG.'
