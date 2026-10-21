$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$ui = Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $root 'Project/Project/Air_Conditioner/Appli/Core/Src/app_ui.c')
# The home view is a shared LVGL module; validate displayed fields across both pages.
$ui += Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $root 'Project/Project/Air_Conditioner/Appli/Core/Src/app_ui_watch_home.c')
$header = Get-Content -Raw -LiteralPath (Join-Path $root 'Project/Project/Air_Conditioner/Appli/Core/Inc/app_ui.h')
$config = Get-Content -Raw -LiteralPath (Join-Path $root 'Project/Project/Air_Conditioner/Appli/Core/Inc/app_config.h')
$app = Get-Content -Raw -LiteralPath (Join-Path $root 'Project/Project/Air_Conditioner/Appli/Core/Src/app.c')

foreach ($text in @('Temperature', 'Humidity', 'State', 'Target', '推理更新率', 'People', '软件请求')) {
    if ($ui -notmatch [regex]::Escape($text)) {
        throw "Missing formal UI field: $text"
    }
}
foreach ($bit in @('01', '02', '04', '08')) {
    if ($ui -notmatch "mask\s*&\s*0x${bit}U") {
        throw "Missing displayed relay status bit 0x${bit}."
    }
}
if ($ui -notmatch 'app_climate_output_mask\(&snapshot->climate\)' -or
    $ui -notmatch 'snapshot->nn_period_ms' -or
    $ui -notmatch 'completed_rate_x10' -or $ui -notmatch 'completed_rate_valid' -or
    $ui -notmatch 'frame_rate_x10' -or $ui -notmatch 'frame_rate_valid') {
    throw 'UI must use shared requests, keep inference rate separate, and measure camera FPS from captured frames.'
}
if ($ui -match '10000U\s*/\s*snapshot->nn_period_ms') {
    throw 'Completed AI throughput must not use the inverse last inference interval.'
}
if ($header -notmatch 'relay_mask') { throw 'UI snapshot must carry relay states.' }
if ($app -notmatch 'relay_mask') { throw 'Application must publish relay states.' }
if ($config -notmatch '#define APP_CAMERA_PIPELINE_ENABLED\s+\(1U\)') {
    throw 'Camera/NPU pipeline must be enabled for the formal UI build.'
}
Write-Output 'PASS: formal user/debug UI contract is present.'
