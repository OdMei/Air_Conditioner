$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$project = Join-Path $root 'Project/Project/Air_Conditioner/Appli/Core'
$lcd = Get-Content -Raw -LiteralPath (Join-Path $project 'Src/app_lcd.c')
$ui = Get-Content -Raw -LiteralPath (Join-Path $project 'Src/app_ui.c')


$capacity = [regex]::Match($ui, 'static char s_debug_values_text\[(\d+)\]')
if (-not $capacity.Success -or [int]$capacity.Groups[1].Value -lt 512) {
    throw 'The debug panel buffer must hold both LTDC layer diagnostics.'
}
if ($ui -notmatch 'LCD BG %08lX P%lu C%08lX' -or
    $ui -notmatch 'LCD FG %08lX P%lu C%08lX') {
    throw 'The debug panel must show both live LTDC layer addresses.'
}
if ($lcd -notmatch 'LTDC_Layer1->CFBAR' -or
    $lcd -notmatch 'LTDC_Layer2->CFBAR') {
    throw 'The LCD diagnostics must read both LTDC CFBAR registers.'
}
if ($lcd -notmatch 'LTDC_Layer1->CFBLR' -or
    $lcd -notmatch 'LTDC_Layer2->CFBLR') {
    throw 'The LCD diagnostics must read both LTDC line pitches.'
}

Write-Output 'PASS: both LTDC layer contracts are visible in the debug panel.'
