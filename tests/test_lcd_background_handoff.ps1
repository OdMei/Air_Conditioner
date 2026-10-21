param([string]$Compiler = 'C:/msys64/mingw64/bin/gcc.exe')
$ErrorActionPreference = 'Stop'
$bgTestRoot = Split-Path -Parent $PSScriptRoot
$bgTestOutput = [System.IO.Path]::GetFullPath((Join-Path $bgTestRoot '../../tmp/lcd_background_20261011/host'))
New-Item -ItemType Directory -Path $bgTestOutput -Force | Out-Null
$bgTestExe = Join-Path $bgTestOutput 'test_lcd_background_handoff.exe'
$bgCompilerArgs = @('-std=c11', '-Wall', '-Wextra', '-Werror', '-Wno-pointer-to-int-cast', '-D__POSTPROCESS_CONF_H__',
    ('-I' + (Join-Path $PSScriptRoot 'stubs_lcd')),
    ('-I' + (Join-Path $bgTestRoot 'Project/Project/Air_Conditioner/Appli/Core/Inc')),
    (Join-Path $bgTestRoot 'Project/Project/Air_Conditioner/Appli/Core/Src/app_lcd.c'),
    (Join-Path $bgTestRoot 'Project/Project/Air_Conditioner/Appli/Core/Src/app_bg_buffers.c'),
    (Join-Path $PSScriptRoot 'test_lcd_background_handoff.c'), '-o', $bgTestExe)
$bgOldPath = $env:PATH
try {
    $env:PATH = (Split-Path -Parent $Compiler) + ';' + $bgOldPath
    & $Compiler @bgCompilerArgs
    if ($LASTEXITCODE -ne 0) { throw "LCD background handoff compile failed: $LASTEXITCODE" }
    & $bgTestExe
    if ($LASTEXITCODE -ne 0) { throw "LCD background handoff assertions failed: $LASTEXITCODE" }
} finally { $env:PATH = $bgOldPath }
