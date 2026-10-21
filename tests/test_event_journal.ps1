param([string]$Compiler = 'C:/msys64/mingw64/bin/gcc.exe')
$ErrorActionPreference = 'Stop'
$journalTestRoot = Split-Path -Parent $PSScriptRoot
$journalTestCore = Join-Path $journalTestRoot 'Project/Project/Air_Conditioner/Appli/Core'
$journalTestOutput = [System.IO.Path]::GetFullPath((Join-Path $journalTestRoot '../../tmp/lvgl_20261012_v9_5/journal'))
New-Item -ItemType Directory -Path $journalTestOutput -Force | Out-Null
$journalTestExe = Join-Path $journalTestOutput 'test_event_journal.exe'
$journalTestOldPath = $env:PATH
try {
    $env:PATH = (Split-Path -Parent $Compiler) + ';' + $journalTestOldPath
    & $Compiler '-std=c11' '-O2' '-Wall' '-Wextra' '-Wpedantic' '-Werror' '-fstack-usage' `
        ('-I' + (Join-Path $journalTestCore 'Inc')) `
        (Join-Path $journalTestCore 'Src/app_event_journal.c') `
        (Join-Path $PSScriptRoot 'test_event_journal.c') '-o' $journalTestExe
    if ($LASTEXITCODE -ne 0) { throw "Event journal host compile failed: $LASTEXITCODE" }
    & $journalTestExe
    if ($LASTEXITCODE -ne 0) { throw "Event journal assertions failed: $LASTEXITCODE" }
} finally { $env:PATH = $journalTestOldPath }
