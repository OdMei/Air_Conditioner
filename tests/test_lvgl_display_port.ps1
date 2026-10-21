$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$ui = Get-Content -Raw -LiteralPath (Join-Path $root 'Project/Project/Air_Conditioner/Appli/Core/Src/app_ui.c')
$lcd = Get-Content -Raw -LiteralPath (Join-Path $root 'Project/Project/Air_Conditioner/Appli/Core/Src/app_lcd.c')
$app = Get-Content -Raw -LiteralPath (Join-Path $root 'Project/Project/Air_Conditioner/Appli/Core/Src/app.c')
$pixels = Get-Content -Raw -LiteralPath (Join-Path $root 'Project/Project/Air_Conditioner/Appli/Core/Src/app_ui_pixels.c')

if ($ui -notmatch 'LV_COLOR_FORMAT_ARGB8888') {
    throw 'LVGL must render transparent partial areas with per-pixel alpha.'
}
if ($ui -notmatch 'APP_UI_LVGL_PIXEL_BYTES \(4U\)') {
    throw 'The LVGL partial draw buffers must use four-byte ARGB8888 pixels.'
}
if ($ui -notmatch 'app_ui_pixels_convert\(target, LCD_FG_WIDTH, px_map, width, height\)') {
    throw 'The LVGL flush callback must convert ARGB8888 to LTDC ARGB4444.'
}
if ($pixels -notmatch 'src_bgra \+= \(size_t\)width \* 4U;' -or
    $pixels -notmatch 'dst \+= dst_stride_pixels;') {
    throw 'The partial LVGL buffer must advance by its rendered area stride.'
}
if ($pixels -match 'src_bgra \+= LCD_FG_WIDTH;') {
    throw 'The partial LVGL buffer must not use the full display width stride.'
}
if ($ui -notmatch 'app_lcd_get_fg_draw_buffer' -or
    $ui -notmatch 'app_lcd_present_fg_buffer') {
    throw 'The LVGL port must use the validated foreground double buffer.'
}
if ($lcd -notmatch '(?s)void app_lcd_present_fg_buffer\(uint8_t \*buffer\).*?HAL_LTDC_ReloadLayer') {
    throw 'The LCD port must commit an LVGL-rendered foreground buffer.'
}
if ($app -notmatch 'app_ui_init\(' -or $app -notmatch 'app_ui_process\(') {
    throw 'The display thread must initialize and process LVGL.'
}
if ($app -match 'app_display_network_output\(&display_info\);') {
    throw 'The legacy foreground renderer must be disabled when LVGL is active.'
}

Write-Output 'LVGL alpha-capable display port contract passed'
