param([string]$Compiler = 'C:/msys64/mingw64/bin/gcc.exe')
$ErrorActionPreference = 'Stop'
$geometryTestRoot = Split-Path -Parent $PSScriptRoot
$geometryTestOutput = [System.IO.Path]::GetFullPath((Join-Path $geometryTestRoot '../../tmp/code_optimization_20261010/host'))
New-Item -ItemType Directory -Path $geometryTestOutput -Force | Out-Null
$geometryTestExe = Join-Path $geometryTestOutput 'test_camera_geometry.exe'
$geometryCompilerArgs = @('-std=c11', '-Wall', '-Wextra', '-Werror',
    ('-I' + (Join-Path $geometryTestRoot 'Project/Project/Air_Conditioner/Appli/Core/Inc')),
    (Join-Path $PSScriptRoot 'test_camera_geometry.c'), '-lm', '-o', $geometryTestExe)
$geometryOldPath = $env:PATH
try {
    $env:PATH = (Split-Path -Parent $Compiler) + ';' + $geometryOldPath
    & $Compiler @geometryCompilerArgs
    if ($LASTEXITCODE -ne 0) { throw "Geometry compile failed: $LASTEXITCODE" }
    & $geometryTestExe
    if ($LASTEXITCODE -ne 0) { throw "Geometry assertions failed: $LASTEXITCODE" }
} finally { $env:PATH = $geometryOldPath }
