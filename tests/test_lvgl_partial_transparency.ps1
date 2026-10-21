$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$ui = Get-Content -Raw -LiteralPath (Join-Path $root 'Project/Project/Air_Conditioner/Appli/Core/Src/app_ui.c')
$pixels = Get-Content -Raw -LiteralPath (Join-Path $root 'Project/Project/Air_Conditioner/Appli/Core/Src/app_ui_pixels.c')

if ($ui -notmatch 'lv_display_set_color_format\(s_display,\s*LV_COLOR_FORMAT_ARGB8888\)') {
    throw 'Transparent partial rendering requires an alpha-capable LVGL buffer.'
}
if ($ui -notmatch 'APP_UI_LVGL_PIXEL_BYTES \(4U\)' -or
    $ui -notmatch 'APP_UI_LCD_PIXEL_BYTES \(2U\)') {
    throw 'LVGL and LTDC framebuffer pixel sizes must remain independent.'
}
if ($ui -notmatch '(?s)memset\(buffer, 0,\s*LCD_FG_WIDTH \* LCD_FG_HEIGHT \* APP_UI_LCD_PIXEL_BYTES\)') {
    throw 'Clearing the LCD foreground must use its 16-bit pixel size.'
}
if ($ui -notmatch '#include "app_ui_pixels.h"' -or
    $pixels -notmatch 'void app_ui_pixels_convert\(') {
    throw 'LVGL ARGB8888 pixels must be converted explicitly to LTDC ARGB4444.'
}
if ($pixels -notmatch 'bgra\[3\]' -or $pixels -notmatch 'bgra\[2\]' -or
    $pixels -notmatch 'bgra\[1\]' -or $pixels -notmatch 'bgra\[0\]') {
    throw 'The pixel conversion must preserve alpha and all RGB channels.'
}
if ($ui -notmatch 'app_ui_pixels_convert\(target, LCD_FG_WIDTH, px_map, width, height\)' -or
    $pixels -notmatch 'while \(remaining != 0U\)') {
    throw 'The LVGL flush path must convert each ARGB8888 pixel.'
}

Write-Output 'PASS: partial transparent pixels retain alpha through the LTDC flush.'
