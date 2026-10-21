$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$project = Join-Path $root 'Project/Project/Air_Conditioner/Appli/Core'
$config = Get-Content -Raw -LiteralPath (Join-Path $project 'Inc/app_config.h')
$app = Get-Content -Raw -LiteralPath (Join-Path $project 'Src/app.c')
$lcd = Get-Content -Raw -LiteralPath (Join-Path $project 'Src/app_lcd.c')
$lcdHeader = Get-Content -Raw -LiteralPath (Join-Path $project 'Inc/app_lcd.h')

$mode = [regex]::Match($config, '#define\s+APP_CAMERA_LCD_TEST_PATTERN\s+\(([01])U\)')
if (-not $mode.Success) {
    throw 'The camera-area test pattern mode must explicitly select 0U or 1U.'
}
if ($mode.Groups[1].Value -eq '0') {
    Write-Output 'SKIP: active color-bar mode; this configuration selects the live camera. Static diagnostic implementation checks continue.'
}
if ($app -notmatch '(?s)#if \(APP_CAMERA_LCD_TEST_PATTERN != 0U\).*?app_lcd_show_camera_test_pattern\(\).*?#endif') {
    throw 'The test pattern must be shown before camera capture begins.'
}
if ($app -notmatch '(?s)static void app_camera_display_pipe_frame_cb\(void\).*?#if \(APP_CAMERA_LCD_TEST_PATTERN == 0U\).*?app_lcd_switch_bg_buffer\(\).*?#endif') {
    throw 'The diagnostic pattern buffer must stay outside the DCMIPP ring.'
}
if ($lcdHeader -notmatch 'app_lcd_show_camera_test_pattern' -or
    $lcd -notmatch 'app_lcd_bg_buffer\[DISPLAY_BUFFER_NB - 1U\]' -or
    $lcd -notmatch 'SCB_CleanDCache_by_Addr') {
    throw 'The test pattern must use a reserved, cache-cleaned LCD buffer.'
}
if ($lcd -notmatch '(?s)status = HAL_LTDC_ReloadLayer\(.*?if \(status == HAL_OK\).*?LTDC_Layer1->RCR.*?app_bg_buffers_init\(&app_lcd_bg_state, DISPLAY_BUFFER_NB,\s*DISPLAY_BUFFER_NB - 1U, 0U\)') {
    throw 'The display index must follow a successfully applied test buffer.'
}

Write-Output 'PASS: fixed color bars isolate LTDC scanout from DCMIPP writes.'
