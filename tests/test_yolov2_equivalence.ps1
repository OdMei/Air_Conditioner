param([string]$Compiler = 'C:/msys64/mingw64/bin/gcc.exe')
$ErrorActionPreference = 'Stop'
$yoloEquivalenceProjectRoot = Split-Path -Parent $PSScriptRoot
$yoloEquivalenceOutputRoot = [System.IO.Path]::GetFullPath((Join-Path $yoloEquivalenceProjectRoot '../../tmp/performance_20261010/host'))
New-Item -ItemType Directory -Path $yoloEquivalenceOutputRoot -Force | Out-Null
$yoloEquivalenceExecutable = Join-Path $yoloEquivalenceOutputRoot 'yolov2_equivalence.exe'
$yoloEquivalenceReferenceObject = Join-Path $yoloEquivalenceOutputRoot 'yolov2_v6_reference.o'
$yoloEquivalenceCommonArguments = @('-std=c11', '-O2', '-Wall', '-Wextra', '-Werror', '-Wno-error=sign-compare',
    ('-I' + (Join-Path $PSScriptRoot 'stubs_vision')),
    ('-I' + (Join-Path $yoloEquivalenceProjectRoot 'Middlewares/STM32_VISION_MODELS_PP/Inc')),
    ('-I' + (Join-Path $yoloEquivalenceProjectRoot 'Middlewares/STM32_VISION_MODELS_PP/Src')))
# Compile the frozen v6 implementation in a separate translation unit.  This
# compares the real production pipeline against an independent old algorithm,
# rather than generating expected results with the optimized implementation.
$yoloEquivalenceReferenceSymbols = @('yolov2_nms_comparator', 'yolov2_pp_nmsFiltering_centroid',
    'yolov2_pp_scoreFiltering_centroid', 'yolov2_pp_getNNBoxes_centroid',
    'od_yolov2_pp_reset', 'od_yolov2_pp_process')
$yoloEquivalenceReferenceArguments = @($yoloEquivalenceCommonArguments)
foreach ($yoloEquivalenceSymbol in $yoloEquivalenceReferenceSymbols) {
    $yoloEquivalenceReferenceArguments += '-D' + $yoloEquivalenceSymbol + '=v6_' + $yoloEquivalenceSymbol
}
$yoloEquivalenceReferenceArguments += @('-c', (Join-Path $PSScriptRoot 'fixtures/yolov2_v6_reference.c'), '-o', $yoloEquivalenceReferenceObject)
$yoloEquivalenceCompileArguments = @($yoloEquivalenceCommonArguments) + @(
    (Join-Path $yoloEquivalenceProjectRoot 'Middlewares/STM32_VISION_MODELS_PP/Src/od_pp_yolov2.c'),
    (Join-Path $yoloEquivalenceProjectRoot 'Middlewares/STM32_VISION_MODELS_PP/Src/vision_models_pp.c'),
    (Join-Path $PSScriptRoot 'test_yolov2_equivalence.c'), $yoloEquivalenceReferenceObject,
    '-lm', '-o', $yoloEquivalenceExecutable)
$yoloEquivalenceOriginalPath = $env:PATH
try {
    $env:PATH = (Split-Path -Parent $Compiler) + ';' + $yoloEquivalenceOriginalPath
    & $Compiler @yoloEquivalenceReferenceArguments
    if ($LASTEXITCODE -ne 0) { throw "YOLOv2 v6 reference compile failed: $LASTEXITCODE" }
    & $Compiler @yoloEquivalenceCompileArguments
    if ($LASTEXITCODE -ne 0) { throw "YOLOv2 equivalence compile failed: $LASTEXITCODE" }
    & $yoloEquivalenceExecutable
    if ($LASTEXITCODE -ne 0) { throw "YOLOv2 equivalence assertions failed: $LASTEXITCODE" }
}
finally { $env:PATH = $yoloEquivalenceOriginalPath }
