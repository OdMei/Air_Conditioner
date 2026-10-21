param([string]$Compiler = 'C:/msys64/mingw64/bin/gcc.exe')
$ErrorActionPreference = 'Stop'
$queueTestRoot = Split-Path -Parent $PSScriptRoot
$queueTestCore = Join-Path $queueTestRoot 'Project/Project/Air_Conditioner/Appli/Core'
$queueTestConfig = Get-Content -Raw -LiteralPath (Join-Path $queueTestCore 'Inc/app_config.h')
if ($queueTestConfig -notmatch '#define BQUEUE_MAX_BUFFERS\s+2\b') {
    throw 'Queue host fixture must match the production two-buffer capacity.'
}
$queueTestOutput = [System.IO.Path]::GetFullPath((Join-Path $queueTestRoot '../../tmp/code_optimization_20261010/host'))
New-Item -ItemType Directory -Path $queueTestOutput -Force | Out-Null
$queueTestExe = Join-Path $queueTestOutput 'test_app_bqueue.exe'
$queueTestOldPath = $env:PATH
try {
    $env:PATH = (Split-Path -Parent $Compiler) + ';' + $queueTestOldPath
    # Only app_config's hardware/model includes are bypassed; capacity above
    # is checked against production. Compile the actual queue implementation.
    & $Compiler '-std=c11' '-Wall' '-Wextra' '-Werror' '-D__APP_CONFIG_H' '-DBQUEUE_MAX_BUFFERS=2' `
        ('-I' + (Join-Path $PSScriptRoot 'stubs_bqueue')) `
        ('-I' + (Join-Path $queueTestCore 'Inc')) `
        (Join-Path $queueTestCore 'Src/app_bqueue.c') `
        (Join-Path $PSScriptRoot 'test_app_bqueue.c') '-o' $queueTestExe
    if ($LASTEXITCODE -ne 0) { throw "bqueue host compile failed: $LASTEXITCODE" }
    & $queueTestExe
    if ($LASTEXITCODE -ne 0) { throw "bqueue host assertions failed: $LASTEXITCODE" }
} finally { $env:PATH = $queueTestOldPath }
