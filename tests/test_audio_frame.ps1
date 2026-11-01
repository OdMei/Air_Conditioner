param([string]$Compiler = 'E:/ST/STM32CubeCLT_1.18.0/GNU-tools-for-STM32/bin/arm-none-eabi-gcc.exe')
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
& $Compiler '-std=gnu11' '-Wall' '-Wextra' '-Werror' '-fsyntax-only' ('-I' + (Join-Path $root 'Project/Project/Air_Conditioner/Appli/Core/Inc')) (Join-Path $root 'tests/test_audio_frame.c')
if ($LASTEXITCODE -ne 0) { throw "audio frame syntax check failed: $LASTEXITCODE" }
