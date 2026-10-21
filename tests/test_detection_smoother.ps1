param([string]$Compiler = 'C:/msys64/mingw64/bin/gcc.exe')
$ErrorActionPreference = 'Stop'
$smootherTestRoot = Split-Path -Parent $PSScriptRoot
$smootherTestCore = Join-Path $smootherTestRoot 'Project/Project/Air_Conditioner/Appli/Core'
$smootherTestOutput = [System.IO.Path]::GetFullPath((Join-Path $smootherTestRoot '../../tmp/interaction_20261011/smoother'))
New-Item -ItemType Directory -Path $smootherTestOutput -Force | Out-Null
$smootherTestExe = Join-Path $smootherTestOutput 'test_detection_smoother.exe'
$smootherTestOldPath = $env:PATH
try {
    $env:PATH = (Split-Path -Parent $Compiler) + ';' + $smootherTestOldPath
    & $Compiler '-std=c11' '-O2' '-Wall' '-Wextra' '-Wpedantic' '-Werror' '-fstack-usage' `
        ('-I' + (Join-Path $smootherTestCore 'Inc')) `
        (Join-Path $smootherTestCore 'Src/app_detection_smoother.c') `
        (Join-Path $PSScriptRoot 'test_detection_smoother.c') '-lm' '-o' $smootherTestExe
    if ($LASTEXITCODE -ne 0) { throw "Detection smoother host compile failed: $LASTEXITCODE" }
    & $smootherTestExe
    if ($LASTEXITCODE -ne 0) { throw "Detection smoother assertions failed: $LASTEXITCODE" }
} finally { $env:PATH = $smootherTestOldPath }
