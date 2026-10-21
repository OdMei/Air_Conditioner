$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$lcd = Get-Content -Raw -LiteralPath (Join-Path $root 'Project/Project/Air_Conditioner/Appli/Core/Src/app_lcd.c')
$app = Get-Content -Raw -LiteralPath (Join-Path $root 'Project/Project/Air_Conditioner/Appli/Core/Src/app.c')

if ($lcd -notmatch 'LCD_PIXEL_FORMAT_ARGB4444') {
    throw 'The baseline display layer must use the validated ARGB4444 format.'
}
if ($lcd -notmatch 'app_lcd_fg_buffer\[2\]\[LCD_FG_WIDTH \* LCD_FG_HEIGHT \* 2\]') {
    throw 'The baseline display must use the validated 16-bit front buffers.'
}
if ($app -notmatch 'app_ui_set_snapshot\(&ui_snapshot\);') {
    throw 'The LVGL display snapshot is not enabled.'
}
if ($app -notmatch 'app_ui_process\(\);') {
    throw 'The LVGL foreground renderer is not enabled.'
}
if ($app -notmatch 'app_camera_display_pipe_start') {
    throw 'The camera background pipeline is not enabled.'
}

Write-Output 'display baseline contract passed'
