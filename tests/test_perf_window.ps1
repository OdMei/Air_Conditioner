param([string]$Compiler = 'C:/msys64/mingw64/bin/gcc.exe')
$ErrorActionPreference = 'Stop'
$perfTestRoot = Split-Path -Parent $PSScriptRoot
$perfTestCore = Join-Path $perfTestRoot 'Project/Project/Air_Conditioner/Appli/Core'
$perfTestOutput = [System.IO.Path]::GetFullPath((Join-Path $perfTestRoot '../../tmp/performance_20261010/host'))
New-Item -ItemType Directory -Path $perfTestOutput -Force | Out-Null
$perfTestExe = Join-Path $perfTestOutput 'test_perf_window.exe'
$perfTestOldPath = $env:PATH
try {
    $env:PATH = (Split-Path -Parent $Compiler) + ';' + $perfTestOldPath
    & $Compiler '-std=c11' '-Wall' '-Wextra' '-Wpedantic' '-Werror' '-O2' `
        ('-I' + (Join-Path $perfTestCore 'Inc')) `
        (Join-Path $perfTestCore 'Src/app_perf_window.c') `
        (Join-Path $PSScriptRoot 'test_perf_window.c') '-o' $perfTestExe
    if ($LASTEXITCODE -ne 0) { throw "Performance window compile failed: $LASTEXITCODE" }
    & $perfTestExe
    if ($LASTEXITCODE -ne 0) { throw "Performance window assertions failed: $LASTEXITCODE" }
} finally { $env:PATH = $perfTestOldPath }
