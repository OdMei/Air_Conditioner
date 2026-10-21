param([string]$Compiler = 'C:/msys64/mingw64/bin/gcc.exe')
$ErrorActionPreference = 'Stop'
$runtimeTestProjectRoot = Split-Path -Parent $PSScriptRoot
$runtimeTestOutputRoot = [System.IO.Path]::GetFullPath((Join-Path $runtimeTestProjectRoot '../../tmp/code_optimization_20261010/host'))
New-Item -ItemType Directory -Path $runtimeTestOutputRoot -Force | Out-Null
$runtimeTestCases = @(
    @{ Name = 'remote_events'; Driver = 'REMOTE/remote.c'; Test = 'test_remote_events.c'; DisableRx = $false },
    @{ Name = 'dht11_faults'; Driver = 'DHT11/dht11.c'; Test = 'test_dht11_faults.c'; DisableRx = $false },
    @{ Name = 'uart_queue'; Driver = 'UART/uart.c'; Test = 'test_uart_queue.c'; DisableRx = $true }
)
$runtimeTestOriginalPath = $env:PATH
try {
    $env:PATH = (Split-Path -Parent $Compiler) + ';' + $runtimeTestOriginalPath
    foreach ($testCase in $runtimeTestCases) {
        $runtimeTestExecutable = Join-Path $runtimeTestOutputRoot ($testCase.Name + '.exe')
        $runtimeCompileArguments = @('-std=c11', '-Wall', '-Wextra', '-Werror',
            ('-I' + (Join-Path $PSScriptRoot 'stubs_runtime')),
            ('-I' + (Join-Path $runtimeTestProjectRoot ('Drivers/BSP/' + (Split-Path -Parent $testCase.Driver)))),
            (Join-Path $runtimeTestProjectRoot ('Drivers/BSP/' + $testCase.Driver)),
            (Join-Path $PSScriptRoot $testCase.Test), '-o', $runtimeTestExecutable)
        if ($testCase.DisableRx) { $runtimeCompileArguments += '-DBSP_UART_RX_DISABLE' }
        & $Compiler @runtimeCompileArguments
        if ($LASTEXITCODE -ne 0) { throw ($testCase.Name + " compile failed: $LASTEXITCODE") }
        & $runtimeTestExecutable
        if ($LASTEXITCODE -ne 0) { throw ($testCase.Name + " assertions failed: $LASTEXITCODE") }
    }
}
finally {
    $env:PATH = $runtimeTestOriginalPath
}
