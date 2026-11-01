param([string]$Compiler = 'E:/ST/STM32CubeCLT_1.18.0/GNU-tools-for-STM32/bin/arm-none-eabi-gcc.exe')
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$inc = Join-Path $root 'Project/Project/Air_Conditioner/Appli/Core/Inc'
& $Compiler '-std=gnu11' '-Wall' '-Wextra' '-Werror' '-fsyntax-only' ('-I' + $inc) (Join-Path $root 'tests/test_audio_es8388_service.c')
if ($LASTEXITCODE -ne 0) { throw "audio service syntax check failed: $LASTEXITCODE" }
