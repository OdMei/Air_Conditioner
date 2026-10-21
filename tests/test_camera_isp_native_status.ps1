$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$mw = Join-Path $root 'Middlewares/STM32_MW_CAMERA'
$cameraCore = Join-Path $mw 'cmw_camera.c'
$cameraHeaderPath = Join-Path $mw 'cmw_camera.h'
$sensorPath = Join-Path $mw 'sensors/cmw_imx335.c'
$sensorHeaderPath = Join-Path $mw 'sensors/cmw_imx335.h'
$appCameraPath = Join-Path $root 'Project/Project/Air_Conditioner/Appli/Core/Src/app_camera.c'
$appCameraHeaderPath = Join-Path $root 'Project/Project/Air_Conditioner/Appli/Core/Inc/app_camera.h'
$appPath = Join-Path $root 'Project/Project/Air_Conditioner/Appli/Core/Src/app.c'
$uiPath = Join-Path $root 'Project/Project/Air_Conditioner/Appli/Core/Src/app_ui.c'
$uiHeaderPath = Join-Path $root 'Project/Project/Air_Conditioner/Appli/Core/Inc/app_ui.h'

$cameraCoreText = Get-Content -Raw -LiteralPath $cameraCore
$cameraHeader = Get-Content -Raw -LiteralPath $cameraHeaderPath
$sensor = Get-Content -Raw -LiteralPath $sensorPath
$sensorHeader = Get-Content -Raw -LiteralPath $sensorHeaderPath
$appCamera = Get-Content -Raw -LiteralPath $appCameraPath
$appCameraHeader = Get-Content -Raw -LiteralPath $appCameraHeaderPath
$appCameraHeader += "`n" + (Get-Content -Raw -LiteralPath (Join-Path $root 'Project/Project/Air_Conditioner/Appli/Core/Inc/app_ui_port.h'))
$app = Get-Content -Raw -LiteralPath $appPath
$ui = Get-Content -Raw -LiteralPath $uiPath
$uiHeader = Get-Content -Raw -LiteralPath $uiHeaderPath

if ($sensorHeader -notmatch 'int32_t\s+last_isp_background_status;') {
    throw 'Preserve the native ISP background status in the IMX335 context.'
}
if ($sensor -notmatch '(?s)CMW_IMX335_Run\(void \*io_ctx\).*?last_isp_background_status\s*=\s*ret') {
    throw 'Capture ISP_BackgroundProcess result before mapping it to CMW error -4.'
}
if ($cameraHeader -notmatch 'CMW_CAMERA_GetLastISPBackgroundStatus\(' -or
    $cameraCoreText -notmatch 'CMW_CAMERA_GetLastISPBackgroundStatus\(') {
    throw 'Expose the raw ISP status through a checked camera diagnostic API.'
}
if ($appCameraHeader -notmatch 'int32_t\s+isp_native_status;') {
    throw 'Carry native ISP status through camera diagnostics.'
}
if ($appCamera -notmatch 'CMW_CAMERA_GetLastISPBackgroundStatus\(' -or
    $app -notmatch 'app_camera_get_display_diag\(&ui_snapshot\.camera\)') {
    throw 'Publish the native ISP status to the debug UI.'
}
if ($uiHeader -notmatch 'app_camera_display_diag_t\s+camera;' -or
    $ui -notmatch 'snapshot->camera\.isp_native_status\b' -or
    $ui -notmatch 'ISP %ld/%ld Exp%ldus Gain%ldmdB') {
    throw 'Show both mapped CMW and raw ISP status in the debug UI.'
}

Write-Output 'PASS: raw ISP failure status reaches the debug page.'
