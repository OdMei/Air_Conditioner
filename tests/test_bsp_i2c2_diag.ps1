param([string]$Compiler = 'C:/msys64/mingw64/bin/gcc.exe')
$ErrorActionPreference = 'Stop'
$diagTestProjectRoot = Split-Path -Parent $PSScriptRoot
$diagTestOutputRoot = [System.IO.Path]::GetFullPath((Join-Path $diagTestProjectRoot '../../tmp/bus_diag_20261011/host'))
New-Item -ItemType Directory -Path $diagTestOutputRoot -Force | Out-Null
$diagTestExecutable = Join-Path $diagTestOutputRoot 'test_bsp_i2c2_diag.exe'
$diagCompileArguments = @('-std=c11','-Wall','-Wextra','-Werror',
    ('-I'+(Join-Path $PSScriptRoot 'stubs_bus_diag')),
    ('-I'+(Join-Path $PSScriptRoot 'stubs_bus')),
    ('-I'+(Join-Path $diagTestProjectRoot 'Project/Project/Air_Conditioner/Drivers/BSP/BUS')),
    (Join-Path $PSScriptRoot 'test_bsp_i2c2_diag.c'),'-o',$diagTestExecutable)
$diagTestOriginalPath = $env:PATH
try {
    $env:PATH = (Split-Path -Parent $Compiler)+';'+$diagTestOriginalPath
    & $Compiler @diagCompileArguments
    if ($LASTEXITCODE -ne 0) { throw "BSP diagnostic host compile failed: $LASTEXITCODE" }
    & $diagTestExecutable
    if ($LASTEXITCODE -ne 0) { throw "BSP diagnostic host assertions failed: $LASTEXITCODE" }
} finally { $env:PATH = $diagTestOriginalPath }
