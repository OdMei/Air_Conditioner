param([string]$Compiler = 'C:/msys64/mingw64/bin/gcc.exe')
$ErrorActionPreference = 'Stop'
$touchTestProjectRoot = Split-Path -Parent $PSScriptRoot
$touchTestOutputRoot = [System.IO.Path]::GetFullPath((Join-Path $touchTestProjectRoot '../../tmp/touch_recovery_20261009/host'))
New-Item -ItemType Directory -Path $touchTestOutputRoot -Force | Out-Null
$touchTestExecutable = Join-Path $touchTestOutputRoot 'test_app_touch_recovery.exe'
$touchCompileArguments = @(
    '-std=c11', '-Wall', '-Wextra', '-Werror',
    ('-I' + (Join-Path $PSScriptRoot 'stubs_touch')),
    ('-I' + (Join-Path $touchTestProjectRoot 'Project/Project/Air_Conditioner/Appli/Core/Inc')),
    ('-I' + (Join-Path $touchTestProjectRoot 'Drivers/BSP/TOUCH')),
    (Join-Path $touchTestProjectRoot 'Project/Project/Air_Conditioner/Appli/Core/Src/app_touch.c'),
    (Join-Path $touchTestProjectRoot 'Drivers/BSP/TOUCH/ctiic.c'),
    (Join-Path $PSScriptRoot 'test_app_touch_recovery.c'),
    '-o', $touchTestExecutable
)
$touchTestOriginalPath = $env:PATH
try {
    $env:PATH = (Split-Path -Parent $Compiler) + ';' + $touchTestOriginalPath
    & $Compiler @touchCompileArguments
    if ($LASTEXITCODE -ne 0) { throw "Touch host compile failed: $LASTEXITCODE" }
    & $touchTestExecutable
    if ($LASTEXITCODE -ne 0) { throw "Touch host assertions failed: $LASTEXITCODE" }
} finally {
    $env:PATH = $touchTestOriginalPath
}
