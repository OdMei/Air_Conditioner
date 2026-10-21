$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$header = Join-Path $root 'Project/Project/Air_Conditioner/Appli/Core/Inc/app_ui.h'
$source = Join-Path $root 'Project/Project/Air_Conditioner/Appli/Core/Src/app_ui.c'

if (-not (Test-Path -LiteralPath $header)) { throw 'app_ui.h is missing' }
if (-not (Test-Path -LiteralPath $source)) { throw 'app_ui.c is missing' }

$h = Get-Content -Raw -LiteralPath $header
$c = Get-Content -Raw -LiteralPath $source
foreach ($symbol in @('app_ui_init', 'app_ui_set_snapshot', 'app_ui_process', 'LV_INDEV_TYPE_POINTER')) {
    if (($h + $c) -notmatch [regex]::Escape($symbol)) {
        throw "Missing UI contract symbol: $symbol"
    }
}
if ($c -notmatch 'lv_display_set_flush_cb') { throw 'LVGL display flush callback is missing' }
if ($c -notmatch 'lv_indev_set_read_cb') { throw 'LVGL touch input callback is missing' }
if ($c -notmatch 'USER|DEBUG') { throw 'Dual UI pages are missing' }
Write-Output 'app_ui contract passed'
