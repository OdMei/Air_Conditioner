$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$project = Join-Path $root 'Project/Project/Air_Conditioner/Appli/Core'
$camera = Get-Content -Raw -LiteralPath (Join-Path $project 'Src/app_camera.c')
$cameraHeader = Get-Content -Raw -LiteralPath (Join-Path $project 'Inc/app_camera.h')
$lcd = Get-Content -Raw -LiteralPath (Join-Path $project 'Src/app_lcd.c')
$lcdHeader = Get-Content -Raw -LiteralPath (Join-Path $project 'Inc/app_lcd.h')
$app = Get-Content -Raw -LiteralPath (Join-Path $project 'Src/app.c')
$ui = Get-Content -Raw -LiteralPath (Join-Path $project 'Src/app_ui.c')

if ($camera -notmatch '(?s)static int32_t app_camera_display_pipe_init.*?status\s*=\s*CMW_CAMERA_SetPipeConfig\(DCMIPP_PIPE1.*?;.*?return status;' -or
    $camera -notmatch '(?s)status\s*=\s*app_camera_display_pipe_init\(.*?if\s*\(status\s*!=\s*CMW_ERROR_NONE\).*?return status;') {
    throw 'Display-pipe configuration errors must propagate to camera status.'
}
if ($cameraHeader -notmatch 'app_camera_display_diag_t' -or
    $camera -notmatch 'P1PPM0PR' -or
    $camera -notmatch 'P1PPM0AR1' -or
    $camera -notmatch 'frame_count') {
    throw 'Camera diagnostics must expose configured/hardware pitch, DMA address, and frame count.'
}
if ($camera -notmatch '__get_PRIMASK\(\)' -or
    $camera -notmatch '__set_PRIMASK\(') {
    throw 'Camera diagnostics shared with the startup thread and ISR must be snapshotted atomically.'
}
if ($lcdHeader -notmatch 'app_lcd_display_diag_t' -or
    $lcd -notmatch 'LTDC_Layer1->CFBLR' -or
    $lcd -notmatch 'LTDC_Layer1->CFBAR') {
    throw 'LCD diagnostics must expose background-layer pitch and address.'
}
if ($lcd -notmatch 'static app_bg_buffers_t app_lcd_bg_state;' -or
    $lcd -notmatch '(?s)__disable_irq\(\);\s*diag->pitch_bytes.*?diag->display_index = app_lcd_bg_state.front;.*?diag->fill_index = app_lcd_bg_state.fill;.*?__set_PRIMASK\(irq_state\);') {
    throw 'LCD indices shared with the frame ISR must be sampled atomically.'
}
if ($app -notmatch 'app_camera_get_display_diag' -or
    $app -notmatch 'app_lcd_get_display_diag') {
    throw 'The display task must publish camera and LTDC diagnostics.'
}
if ($ui -notmatch 'CAM CFG' -or $ui -notmatch 'DMA %08lX' -or
    $ui -notmatch 'Frames %lu') {
    throw 'Debug mode must show the pipe status, DMA address, and frame count.'
}

Write-Output 'PASS: camera-to-LTDC diagnostics are exposed in debug mode.'
