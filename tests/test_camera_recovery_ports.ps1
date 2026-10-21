param([string]$Compiler = 'C:/msys64/mingw64/bin/gcc.exe')
$ErrorActionPreference = 'Stop'
$recoveryTestRoot = Split-Path -Parent $PSScriptRoot
$recoveryTestOutput = [System.IO.Path]::GetFullPath((Join-Path $recoveryTestRoot '../../tmp/code_optimization_20261010/host'))
New-Item -ItemType Directory -Path $recoveryTestOutput -Force | Out-Null
function Read-ProductionFunction([string]$File, [string]$Name) {
    $recoveryText = Get-Content -Raw -LiteralPath (Join-Path $recoveryTestRoot $File)
    $recoveryMatch = [regex]::Match($recoveryText, "(?ms)^(?:static\s+)?(?:int32_t|void)\s+$Name\([^;]*?\)\s*\{")
    if (-not $recoveryMatch.Success) { throw "Function missing: $Name" }
    $recoveryStart = $recoveryMatch.Index
    $recoveryBrace = $recoveryStart + $recoveryMatch.Length - 1
    $recoveryDepth = 0
    for ($recoveryIndex = $recoveryBrace; $recoveryIndex -lt $recoveryText.Length; $recoveryIndex++) {
        if ($recoveryText[$recoveryIndex] -eq '{') { $recoveryDepth++ }
        if ($recoveryText[$recoveryIndex] -eq '}') {
            $recoveryDepth--
            if ($recoveryDepth -eq 0) { return $recoveryText.Substring($recoveryStart, $recoveryIndex - $recoveryStart + 1) }
        }
    }
    throw "Function has no closing brace: $Name"
}
$recoveryFunctions = @(
    (Read-ProductionFunction 'Middlewares/STM32_MW_CAMERA/sensors/cmw_imx335.c' 'CMW_IMX335_DeInit'),
    (Read-ProductionFunction 'Middlewares/STM32_MW_CAMERA/sensors/cmw_imx335.c' 'CMW_IMX335_VsyncEventCallback'),
    (Read-ProductionFunction 'Middlewares/STM32_MW_CAMERA/cmw_camera.c' 'CMW_CAMERA_DeInit'),
    (Read-ProductionFunction 'Project/Project/Air_Conditioner/Appli/Core/Src/app_camera.c' 'app_camera_pipe_stop'))
$recoveryTemplate = Get-Content -Raw -LiteralPath (Join-Path $PSScriptRoot 'test_camera_recovery_ports.c')
$recoveryGenerated = $recoveryTemplate.Replace('/* GENERATED_PORT_FUNCTIONS */', ($recoveryFunctions -join "`n`n"))
$recoveryTestSource = Join-Path $recoveryTestOutput 'test_camera_recovery_ports_generated.c'
[System.IO.File]::WriteAllText($recoveryTestSource, $recoveryGenerated, [System.Text.UTF8Encoding]::new($false))
$recoveryTestExe = Join-Path $recoveryTestOutput 'test_camera_recovery_ports.exe'
$recoveryOldPath = $env:PATH
try {
    $env:PATH = (Split-Path -Parent $Compiler) + ';' + $recoveryOldPath
    & $Compiler '-std=c11' '-Wall' '-Wextra' '-Werror' $recoveryTestSource '-o' $recoveryTestExe
    if ($LASTEXITCODE -ne 0) { throw "Camera recovery compile failed: $LASTEXITCODE" }
    & $recoveryTestExe
    if ($LASTEXITCODE -ne 0) { throw "Camera recovery assertions failed: $LASTEXITCODE" }
} finally { $env:PATH = $recoveryOldPath }
