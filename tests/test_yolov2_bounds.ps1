param([string]$Compiler = 'C:/msys64/mingw64/bin/gcc.exe')
$ErrorActionPreference = 'Stop'
$visionTestProjectRoot = Split-Path -Parent $PSScriptRoot
$visionTestOutputRoot = [System.IO.Path]::GetFullPath((Join-Path $visionTestProjectRoot '../../tmp/code_optimization_20261010/host'))
New-Item -ItemType Directory -Path $visionTestOutputRoot -Force | Out-Null
$visionTestExecutable = Join-Path $visionTestOutputRoot 'yolov2_bounds.exe'
# Two existing signedness warnings in unrelated vendor integer helpers are
# reported but do not prevent testing the scalar YOLOv2 path unchanged.
$visionCompileArguments = @('-std=c11', '-O1', '-Wall', '-Wextra', '-Werror', '-Wno-error=sign-compare',
    ('-I' + (Join-Path $PSScriptRoot 'stubs_vision')),
    ('-I' + (Join-Path $visionTestProjectRoot 'Middlewares/STM32_VISION_MODELS_PP/Inc')),
    ('-I' + (Join-Path $visionTestProjectRoot 'Middlewares/STM32_VISION_MODELS_PP/Src')),
    (Join-Path $visionTestProjectRoot 'Middlewares/STM32_VISION_MODELS_PP/Src/od_pp_yolov2.c'),
    (Join-Path $visionTestProjectRoot 'Middlewares/STM32_VISION_MODELS_PP/Src/vision_models_pp.c'),
    (Join-Path $PSScriptRoot 'test_yolov2_bounds.c'), '-lm', '-o', $visionTestExecutable)
$visionTestOriginalPath = $env:PATH
try {
    $env:PATH = (Split-Path -Parent $Compiler) + ';' + $visionTestOriginalPath
    & $Compiler @visionCompileArguments
    if ($LASTEXITCODE -ne 0) { throw "YOLOv2 host compile failed: $LASTEXITCODE" }
    & $visionTestExecutable
    if ($LASTEXITCODE -ne 0) { throw "YOLOv2 host assertions failed: $LASTEXITCODE" }
}
finally { $env:PATH = $visionTestOriginalPath }
