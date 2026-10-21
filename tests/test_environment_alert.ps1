param([string]$Compiler = 'C:/msys64/mingw64/bin/gcc.exe')
$ErrorActionPreference = 'Stop'
$environmentTestRoot = Split-Path -Parent $PSScriptRoot
$environmentTestCore = Join-Path $environmentTestRoot 'Project/Project/Air_Conditioner/Appli/Core'
$environmentTestOutput = [System.IO.Path]::GetFullPath((Join-Path $environmentTestRoot '../../tmp/environment_20261012_v9_5/native'))
New-Item -ItemType Directory -Path $environmentTestOutput -Force | Out-Null
$environmentTestOldPath = $env:PATH
try {
    $env:PATH = (Split-Path -Parent $Compiler) + ';' + $environmentTestOldPath
    foreach ($environmentTestMode in @('production', 'fast_test_clock')) {
        $environmentTestExe = Join-Path $environmentTestOutput ('test_environment_alert_' + $environmentTestMode + '.exe')
        $environmentCompilerArgs = @('-std=c11', '-O2', '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-fstack-usage',
            ('-I' + (Join-Path $environmentTestCore 'Inc')),
            (Join-Path $environmentTestCore 'Src/app_environment_alert.c'),
            (Join-Path $PSScriptRoot 'test_environment_alert.c'), '-o', $environmentTestExe)
        if ($environmentTestMode -eq 'fast_test_clock') { $environmentCompilerArgs += '-DAPP_ENV_HOLD_MIN_MS=1' }
        & $Compiler @environmentCompilerArgs
        if ($LASTEXITCODE -ne 0) { throw "Environment alert $environmentTestMode compile failed: $LASTEXITCODE" }
        & $environmentTestExe
        if ($LASTEXITCODE -ne 0) { throw "Environment alert $environmentTestMode assertions failed: $LASTEXITCODE" }
    }
} finally { $env:PATH = $environmentTestOldPath }
