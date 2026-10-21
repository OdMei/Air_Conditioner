$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$source = Join-Path $root 'Project/Project/Air_Conditioner/Appli/Core'
$lcd = Get-Content -Raw -LiteralPath (Join-Path $source 'Src/app_lcd.c')
$lcdHeader = Get-Content -Raw -LiteralPath (Join-Path $source 'Inc/app_lcd.h')
$ui = Get-Content -Raw -LiteralPath (Join-Path $source 'Src/app_ui.c')
$uiHeader = Get-Content -Raw -LiteralPath (Join-Path $source 'Inc/app_ui.h')
$app = Get-Content -Raw -LiteralPath (Join-Path $source 'Src/app.c')

$lcdHeader += "`n" + (Get-Content -Raw -LiteralPath (Join-Path $root 'Project/Project/Air_Conditioner/Appli/Core/Inc/app_ui_port.h'))

if ($lcd -notmatch 'static void app_lcd_sample_completed_frame\(uint8_t buffer_index\)') {
    throw 'The probe must sample pixels from a completed camera frame.'
}
if ($lcd -notmatch 'app_bg_buffers_pin_front\(&app_lcd_bg_state, &completed_index, &completed_sequence\)' -or
    $lcd -notmatch 'app_bg_buffers_release_probe\(&app_lcd_bg_state, completed_index\)' -or
    $lcd -notmatch '(?s)void app_lcd_get_display_diag.*?__set_PRIMASK\(irq_state\);.*?app_lcd_sample_completed_frame\(completed_index\);') {
    throw 'Pin confirmed scanout, sample with interrupts restored, and release its ownership lease.'
}
$switch = [regex]::Match($lcd, '(?s)uint8_t app_lcd_switch_bg_buffer\(void\)\s*\{(.*?)\n\}').Groups[1].Value
if ($switch -match 'app_lcd_sample_completed_frame|SCB_InvalidateDCache_by_Addr') {
    throw 'The camera ISR must not read external-memory probes or invalidate the whole frame.'
}
if ($lcd -notmatch '(?s)SCB_InvalidateDCache_by_Addr\(\s*\(volatile void \*\)&pixels\[pixel_index\],\s*\(int32_t\)sizeof\(pixel\)\)') {
    throw 'CPU diagnostic pixel reads must still invalidate their sampled cache lines.'
}
if ($lcdHeader -notmatch 'uint16_t pixel_min;' -or
    $lcdHeader -notmatch 'uint16_t pixel_max;') {
    throw 'LCD diagnostics must expose the sampled pixel range.'
}
if ($uiHeader -notmatch 'app_lcd_display_diag_t\s+lcd;' -or
    $app -notmatch 'app_lcd_get_display_diag\(&ui_snapshot\.lcd\)' -or
    $ui -notmatch 'snapshot->lcd\.pixel_min\b' -or
    $ui -notmatch 'snapshot->lcd\.pixel_max\b') {
    throw 'The sampled range must reach the debug UI snapshot.'
}
if ($ui -notmatch 'Pixels %04X/%04X') {
    throw 'The debug page must show the sampled RGB565 pixel range.'
}

Write-Output 'PASS: pinned confirmed camera frame probes reach the debug page without DMA reuse.'
