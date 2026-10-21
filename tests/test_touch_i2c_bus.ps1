$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$bus = Get-Content -Raw -LiteralPath (Join-Path $root 'Drivers/BSP/TOUCH/ctiic.c')
$header = Get-Content -Raw -LiteralPath (Join-Path $root 'Drivers/BSP/TOUCH/ctiic.h')
$ft = Get-Content -Raw -LiteralPath (Join-Path $root 'Drivers/BSP/TOUCH/ft5206.c')
$gt = Get-Content -Raw -LiteralPath (Join-Path $root 'Drivers/BSP/TOUCH/gt9xxx.c')
$ftHeader = Get-Content -Raw -LiteralPath (Join-Path $root 'Drivers/BSP/TOUCH/ft5206.h')
$gtHeader = Get-Content -Raw -LiteralPath (Join-Path $root 'Drivers/BSP/TOUCH/gt9xxx.h')

foreach ($source in @($ft, $gt)) {
    if (-not $source.Contains('#include "ctiic.h"') -or
        $source -match 'hi2c2|HAL_I2C_Mem_|ct_iic_(start|stop|send_byte|wait_ack|read_byte)\(') {
        throw 'Touch controllers must use the common register adapter, not a private bus.'
    }
    if ($source -notmatch 'if\s*\(ct_iic_init\(\)\s*!=\s*0U\)') {
        throw 'Controller initialization must propagate shared-bus initialization failure.'
    }
}
if ($ft -notmatch 'ct_iic_read_reg\(FT5206_CMD_WR, reg, CT_IIC_REG8, buf, len\)' -or
    $ft -notmatch 'ct_iic_write_reg\(FT5206_CMD_WR, reg, CT_IIC_REG8, buf, len\)' -or
    $gt -notmatch 'ct_iic_read_reg\(GT9XXX_CMD_WR, reg, CT_IIC_REG16, buf, len\)' -or
    $gt -notmatch 'ct_iic_write_reg\(GT9XXX_CMD_WR, reg, CT_IIC_REG16, buf, len\)') {
    throw 'FT must retain 8-bit registers and GT 16-bit registers with bulk lengths.'
}
if ($ftHeader -notmatch 'FT5206_CMD_WR\s+0X70' -or
    $gtHeader -notmatch 'GT9XXX_CMD_WR\s+0X28' -or
    $bus -match 'address\s*<<|HAL_GPIO_|sys_delay_us|ct_iic_(start|stop|ack|nack|send_byte|read_byte|wait_ack)\(') {
    throw 'Shifted FT/GT addresses must be preserved without GPIO bit-banging.'
}
foreach ($operation in @('read_reg8', 'write_reg8', 'read_reg16', 'write_reg16', 'probe')) {
    if ($bus -notmatch "bsp_i2c2_$operation\(") { throw "Missing touch bus route: $operation" }
}
if ($header -notmatch 'GPIO_PIN_14' -or $header -notmatch 'GPIO_PIN_4' -or
    $header -notmatch 'ct_iic_last_error\(void\)' -or
    $bus -notmatch 's_ct_iic_error\s*=\s*\(status\s*==\s*0\)') {
    throw 'Keep PD14/PD4 diagnostics and expose the last touch transaction result.'
}
if ($ft -notmatch '(?s)ft5206_rd_reg\(FT5206_REG_NUM_FINGER.*?tp_dev.sta = 0U;\s*return 0U;' -or
    $gt -notmatch '(?s)gt9xxx_rd_reg\(GT9XXX_GSTID_REG.*?tp_dev.sta = 0U;\s*return 0U;') {
    throw 'A failed scan must release the pointer and return without retry loops.'
}
if ($gt -notmatch 'points\[76\]\s*=\s*\{0\}' -or
    $gt -notmatch '(?s)gt9xxx_rd_reg\(GT9XXX_TP1_REG, points,.*?count - 1U\) \* 8U \+ 4U' -or
    $gt -notmatch '(?s)gt9xxx_rd_reg\(GT9XXX_TP1_REG.*?if \(gt9xxx_wr_reg\(GT9XXX_GSTID_REG') {
    throw 'GT must read a bounded contiguous report before clearing data-ready.'
}
Write-Output 'PASS: touch register widths, shifted addresses, bulk GT reports, and failure release are preserved.'
