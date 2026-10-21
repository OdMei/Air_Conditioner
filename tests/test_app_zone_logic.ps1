param([string]$Compiler = 'C:/msys64/mingw64/bin/gcc.exe')
$ErrorActionPreference = 'Stop'
$zoneTestRoot = Split-Path -Parent $PSScriptRoot
$zoneTestCore = Join-Path $zoneTestRoot 'Project/Project/Air_Conditioner/Appli/Core'
$zoneTestOutput = [System.IO.Path]::GetFullPath((Join-Path $zoneTestRoot '../../tmp/zone_logic_20261011/host'))
New-Item -ItemType Directory -Path $zoneTestOutput -Force | Out-Null
$zoneTestExe = Join-Path $zoneTestOutput 'test_app_zone_logic.exe'
$zoneTestOldPath = $env:PATH
try {
    $env:PATH = (Split-Path -Parent $Compiler) + ';' + $zoneTestOldPath
    & $Compiler '-std=c11' '-O2' '-Wall' '-Wextra' '-Werror' `
        ('-I' + (Join-Path $zoneTestCore 'Inc')) `
        (Join-Path $zoneTestCore 'Src/app_zone_logic.c') `
        (Join-Path $PSScriptRoot 'test_app_zone_logic.c') '-o' $zoneTestExe
    if ($LASTEXITCODE -ne 0) { throw "Zone logic compile failed: $LASTEXITCODE" }
    & $zoneTestExe
    if ($LASTEXITCODE -ne 0) { throw "Zone logic assertions failed: $LASTEXITCODE" }
} finally { $env:PATH = $zoneTestOldPath }
