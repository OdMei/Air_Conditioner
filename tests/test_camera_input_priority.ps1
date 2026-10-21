$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$source = Get-Content -Raw -LiteralPath (Join-Path $root 'Project/Project/Air_Conditioner/Appli/Core/Src/app.c')

$port = Get-Content -Raw -LiteralPath (Join-Path $root 'Middlewares/ThreadX/ports/cortex_m55/gnu/inc/tx_port.h')
$maxMatch = [regex]::Match($port, '#define\s+TX_MAX_PRIORITIES\s+(\d+)')
if (-not $maxMatch.Success) { throw 'ThreadX priority range is missing.' }
$priorityLimit = [int]$maxMatch.Groups[1].Value
$priorityValues = @{}
foreach ($threadName in @('INPUT', 'DP', 'ISP', 'NN', 'PP', 'CAMERA')) {
    $definition = [regex]::Match($source, "#define\s+APP_THREAD_${threadName}_PRIORITY\s+\((?<value>[^)]+)\)")
    if (-not $definition.Success) { throw "$threadName priority is missing." }
    $expression = $definition.Groups['value'].Value
    if ($expression -match '^\s*(\d+)U?\s*$') {
        $priorityValues[$threadName] = [int]$Matches[1]
    } elseif ($expression -match '^\s*TX_MAX_PRIORITIES\s*-\s*(\d+)U?\s*$') {
        $priorityValues[$threadName] = $priorityLimit - [int]$Matches[1]
    } else { throw "Unsupported priority expression: $expression" }
}
if ($priorityValues.INPUT -ge $priorityValues.DP -or
    $priorityValues.DP -ge $priorityValues.CAMERA) {
    throw 'Touch input must preempt UI rendering, and optional camera initialization must remain lower priority.'
}
if ($source -notmatch 'tx_semaphore_ceiling_put\(&isp_semaphore,\s*1U\)' -or
    $source -notmatch '(?s)static VOID isp_thread_entry\(ULONG id\)\s*\{.*?app_camera_isp_update\(\);.*?tx_thread_sleep\(1U\)') {
    throw 'ISP must coalesce pending VSYNC notifications and yield time to inference/postprocessing.'
}
Write-Output 'PASS: input preempts UI, camera remains optional, and ISP cannot accumulate unbounded work.'
