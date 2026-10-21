param([string]$Compiler = 'C:/msys64/mingw64/bin/gcc.exe')
$ErrorActionPreference = 'Stop'
$touchDriversRoot = Split-Path -Parent $PSScriptRoot
$touchDriversOutput = [System.IO.Path]::GetFullPath((Join-Path $touchDriversRoot '../../tmp/touch_drivers_20261011/host'))
New-Item -ItemType Directory -Path $touchDriversOutput -Force | Out-Null
$touchDriversExecutable = Join-Path $touchDriversOutput 'test_touch_drivers.exe'
$touchDriversArguments = @(
    '-std=c11', '-Wall', '-Wextra', '-Werror',
    ('-I' + (Join-Path $PSScriptRoot 'stubs_touch_drivers')),
    ('-I' + (Join-Path $touchDriversRoot 'Drivers/BSP/TOUCH')),
    (Join-Path $touchDriversRoot 'Drivers/BSP/TOUCH/ft5206.c'),
    (Join-Path $touchDriversRoot 'Drivers/BSP/TOUCH/gt9xxx.c'),
    (Join-Path $PSScriptRoot 'test_touch_drivers.c'),
    '-o', $touchDriversExecutable
)
$touchDriversOriginalPath = $env:PATH
try {
    $env:PATH = (Split-Path -Parent $Compiler) + ';' + $touchDriversOriginalPath
    & $Compiler @touchDriversArguments
    if ($LASTEXITCODE -ne 0) { throw "Real FT/GT driver compilation failed: $LASTEXITCODE" }
    & $touchDriversExecutable
    if ($LASTEXITCODE -ne 0) { throw "Real FT/GT driver assertions failed: $LASTEXITCODE" }
} finally {
    $env:PATH = $touchDriversOriginalPath
}
