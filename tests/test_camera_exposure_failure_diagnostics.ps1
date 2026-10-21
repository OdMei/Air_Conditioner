$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$sensorDir = Join-Path $root 'Middlewares/STM32_MW_CAMERA/sensors/imx335'
$sensorHeader = Get-Content -Raw -LiteralPath (Join-Path $sensorDir 'imx335.h')
$sensorSource = Get-Content -Raw -LiteralPath (Join-Path $sensorDir 'imx335.c')
$cameraDir = Join-Path $root 'Middlewares/STM32_MW_CAMERA'
$cameraHeader = Get-Content -Raw -LiteralPath (
    Join-Path $cameraDir 'cmw_camera.h')
$cameraSource = Get-Content -Raw -LiteralPath (
    Join-Path $cameraDir 'cmw_camera.c')
$appCore = Join-Path $root 'Project/Project/Air_Conditioner/Appli/Core'
$appHeader = Get-Content -Raw -LiteralPath (
    Join-Path $appCore 'Inc/app_camera.h')
$appSource = Get-Content -Raw -LiteralPath (
    Join-Path $appCore 'Src/app_camera.c')
$uiHeader = Get-Content -Raw -LiteralPath (
    Join-Path $appCore 'Inc/app_ui.h')
$uiSource = Get-Content -Raw -LiteralPath (
    Join-Path $appCore 'Src/app_ui.c')

$appHeader += "`n" + (Get-Content -Raw -LiteralPath (Join-Path $root 'Project/Project/Air_Conditioner/Appli/Core/Inc/app_ui_port.h'))

foreach ($field in @(
        'requested_exposure_us', 'vmax_lines', 'shutter_lines',
        'failure_stage', 'driver_status')) {
    if ($sensorHeader -notmatch "\b$field\b") {
        throw "IMX335 diagnostics must record $field."
    }
}

foreach ($stage in @(
        'READ_VMAX', 'CALCULATE_SHUTTER', 'ASSERT_HOLD',
        'WRITE_SHUTTER', 'RELEASE_HOLD')) {
    if ($sensorSource -notmatch "IMX335_EXPOSURE_STAGE_$stage") {
        throw "IMX335 diagnostics must distinguish $stage failures."
    }
}

if ($sensorSource -notmatch 'IMX335_GetExposureDiagnostics' -or
    $cameraSource -notmatch 'CMW_CAMERA_GetExposureDiagnostics' -or
    $cameraHeader -notmatch 'CMW_CAMERA_GetExposureDiagnostics') {
    throw 'Expose IMX335 exposure failures through the camera middleware.'
}

foreach ($field in @(
        'exposure_request_us', 'exposure_vmax_lines',
        'exposure_shutter_lines', 'exposure_failure_stage',
        'exposure_driver_status')) {
    if (($appHeader -notmatch "\b$field\b") -or
        ($uiHeader -notmatch 'app_camera_display_diag_t\s+camera;') -or
        ($uiSource -notmatch "snapshot->camera\.$field\b")) {
        throw "Carry $field from the camera diagnostics to the UI snapshot."
    }
}

if ($appSource -notmatch 'CMW_CAMERA_GetExposureDiagnostics' -or
    $uiSource -notmatch 'Set%ld V%lu SH%lu Step%ld Err%ld') {
    throw 'Show exposure request, VMAX, shutter, stage and driver status.'
}

Write-Output 'PASS: exposure diagnostics reach the debug UI.'
