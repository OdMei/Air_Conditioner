$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$configPath = Join-Path $root 'Project/Project/Air_Conditioner/Appli/Core/Inc/app_config.h'
$cameraPath = Join-Path $root 'Project/Project/Air_Conditioner/Appli/Core/Src/app_camera.c'
$cameraHeaderPath = Join-Path $root 'Project/Project/Air_Conditioner/Appli/Core/Inc/app_camera.h'
$appPath = Join-Path $root 'Project/Project/Air_Conditioner/Appli/Core/Src/app.c'
$uiPath = Join-Path $root 'Project/Project/Air_Conditioner/Appli/Core/Src/app_ui.c'
$uiHeaderPath = Join-Path $root 'Project/Project/Air_Conditioner/Appli/Core/Inc/app_ui.h'
$config = Get-Content -Raw -LiteralPath $configPath
$camera = Get-Content -Raw -LiteralPath $cameraPath
$cameraHeader = Get-Content -Raw -LiteralPath $cameraHeaderPath
$app = Get-Content -Raw -LiteralPath $appPath
$ui = Get-Content -Raw -LiteralPath $uiPath
$uiHeader = Get-Content -Raw -LiteralPath $uiHeaderPath

$cameraHeader += "`n" + (Get-Content -Raw -LiteralPath (Join-Path $root 'Project/Project/Air_Conditioner/Appli/Core/Inc/app_ui_port.h'))

if ($config -notmatch '#define\s+APP_CAMERA_SENSOR_TEST_PATTERN_MODE\s+\(-1\)') {
    throw 'The live-camera build must disable the IMX335 test pattern.'
}

$patternFunction = [regex]::Match(
    $camera,
    '(?s)void app_camera_set_sensor_test_pattern\(void\).*?\n\}')
if (-not $patternFunction.Success) {
    throw 'Could not find app_camera_set_sensor_test_pattern.'
}
if (($patternFunction.Value -notmatch 'CMW_CAMERA_SetTestPattern\s*\(\s*APP_CAMERA_SENSOR_TEST_PATTERN_MODE\s*\)') -or
    ($patternFunction.Value -match '#if')) {
    throw 'Pass the disabled pattern mode to the sensor explicitly.'
}

$ispFunction = [regex]::Match(
    $camera,
    '(?s)void app_camera_isp_update\(void\).*?\n\}')
if (-not $ispFunction.Success -or
    $ispFunction.Value -notmatch 'CMW_CAMERA_Run\(\)' -or
    $ispFunction.Value -notmatch 'CMW_CAMERA_GetExposure\(' -or
    $ispFunction.Value -notmatch 'CMW_CAMERA_GetGain\(') {
    throw 'Capture ISP run status, exposure and gain after each ISP update.'
}

if (($cameraHeader -notmatch 'int32_t\s+isp_run_status;') -or
    ($cameraHeader -notmatch 'int32_t\s+sensor_exposure_us;') -or
    ($cameraHeader -notmatch 'int32_t\s+sensor_gain_mdb;') -or
    ($uiHeader -notmatch 'app_camera_display_diag_t\s+camera;')) {
    throw 'Expose ISP and sensor exposure diagnostics through the UI snapshot.'
}

if (($app -notmatch 'app_camera_get_display_diag\(&ui_snapshot\.camera\)') -or
    ($ui -notmatch 'snapshot->camera\.isp_run_status\b') -or
    ($ui -notmatch 'snapshot->camera\.sensor_exposure_us\b') -or
    ($ui -notmatch 'snapshot->camera\.sensor_gain_mdb\b') -or
    ($ui -notmatch 'ISP %ld/%ld Exp%ldus Gain%ldmdB')) {
    throw 'Display ISP status, exposure and gain on the debug page.'
}

Write-Output 'PASS: live sensor mode and ISP exposure diagnostics are wired.'
