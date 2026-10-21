$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$bus = Get-Content -Raw -LiteralPath (Join-Path $root 'Project/Project/Air_Conditioner/Drivers/BSP/BUS/bsp_bus.c')
$busHeader = Get-Content -Raw -LiteralPath (Join-Path $root 'Project/Project/Air_Conditioner/Drivers/BSP/BUS/bsp_bus.h')
$touchBus = Get-Content -Raw -LiteralPath (Join-Path $root 'Drivers/BSP/TOUCH/ctiic.c')
$ft = Get-Content -Raw -LiteralPath (Join-Path $root 'Drivers/BSP/TOUCH/ft5206.c')
$gt = Get-Content -Raw -LiteralPath (Join-Path $root 'Drivers/BSP/TOUCH/gt9xxx.c')

if ($touchBus -match 'HAL_GPIO_(Init|WritePin)|GPIO_MODE_OUTPUT|ct_iic_send_byte\(') {
    throw 'Touch must not take PD4/PD14 away from hardware I2C2.'
}
foreach ($operation in @('read_reg8', 'write_reg8', 'read_reg16', 'write_reg16', 'probe')) {
    if ($bus -notmatch "int32_t bsp_i2c2_$operation\s*\(" -or
        $busHeader -notmatch "bsp_i2c2_$operation\s*\(") {
        throw "Shared bus operation missing: $operation"
    }
}
if ($bus -notmatch 'tx_mutex_create\(&bsp_i2c2_mutex,\s*"Shared I2C2",\s*TX_INHERIT\)' -or
    $bus -notmatch 'tx_mutex_get\(&bsp_i2c2_mutex,\s*BSP_I2C2_MUTEX_WAIT_TICKS\)' -or
    $bus -notmatch 'tx_mutex_put\(&bsp_i2c2_mutex') {
    throw 'All clients must serialize transactions with a bounded, inheriting mutex.'
}
if ($bus -match 'TX_WAIT_FOREVER|HAL_I2C_DeInit\(|tx_mutex_delete\(' -or
    $bus -notmatch 'TX_THREAD_GET_SYSTEM_STATE\(\)' -or
    $bus -notmatch 'tx_thread_identify\(\)') {
    throw 'Shared bus must reject ISR use and remain alive across client failures.'
}
if ($busHeader -notmatch '#define\s+BSP_I2C2_TIMEOUT_MS\s+\((\d+)U\)' -or
    [int]$Matches[1] -le 0 -or [int]$Matches[1] -gt 20) {
    throw 'HAL transaction timeout must be nonzero and at most 20 ms.'
}
if ($bus -notmatch 'bsp_i2c2_initialized\s*==\s*0U' -or
    $bus -notmatch 'HAL_I2C_STATE_RESET' -or
    [regex]::Matches($bus, 'MX_I2C2_Init\(\)').Count -ne 1) {
    throw 'Shared initialization must be idempotent and reuse the existing handle.'
}
foreach ($width in @('8', '16')) {
    foreach ($direction in @('read', 'write')) {
        $body = [regex]::Match($bus, "(?s)int32_t bsp_i2c2_${direction}_reg$width\([^)]*\)\s*\{([^}]+)\}").Groups[1].Value
        if ($body -notmatch "bsp_i2c2_transfer\(address, reg, I2C_MEMADD_SIZE_${width}BIT") {
            throw "$direction reg$width bypasses the shared transfer."
        }
    }
}
foreach ($halCall in @('HAL_I2C_Mem_Read', 'HAL_I2C_Mem_Write', 'HAL_I2C_IsDeviceReady')) {
    if ($bus -notmatch "(?s)$halCall\(&hi2c2,.*?BSP_I2C2_TIMEOUT_MS\)") {
        throw "$halCall must use the same handle and a finite HAL deadline."
    }
}
foreach ($source in @($ft, $gt)) {
    if ($source -match 'ct_iic_start\(|ct_iic_send_byte\(' -or
        $source -notmatch 'ct_iic_read_reg\(' -or
        $source -notmatch 'ct_iic_write_reg\(') {
        throw 'Touch register transfers must use the shared hardware bus.'
    }
}
Write-Output 'PASS: camera and touch share a bounded, serialized hardware I2C2 bus.'
