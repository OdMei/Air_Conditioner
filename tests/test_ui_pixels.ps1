param(
    [string]$Compiler = 'C:/msys64/mingw64/bin/gcc.exe',
    [string]$ArmCompiler = '',
    [string]$OutputDirectory = ''
)
$ErrorActionPreference = 'Stop'
$pixelProjectRoot = Split-Path -Parent $PSScriptRoot
$pixelCore = Join-Path $pixelProjectRoot 'Project/Project/Air_Conditioner/Appli/Core'
if (-not $OutputDirectory) {
    $OutputDirectory = Join-Path $pixelProjectRoot 'build/tests/ui_pixels'
}
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$pixelOutput = [System.IO.Path]::GetFullPath($OutputDirectory)
$pixelSource = Join-Path $pixelCore 'Src/app_ui_pixels.c'
$pixelTest = Join-Path $PSScriptRoot 'test_ui_pixels.c'
$pixelInclude = '-I' + (Join-Path $pixelCore 'Inc')
$pixelExe = Join-Path $pixelOutput 'test_ui_pixels.exe'
$pixelOldPath = $env:PATH
try {
    $env:PATH = (Split-Path -Parent $Compiler) + ';' + $pixelOldPath
    & $Compiler '-std=c11' '-O2' '-Wall' '-Wextra' '-Werror' $pixelInclude $pixelSource $pixelTest '-o' $pixelExe
    if ($LASTEXITCODE -ne 0) { throw "UI pixel host compile failed: $LASTEXITCODE" }
    & $pixelExe
    if ($LASTEXITCODE -ne 0) { throw "UI pixel assertions failed: $LASTEXITCODE" }

    # Execute the production MVE branch with lane-accurate host primitives.
    # This checks its algorithm and bounds, not ARM instruction execution or
    # ThreadX context switching. The generated shim stays in build artifacts.
    $pixelShimDirectory = Join-Path $pixelOutput 'mve_host_shim'
    New-Item -ItemType Directory -Path $pixelShimDirectory -Force | Out-Null
    $pixelShim = @'
#ifndef TEST_ARM_MVE_H
#define TEST_ARM_MVE_H
#include <stdint.h>
typedef struct { uint32_t lane[4]; } uint32x4_t;
static inline uint32x4_t vdupq_n_u32(uint32_t value) {
    uint32x4_t result;
    for (unsigned i = 0; i < 4; i++) result.lane[i] = value;
    return result;
}
static inline uint32x4_t vshrq_n_u32(uint32x4_t value, unsigned shift) {
    for (unsigned i = 0; i < 4; i++) value.lane[i] >>= shift;
    return value;
}
static inline uint32x4_t vandq_u32(uint32x4_t a, uint32x4_t b) {
    for (unsigned i = 0; i < 4; i++) a.lane[i] &= b.lane[i];
    return a;
}
static inline uint32x4_t vorrq_u32(uint32x4_t a, uint32x4_t b) {
    for (unsigned i = 0; i < 4; i++) a.lane[i] |= b.lane[i];
    return a;
}
static inline void vstrhq_u32(uint16_t *dst, uint32x4_t value) {
    for (unsigned i = 0; i < 4; i++) dst[i] = (uint16_t)value.lane[i];
}
#endif
'@
    [System.IO.File]::WriteAllText((Join-Path $pixelShimDirectory 'arm_mve.h'), $pixelShim)
    $pixelMveHostExe = Join-Path $pixelOutput 'test_ui_pixels_mve_algorithm.exe'
    & $Compiler '-std=c11' '-O2' '-Wall' '-Wextra' '-Werror' '-D__ARM_FEATURE_MVE=1' '-DAPP_UI_PIXELS_EXPECT_HELIUM=1' `
        ('-I' + $pixelShimDirectory) $pixelInclude $pixelSource $pixelTest '-o' $pixelMveHostExe
    if ($LASTEXITCODE -ne 0) { throw "UI pixel MVE algorithm host compile failed: $LASTEXITCODE" }
    & $pixelMveHostExe
    if ($LASTEXITCODE -ne 0) { throw "UI pixel MVE algorithm assertions failed: $LASTEXITCODE" }
    Write-Output 'PASS: the production MVE algorithm also passes with host lane primitives; this does not execute ARM instructions.'

    if ($ArmCompiler) {
        $pixelArmFlags = @('-std=c11', '-Os', '-Wall', '-Wextra', '-Werror', '-mcpu=cortex-m55',
            '-mfpu=fpv5-d16', '-mfloat-abi=hard', '-mthumb', '-mcmse', $pixelInclude)
        $pixelMveObject = Join-Path $pixelOutput 'app_ui_pixels_helium.o'
        $pixelScalarObject = Join-Path $pixelOutput 'app_ui_pixels_scalar.o'
        & $ArmCompiler @pixelArmFlags '-c' $pixelSource '-o' $pixelMveObject
        if ($LASTEXITCODE -ne 0) { throw "UI pixel Helium ARM compile failed: $LASTEXITCODE" }
        & $ArmCompiler @pixelArmFlags '-DAPP_UI_PIXELS_DISABLE_HELIUM' '-c' $pixelSource '-o' $pixelScalarObject
        if ($LASTEXITCODE -ne 0) { throw "UI pixel scalar ARM compile failed: $LASTEXITCODE" }
        $pixelObjdump = Join-Path (Split-Path -Parent $ArmCompiler) 'arm-none-eabi-objdump.exe'
        $pixelMveAsm = (& $pixelObjdump '-d' $pixelMveObject | Out-String)
        if ($LASTEXITCODE -ne 0) { throw 'UI pixel Helium objdump failed' }
        $pixelScalarAsm = (& $pixelObjdump '-d' $pixelScalarObject | Out-String)
        if ($LASTEXITCODE -ne 0) { throw 'UI pixel scalar objdump failed' }
        [System.IO.File]::WriteAllText((Join-Path $pixelOutput 'app_ui_pixels_helium.asm'), $pixelMveAsm)
        [System.IO.File]::WriteAllText((Join-Path $pixelOutput 'app_ui_pixels_scalar.asm'), $pixelScalarAsm)
        if ($pixelMveAsm -notmatch 'vldrb' -or $pixelMveAsm -notmatch 'vstrh\.32' -or
            $pixelMveAsm -notmatch 'vshr\.u32') {
            throw 'ARM object does not contain the expected Helium conversion loop'
        }
        if ($pixelMveAsm -match '\bvctp|\bvpst' -or $pixelScalarAsm -match '\bq[0-7]\b') {
            throw 'Unexpected MVE predication or vector instructions in forced scalar backend'
        }
        Write-Output 'PASS: current Cortex-M55 flags emit unpredicated Helium loads/shifts/stores; forced scalar ARM backend compiles.'
    }
} finally {
    $env:PATH = $pixelOldPath
}
