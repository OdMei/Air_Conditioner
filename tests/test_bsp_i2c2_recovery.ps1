param([string]$Compiler = 'C:/msys64/mingw64/bin/gcc.exe')
$ErrorActionPreference = 'Stop'
$busTestProjectRoot = Split-Path -Parent $PSScriptRoot
$busTestOutputRoot = [System.IO.Path]::GetFullPath((Join-Path $busTestProjectRoot '../../tmp/touch_bus_20261009/host'))
New-Item -ItemType Directory -Path $busTestOutputRoot -Force | Out-Null
$busTestExecutable = Join-Path $busTestOutputRoot 'test_bsp_i2c2_recovery.exe'
$busCompileArguments = @('-std=c11','-Wall','-Wextra','-Werror',
    ('-I'+(Join-Path $PSScriptRoot 'stubs_bus')),
    ('-I'+(Join-Path $busTestProjectRoot 'Project/Project/Air_Conditioner/Drivers/BSP/BUS')),
    (Join-Path $busTestProjectRoot 'Project/Project/Air_Conditioner/Drivers/BSP/BUS/bsp_bus.c'),
    (Join-Path $PSScriptRoot 'test_bsp_i2c2_recovery.c'),'-o',$busTestExecutable)
$busTestOriginalPath = $env:PATH
try {
    $env:PATH = (Split-Path -Parent $Compiler)+';'+$busTestOriginalPath
    & $Compiler @busCompileArguments
    if ($LASTEXITCODE -ne 0) { throw "BSP host compile failed: $LASTEXITCODE" }
    & $busTestExecutable
    if ($LASTEXITCODE -ne 0) { throw "BSP host assertions failed: $LASTEXITCODE" }
} finally { $env:PATH = $busTestOriginalPath }
