$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$source = Join-Path $root 'Project/Project/Air_Conditioner/Appli/Core'
$camera = Get-Content -Raw -LiteralPath (Join-Path $source 'Src/app_camera.c')
$cameraHeader = Get-Content -Raw -LiteralPath (Join-Path $source 'Inc/app_camera.h')
$app = Get-Content -Raw -LiteralPath (Join-Path $source 'Src/app.c')
$ui = Get-Content -Raw -LiteralPath (Join-Path $source 'Src/app_ui.c')
$uiHeader = Get-Content -Raw -LiteralPath (Join-Path $source 'Inc/app_ui.h')

$cameraHeader += "`n" + (Get-Content -Raw -LiteralPath (Join-Path $root 'Project/Project/Air_Conditioner/Appli/Core/Inc/app_ui_port.h'))

$fields = @(
    'csi_status0', 'csi_status1', 'csi_error1', 'csi_error2',
    'pipe0_data_count', 'hal_error', 'pipe0_state', 'pipe1_state',
    'pipe2_state'
)

foreach ($field in $fields) {
    if ($cameraHeader -notmatch "uint32_t\s+$field;") {
        throw "Camera diagnostic is missing $field."
    }
    if ($uiHeader -notmatch 'app_camera_display_diag_t\s+camera;') {
        throw 'UI snapshot must reuse the complete camera diagnostic structure.'
    }
    if ($app -notmatch 'app_camera_get_display_diag\(&ui_snapshot\.camera\)' -or
        $ui -notmatch "snapshot->camera\.$field\b") {
        throw "The camera diagnostic $field does not reach the debug UI."
    }
}

foreach ($register in @('CSI->SR0', 'CSI->SR1', 'CSI->ERR1', 'CSI->ERR2',
                       'Instance->P0DCCNTR')) {
    if ($camera -notmatch [regex]::Escape($register)) {
        throw "Camera diagnostic does not sample $register."
    }
}

if ($ui -notmatch 'CSI %08lX/%08lX E%08lX/%08lX' -or
    $ui -notmatch 'Pipe %lu/%lu/%lu Raw%08lX H%08lX') {
    throw 'Debug mode must show CSI errors, pipe states and raw input count.'
}

Write-Output 'PASS: CSI/DCMIPP input diagnostics reach the debug page.'
