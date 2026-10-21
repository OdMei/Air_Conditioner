param([string]$Compiler = 'C:/msys64/mingw64/bin/gcc.exe',
      [string]$ModelOutputDirectory = '')
$ErrorActionPreference = 'Stop'
$yolov8TestRoot = Split-Path -Parent $PSScriptRoot
$yolov8Core = Join-Path $yolov8TestRoot 'Project/Project/Air_Conditioner/Appli/Core'
$yolov8Vendor = Join-Path $yolov8TestRoot 'Middlewares/STM32_VISION_MODELS_PP'
$yolov8Output = [System.IO.Path]::GetFullPath((Join-Path $yolov8TestRoot '../../tmp/model_upgrade_20261012_v10/native'))
$yolov8FixtureHeaders = Join-Path $yolov8Output 'headers'
New-Item -ItemType Directory -Path $yolov8FixtureHeaders -Force | Out-Null
# Compile the actual wrapper/header with a HAL-free isolated model selector.
# The real model shape comes from the production postprocess/model headers;
# this fixture supplies only the algorithm selector and candidate thresholds.
Copy-Item -LiteralPath (Join-Path $yolov8Core 'Inc/app_postprocess.h') -Destination $yolov8FixtureHeaders
$yolov8Config = @'
#ifndef HOST_APP_CONFIG_H
#define HOST_APP_CONFIG_H
#include "postprocess_conf.h"
#define POSTPROCESS_TYPE POSTPROCESS_OD_YOLO_V8_UF
#define AI_OBJDETECT_YOLOV8_PP_CONF_THRESHOLD 0.5f
#define AI_OBJDETECT_YOLOV8_PP_IOU_THRESHOLD 0.45f
#define AI_OBJDETECT_YOLOV8_PP_MAX_BOXES_LIMIT 10
#endif
'@
[System.IO.File]::WriteAllText((Join-Path $yolov8FixtureHeaders 'app_config.h'), $yolov8Config, [System.Text.Encoding]::ASCII)
$yolov8Common = @('-std=c11', '-O2', '-Wall', '-Wextra', '-Werror', '-Wno-error=sign-compare',
    ('-I' + $yolov8FixtureHeaders),
    ('-I' + (Join-Path $PSScriptRoot 'stubs_vision')),
    ('-I' + (Join-Path $yolov8Core 'Inc')),
    ('-I' + (Join-Path $yolov8Vendor 'Inc')),
    ('-I' + (Join-Path $yolov8Vendor 'Src')))
$yolov8ReferenceSymbols = @('yolov8_nms_comparator', 'yolov8_pp_nmsFiltering_centroid',
    'yolov8_pp_scoreFiltering_centroid', 'yolov8_pp_getNNBoxes_centroid',
    'yolov8_pp_getNNBoxes_centroid_int8', 'od_yolov8_pp_reset',
    'od_yolov8_pp_process', 'od_yolov8_pp_process_int8')
$yolov8OriginalPath = $env:PATH
try {
    $env:PATH = (Split-Path -Parent $Compiler) + ';' + $yolov8OriginalPath
    foreach ($yolov8Mode in @('scalar', 'parallel_gather')) {
        $yolov8ModeFlags = @()
        # Exercise the vendor four-lane candidate-reader control flow with
        # scalar gather helpers on the host. This is not an ARM instruction
        # performance test; board compilation validates the MVE intrinsic path.
        if ($yolov8Mode -eq 'parallel_gather') { $yolov8ModeFlags += '-DAI_OD_YOLOV8_PP_MVEF_OPTIM' }
        $yolov8CurrentObject = Join-Path $yolov8Output ('yolov8_' + $yolov8Mode + '.o')
        $yolov8ReferenceObject = Join-Path $yolov8Output ('yolov8_reference_' + $yolov8Mode + '.o')
        $yolov8CurrentArgs = @($yolov8Common) + $yolov8ModeFlags + @(
            '-Dvision_models_box_iou=counted_optimized_iou', '-c',
            (Join-Path $yolov8Vendor 'Src/od_pp_yolov8.c'), '-o', $yolov8CurrentObject)
        & $Compiler @yolov8CurrentArgs
        if ($LASTEXITCODE -ne 0) { throw "YOLOv8 $yolov8Mode compile failed: $LASTEXITCODE" }
        $yolov8ReferenceArgs = @($yolov8Common) + $yolov8ModeFlags + @('-Dvision_models_box_iou=counted_reference_iou')
        foreach ($yolov8Symbol in $yolov8ReferenceSymbols) {
            $yolov8ReferenceArgs += '-D' + $yolov8Symbol + '=reference_' + $yolov8Symbol
        }
        $yolov8ReferenceArgs += @('-c', (Join-Path $PSScriptRoot 'fixtures/yolov8_original_reference.c'), '-o', $yolov8ReferenceObject)
        & $Compiler @yolov8ReferenceArgs
        if ($LASTEXITCODE -ne 0) { throw "YOLOv8 frozen reference compile failed: $LASTEXITCODE" }
        $yolov8Exe = Join-Path $yolov8Output ('test_yolov8_' + $yolov8Mode + '.exe')
        $yolov8LinkArgs = @($yolov8Common) + @(
            (Join-Path $yolov8Core 'Src/app_postprocess.c'),
            (Join-Path $yolov8Vendor 'Src/vision_models_pp.c'),
            (Join-Path $PSScriptRoot 'test_yolov8_postprocess.c'),
            $yolov8CurrentObject, $yolov8ReferenceObject, '-lm', '-o', $yolov8Exe)
        & $Compiler @yolov8LinkArgs
        if ($LASTEXITCODE -ne 0) { throw "YOLOv8 wrapper compile failed: $LASTEXITCODE" }
        if ($ModelOutputDirectory) { & $yolov8Exe $ModelOutputDirectory }
        else { & $yolov8Exe }
        if ($LASTEXITCODE -ne 0) { throw "YOLOv8 $yolov8Mode numerical/guard assertions failed: $LASTEXITCODE" }
    }

    # The unified parameter alias must still compile and initialize YOLOv2.
    # Existing V2 bounds/equivalence suites remain unmodified and are run by
    # the project's usual engineering checks.
    $yolov2Config = $yolov8Config.Replace('POSTPROCESS_OD_YOLO_V8_UF', 'POSTPROCESS_OD_YOLO_V2_UF')
    $yolov2Config = $yolov2Config.Replace('YOLOV8_PP_CONF_THRESHOLD', 'YOLOV2_PP_CONF_THRESHOLD')
    $yolov2Config = $yolov2Config.Replace('YOLOV8_PP_IOU_THRESHOLD', 'YOLOV2_PP_IOU_THRESHOLD')
    $yolov2Config = $yolov2Config.Replace('YOLOV8_PP_MAX_BOXES_LIMIT', 'YOLOV2_PP_MAX_BOXES_LIMIT')
    [System.IO.File]::WriteAllText((Join-Path $yolov8FixtureHeaders 'app_config.h'), $yolov2Config, [System.Text.Encoding]::ASCII)
    $yolov2AliasSource = @'
#include <assert.h>
#include <stdio.h>
#include "app_postprocess.h"
int main(void) {
    app_postprocess_params_t params;
    static float raw[7 * 7 * 5 * 6];
    void *inputs[] = {raw};
    od_pp_out_t output = {0};
    assert(sizeof(params) == sizeof(yolov2_pp_static_param_t));
    assert(app_postprocess_init(&params) == 0);
    assert(params.nb_classes == 1 && params.nb_anchors == 5);
    for (int index = 0; index < 7 * 7 * 5; ++index) raw[index * 6 + 4] = -10;
    assert(app_postprocess_run(inputs, 1, &output, &params) == 0);
    assert(output.nb_detect == 0);
    puts("Unified postprocess parameter alias: YOLOv2 compile/init/run passed");
    return 0;
}
'@
    $yolov2AliasFile = Join-Path $yolov8Output 'test_yolov2_alias.c'
    [System.IO.File]::WriteAllText($yolov2AliasFile, $yolov2AliasSource, [System.Text.Encoding]::ASCII)
    $yolov2AliasExe = Join-Path $yolov8Output 'test_yolov2_alias.exe'
    $yolov2AliasArgs = @($yolov8Common) + @(
        (Join-Path $yolov8Core 'Src/app_postprocess.c'),
        (Join-Path $yolov8Vendor 'Src/od_pp_yolov2.c'),
        (Join-Path $yolov8Vendor 'Src/vision_models_pp.c'), $yolov2AliasFile, '-lm', '-o', $yolov2AliasExe)
    & $Compiler @yolov2AliasArgs
    if ($LASTEXITCODE -ne 0) { throw "YOLOv2 parameter alias compile failed: $LASTEXITCODE" }
    & $yolov2AliasExe
    if ($LASTEXITCODE -ne 0) { throw "YOLOv2 parameter alias assertions failed: $LASTEXITCODE" }
} finally { $env:PATH = $yolov8OriginalPath }
