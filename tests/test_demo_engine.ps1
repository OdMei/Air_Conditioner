param([string]$Compiler = 'C:/msys64/mingw64/bin/gcc.exe')
$ErrorActionPreference = 'Stop'
$demoTestRoot = Split-Path -Parent $PSScriptRoot
$demoTestCore = Join-Path $demoTestRoot 'Project/Project/Air_Conditioner/Appli/Core'
$demoTestOutput = [System.IO.Path]::GetFullPath((Join-Path $demoTestRoot '../../tmp/v9_visual_20261011/host'))
New-Item -ItemType Directory -Path $demoTestOutput -Force | Out-Null
$demoTestExe = Join-Path $demoTestOutput 'test_demo_engine.exe'
$demoTestOldPath = $env:PATH
try {
    $env:PATH = (Split-Path -Parent $Compiler) + ';' + $demoTestOldPath
    & $Compiler '-std=c11' '-O2' '-Wall' '-Wextra' '-Werror' `
        ('-I' + (Join-Path $demoTestCore 'Inc')) `
        (Join-Path $demoTestCore 'Src/app_demo_engine.c') `
        (Join-Path $demoTestCore 'Src/app_climate_logic.c') `
        (Join-Path $PSScriptRoot 'test_demo_engine.c') '-lm' '-o' $demoTestExe
    if ($LASTEXITCODE -ne 0) { throw "Demo engine compile failed: $LASTEXITCODE" }
    & $demoTestExe
    if ($LASTEXITCODE -ne 0) { throw "Demo engine assertions failed: $LASTEXITCODE" }
} finally { $env:PATH = $demoTestOldPath }
