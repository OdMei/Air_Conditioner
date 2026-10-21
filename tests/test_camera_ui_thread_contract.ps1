$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path -Parent $PSScriptRoot
$sourcePath = Join-Path $projectRoot 'Project/Project/Air_Conditioner/Appli/Core/Src/app.c'
$source = Get-Content -Raw -LiteralPath $sourcePath

if ($source -notmatch 'static TX_THREAD camera_thread;') {
    throw 'Camera startup must have a dedicated ThreadX thread.'
}

if ($source -notmatch 'static VOID camera_thread_entry\(ULONG id\)') {
    throw 'Camera thread entry function is missing.'
}

$runBody = [regex]::Match($source, '(?s)void app_run\(void\)\s*\{(?<body>.*?)\n\}\s*\n\s*static void app_input_update_sensor')
if (-not $runBody.Success) { throw 'Unable to locate app_run body.' }
if ($runBody.Groups['body'].Value -match 'app_camera_init\(') {
    throw 'app_run must not call the potentially blocking camera initializer.'
}
if ($runBody.Groups['body'].Value -match 'app_camera_display_pipe_start\(') {
    throw 'app_run must not call the potentially blocking display pipe start.'
}

$uiIndex = $source.IndexOf('app_ui_init(app_input_apply_action);')
$dpIndex = $source.IndexOf('"DP Thread"')
$cameraIndex = $source.IndexOf('"Camera Thread"')
if ($uiIndex -lt 0 -or $dpIndex -lt 0 -or $cameraIndex -lt 0) {
    throw 'UI, DP, and camera thread creation markers are required.'
}
if ($cameraIndex -lt $dpIndex) {
    throw 'Camera thread must be created after the UI thread.'
}

Write-Output 'PASS: camera startup is isolated from the LVGL UI thread.'
