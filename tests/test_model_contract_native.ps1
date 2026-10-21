param([string]$Compiler = 'C:/msys64/mingw64/bin/gcc.exe')
$ErrorActionPreference = 'Stop'
$modelContractRoot = Split-Path -Parent $PSScriptRoot
$modelContractCore = Join-Path $modelContractRoot 'Project/Project/Air_Conditioner/Appli/Core'
$modelContractOutput = [System.IO.Path]::GetFullPath((Join-Path $modelContractRoot '../../tmp/model_upgrade_20261012_v10/native_contract'))
New-Item -ItemType Directory -Path $modelContractOutput -Force | Out-Null
$modelContractOldPath = $env:PATH
try {
    $env:PATH = (Split-Path -Parent $Compiler) + ';' + $modelContractOldPath
    foreach ($modelContractOptimization in @('O0', 'O2')) {
        $modelContractExe = Join-Path $modelContractOutput ('test_model_contract_' + $modelContractOptimization + '.exe')
        & $Compiler '-std=c11' ('-' + $modelContractOptimization) '-Wall' '-Wextra' '-Wpedantic' '-Werror' '-fstack-usage' `
            ('-I' + (Join-Path $modelContractCore 'Inc')) `
            (Join-Path $modelContractCore 'Src/app_model_contract.c') `
            (Join-Path $PSScriptRoot 'test_model_contract_native.c') '-lm' '-o' $modelContractExe
        if ($LASTEXITCODE -ne 0) { throw "Model contract $modelContractOptimization compile failed: $LASTEXITCODE" }
        & $modelContractExe
        if ($LASTEXITCODE -ne 0) { throw "Model contract $modelContractOptimization assertions failed: $LASTEXITCODE" }
    }
} finally { $env:PATH = $modelContractOldPath }
