from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HAL_CONF = ROOT / "Project/Project/Air_Conditioner/Appli/Core/Inc/stm32n6xx_hal_conf.h"
MSP = ROOT / "Project/Project/Air_Conditioner/Appli/Core/Src/stm32n6xx_hal_msp.c"
MAIN = ROOT / "Project/Project/Air_Conditioner/Appli/Core/Src/main.c"


def test_official_es8388_sai_contract_is_enabled():
    hal_conf = HAL_CONF.read_text(encoding="utf-8")
    msp = MSP.read_text(encoding="utf-8")
    main = MAIN.read_text(encoding="utf-8")
    assert "#define HAL_SAI_MODULE_ENABLED" in hal_conf
    for token in ("SAI1_Block_A", "SAI1_Block_B", "GPIO_PIN_2",
                  "GPIO_PIN_0", "GPIO_PIN_3", "GPIO_AF6_SAI1",
                  "GPDMA1_REQUEST_SAI1_A", "GPDMA1_REQUEST_SAI1_B"):
        assert token in msp
    assert "DMA_HandleTypeDef handle_GPDMA1_Channel1" in main
    assert "DMA_HandleTypeDef handle_GPDMA1_Channel2" in main
    assert "HAL_SAI_TxCpltCallback" in main
    assert "HAL_SAI_RxCpltCallback" in main
