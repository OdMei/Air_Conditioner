$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path -Parent $PSScriptRoot
$source = Get-Content -Raw -LiteralPath (Join-Path $projectRoot 'Project/Project/Air_Conditioner/Appli/Core/Src/main.c')

$requiredPins = @(
    'HAL_GPIO_ConfigPinAttributes\(GPIOD,GPIO_PIN_4,GPIO_PIN_SEC\|GPIO_PIN_NPRIV\)',
    'HAL_GPIO_ConfigPinAttributes\(GPIOD,GPIO_PIN_14,GPIO_PIN_SEC\|GPIO_PIN_NPRIV\)',
    'HAL_GPIO_ConfigPinAttributes\(GPIOD,GPIO_PIN_10,GPIO_PIN_SEC\|GPIO_PIN_NPRIV\)',
    'HAL_GPIO_ConfigPinAttributes\(GPIOB,GPIO_PIN_3,GPIO_PIN_SEC\|GPIO_PIN_NPRIV\)'
)

foreach ($requiredPin in $requiredPins) {
    if ($source -notmatch $requiredPin) {
        throw "Touch pin security configuration missing: $requiredPin"
    }
}

$msp = Get-Content -Raw -LiteralPath (Join-Path $projectRoot 'Project/Project/Air_Conditioner/Appli/Core/Src/stm32n6xx_hal_msp.c')
$touchBus = Get-Content -Raw -LiteralPath (Join-Path $projectRoot 'Drivers/BSP/TOUCH/ctiic.c')
if ($msp -notmatch '(?s)GPIO_InitStruct.Pin = GPIO_PIN_4\|GPIO_PIN_14;\s*GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;.*?GPIO_InitStruct.Alternate = GPIO_AF4_I2C2;' -or
    $touchBus -match 'HAL_GPIO_(Init|WritePin|ConfigPinAttributes)\(') {
    throw 'PD4/PD14 must retain hardware I2C2 AF4 and their existing security attributes.'
}
Write-Output 'PASS: secure touch pins remain accessible and PD4/PD14 retain hardware I2C2 AF4.'
