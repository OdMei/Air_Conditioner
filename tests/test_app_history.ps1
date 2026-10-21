param([string]$Compiler = 'C:/msys64/mingw64/bin/gcc.exe')
$ErrorActionPreference = 'Stop'
$historyTestRoot = Split-Path -Parent $PSScriptRoot
$historyTestCore = Join-Path $historyTestRoot 'Project/Project/Air_Conditioner/Appli/Core'
$historyTestOutput = [System.IO.Path]::GetFullPath((Join-Path $historyTestRoot '../../tmp/v9_20261011/history'))
New-Item -ItemType Directory -Path $historyTestOutput -Force | Out-Null
$historyTestExe = Join-Path $historyTestOutput 'test_app_history.exe'
$historyTestOldPath = $env:PATH
try {
    $env:PATH = (Split-Path -Parent $Compiler) + ';' + $historyTestOldPath
    & $Compiler '-std=c11' '-O2' '-Wall' '-Wextra' '-Werror' `
        ('-I' + (Join-Path $historyTestCore 'Inc')) `
        (Join-Path $historyTestCore 'Src/app_history.c') `
        (Join-Path $PSScriptRoot 'test_app_history.c') '-o' $historyTestExe
    if ($LASTEXITCODE -ne 0) { throw "History host compile failed: $LASTEXITCODE" }
    & $historyTestExe
    if ($LASTEXITCODE -ne 0) { throw "History host assertions failed: $LASTEXITCODE" }
} finally { $env:PATH = $historyTestOldPath }
