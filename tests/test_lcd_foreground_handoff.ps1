param([string]$Compiler = 'C:/msys64/mingw64/bin/gcc.exe')
$ErrorActionPreference = 'Stop'
$lcdTestRoot = Split-Path -Parent $PSScriptRoot
$lcdTestOutput = [System.IO.Path]::GetFullPath((Join-Path $lcdTestRoot '../../tmp/code_optimization_20261010/host'))
New-Item -ItemType Directory -Path $lcdTestOutput -Force | Out-Null
$lcdTestExe = Join-Path $lcdTestOutput 'test_lcd_foreground_handoff.exe'
$lcdCompilerArgs = @('-std=c11', '-Wall', '-Wextra', '-Werror', '-Wno-pointer-to-int-cast', '-D__POSTPROCESS_CONF_H__',
    ('-I' + (Join-Path $PSScriptRoot 'stubs_lcd')),
    ('-I' + (Join-Path $lcdTestRoot 'Project/Project/Air_Conditioner/Appli/Core/Inc')),
    (Join-Path $lcdTestRoot 'Project/Project/Air_Conditioner/Appli/Core/Src/app_lcd.c'),
    (Join-Path $lcdTestRoot 'Project/Project/Air_Conditioner/Appli/Core/Src/app_bg_buffers.c'),
    (Join-Path $PSScriptRoot 'test_lcd_foreground_handoff.c'), '-o', $lcdTestExe)
$lcdOldPath = $env:PATH
try {
    $env:PATH = (Split-Path -Parent $Compiler) + ';' + $lcdOldPath
    & $Compiler @lcdCompilerArgs
    if ($LASTEXITCODE -ne 0) { throw "LCD handoff compile failed: $LASTEXITCODE" }
    & $lcdTestExe
    if ($LASTEXITCODE -ne 0) { throw "LCD handoff assertions failed: $LASTEXITCODE" }
} finally { $env:PATH = $lcdOldPath }
