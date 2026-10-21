[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$buildDirectory = Join-Path $PSScriptRoot 'build'
$previewExecutable = Join-Path $buildDirectory 'lvgl_preview.exe'

if (-not (Test-Path -LiteralPath $previewExecutable -PathType Leaf)) {
    throw 'Preview executable is missing. Open lvgl-preview.code-workspace and run its build task.'
}

Write-Host 'LVGL PC preview: initial temperature 27 C, humidity 55%.'
Write-Host '0/1: people count. D/B: dark/bright. T/Shift+T: temperature +/-1 C.'
Write-Host 'H/Shift+H: humidity +/-1%. Use the VISION / HOME buttons to switch pages; +/- adjusts the target.'
Write-Host 'C/A/S: AI/light/temperature fault and recovery. F: virtual time 1x/60x. R: reset.'
Write-Host 'Close the preview window to exit. Data is simulated; no board is accessed.'

$originalPath = $env:PATH
$previewExitCode = 0
Push-Location -LiteralPath $buildDirectory
try {
    $env:PATH = "C:/msys64/mingw64/bin;$originalPath"
    & $previewExecutable | Out-Host
    $previewExitCode = $LASTEXITCODE
}
finally {
    Pop-Location
    $env:PATH = $originalPath
}

if ($null -ne $previewExitCode -and $previewExitCode -ne 0) {
    throw "Preview exited with code $previewExitCode."
}
