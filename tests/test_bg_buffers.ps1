param([string]$Compiler = 'C:/msys64/mingw64/bin/gcc.exe')
$ErrorActionPreference = 'Stop'
$bgTestRoot = Split-Path -Parent $PSScriptRoot
$bgTestCore = Join-Path $bgTestRoot 'Project/Project/Air_Conditioner/Appli/Core'
$bgTestOutput = [System.IO.Path]::GetFullPath((Join-Path $bgTestRoot '../../tmp/runtime_optimization_20261011/host'))
New-Item -ItemType Directory -Path $bgTestOutput -Force | Out-Null
$bgTestExe = Join-Path $bgTestOutput 'test_bg_buffers.exe'
$bgTestOldPath = $env:PATH
try {
    $env:PATH = (Split-Path -Parent $Compiler) + ';' + $bgTestOldPath
    & $Compiler '-std=c11' '-O2' '-Wall' '-Wextra' '-Werror' `
        ('-I' + (Join-Path $bgTestCore 'Inc')) `
        (Join-Path $bgTestCore 'Src/app_bg_buffers.c') `
        (Join-Path $PSScriptRoot 'test_bg_buffers.c') '-o' $bgTestExe
    if ($LASTEXITCODE -ne 0) { throw "Background ownership compile failed: $LASTEXITCODE" }
    & $bgTestExe
    if ($LASTEXITCODE -ne 0) { throw "Background ownership assertions failed: $LASTEXITCODE" }
} finally { $env:PATH = $bgTestOldPath }
