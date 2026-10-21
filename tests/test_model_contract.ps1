param(
    [string]$Python = 'D:/STM32-Toolchain/Apps/STEdgeAI/4.0/Utilities/windows/python.exe',
    [string]$ToolOverlay = 'D:/Learning/AC-N647/tmp/model_upgrade_20261012_v10/tool_overlay',
    [string]$OutputDirectory = 'D:/Learning/AC-N647/tmp/model_upgrade_20261012_v10/model-contract'
)
$ErrorActionPreference = 'Stop'
$taskPreviousPythonPath = $env:PYTHONPATH
$taskPreviousTensorflowLog = $env:TF_CPP_MIN_LOG_LEVEL
try {
    if (Test-Path -LiteralPath $ToolOverlay) {
        $env:PYTHONPATH = $ToolOverlay
    }
    $env:TF_CPP_MIN_LOG_LEVEL = '2'
    & $Python (Join-Path $PSScriptRoot 'test_model_contract.py') --output-dir $OutputDirectory
    if ($LASTEXITCODE -ne 0) { throw "Real TFLite model contract failed: $LASTEXITCODE" }
}
finally {
    $env:PYTHONPATH = $taskPreviousPythonPath
    $env:TF_CPP_MIN_LOG_LEVEL = $taskPreviousTensorflowLog
}
