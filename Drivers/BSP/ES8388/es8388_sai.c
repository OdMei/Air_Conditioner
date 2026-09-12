/**
 ****************************************************************************************************
 * @file        es8388_sai.c
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2025-01-13
 * @brief       ES8388 SAI驱动代码
 * @license     Copyright (c) 2020-2032, 广州市星翼电子科技有限公司
 ****************************************************************************************************
 * @attention
 * 
 * 实验平台:正点原子 N647开发板
 * 在线视频:www.yuanzige.com
 * 技术论坛:www.openedv.com
 * 公司网址:www.alientek.com
 * 购买地址:openedv.taobao.com
 * 
 ****************************************************************************************************
 */

#include "es8388_sai.h"

extern SAI_HandleTypeDef hsai_BlockA1;              /* SAI句柄 */
extern SAI_HandleTypeDef hsai_BlockB1;              /* SAI句柄 */
extern DMA_HandleTypeDef handle_GPDMA1_Channel1;    /* GPDMA Channel句柄 */
extern DMA_HandleTypeDef handle_GPDMA1_Channel2;    /* GPDMA Channel句柄 */

static __IO uint8_t es8388_sai_tx_dma_cplt_flag = 1;
static __IO uint8_t es8388_sai_rx_dma_cplt_flag = 1;

static void (*es8388_sai_tx_dma_cplt_user_cb)(void) = NULL;
static void (*es8388_sai_rx_dma_cplt_user_cb)(void) = NULL;

/**
 * @brief   配置ES8388 SAI时钟
 * @param   sample_rate: 采样率
 * @retval  配置结果
 * @arg     0: 配置成功
 * @arg     1: 配置失败
 */
uint8_t es8388_sai_clock_config(uint32_t sample_rate)
{
    RCC_OscInitTypeDef rcc_osc_init_struct = {0};
    RCC_PeriphCLKInitTypeDef rcc_periph_clk_init_struct = {0};

    rcc_osc_init_struct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    rcc_osc_init_struct.HSEState = RCC_HSE_ON;
    if (HAL_RCC_OscConfig(&rcc_osc_init_struct) != HAL_OK)
    {
        return 1;
    }

    rcc_osc_init_struct.OscillatorType = RCC_OSCILLATORTYPE_NONE;
    rcc_osc_init_struct.PLL1.PLLState = RCC_PLL_NONE;
    rcc_osc_init_struct.PLL2.PLLState = RCC_PLL_ON;
    rcc_osc_init_struct.PLL2.PLLSource = RCC_PLLSOURCE_HSE;
    rcc_osc_init_struct.PLL2.PLLFractional = 0;
    rcc_osc_init_struct.PLL2.PLLM = 6;
    if ((sample_rate == 11025) || (sample_rate == 22050) || (sample_rate == 44100))
    {
        rcc_osc_init_struct.PLL2.PLLN = 192;
        rcc_osc_init_struct.PLL2.PLLP1 = 4;
        rcc_osc_init_struct.PLL2.PLLP2 = 2;
    }
    else if ((sample_rate == 8000) || (sample_rate == 16000) || (sample_rate == 32000) || (sample_rate == 48000) || (sample_rate == 96000) || (sample_rate == 192000))
    {
        rcc_osc_init_struct.PLL2.PLLN = 172;
        rcc_osc_init_struct.PLL2.PLLP1 = 7;
        rcc_osc_init_struct.PLL2.PLLP2 = 4;
    }
    else
    {
        return 1;
    }
    rcc_osc_init_struct.PLL3.PLLState = RCC_PLL_NONE;
    rcc_osc_init_struct.PLL4.PLLState = RCC_PLL_NONE;
    if (HAL_RCC_OscConfig(&rcc_osc_init_struct) != HAL_OK)
    {
        return 1;
    }

    rcc_periph_clk_init_struct.PeriphClockSelection = RCC_PERIPHCLK_SAI1;
    rcc_periph_clk_init_struct.Sai1ClockSelection = RCC_SAI1CLKSOURCE_IC7;
    rcc_periph_clk_init_struct.ICSelection[RCC_IC7].ClockSelection = RCC_ICCLKSOURCE_PLL2;
    if ((sample_rate == 11025) || (sample_rate == 22050) || (sample_rate == 44100))
    {
        rcc_periph_clk_init_struct.ICSelection[RCC_IC7].ClockDivider = 17;
    }
    else if ((sample_rate == 8000) || (sample_rate == 16000) || (sample_rate == 32000) || (sample_rate == 48000) || (sample_rate == 96000) || (sample_rate == 192000))
    {
        rcc_periph_clk_init_struct.ICSelection[RCC_IC7].ClockDivider = 1;
    }
    else
    {
        return 1;
    }
    if (HAL_RCCEx_PeriphCLKConfig(&rcc_periph_clk_init_struct) != HAL_OK)
    {
        return 1;
    }
  
    return 0;
}

/**
 * @brief   初始化ES8388 SAI发送
 * @param   channels: 通道数
 * @param   sample_rate: 采样率
 * @param   bits_per_sample: 采样位宽
 * @retval  初始化结果
 * @arg     0: 初始化成功
 * @arg     1: 初始化失败
 */
uint8_t es8388_sai_init_tx(uint16_t channels, uint32_t sample_rate, uint16_t bits_per_sample)
{
    uint32_t i;
    uint32_t sai_mono_stereo_mode;
    uint32_t sai_framelength_factor;
    uint32_t sai_slot_number;
    uint32_t sai_slot_active;
    uint32_t sai_audio_frequency;
    uint32_t sai_datasize;
    uint32_t sai_framelength;
    uint32_t sai_slot_size;
    uint32_t dma_src_data_width;
    uint32_t dma_dest_data_width;

    struct channels_config {
        uint32_t channels;
        uint32_t sai_mono_stereo_mode;
        uint32_t sai_framelength_factor;
        uint32_t sai_slot_number;
        uint32_t sai_slot_active;
    } channels_config_tbl[] = {
        {1, SAI_MONOMODE, 1, 1, SAI_SLOTACTIVE_0},
        {2, SAI_STEREOMODE, 2, 2, SAI_SLOTACTIVE_0 | SAI_SLOTACTIVE_1},
    };

    struct sample_rate_config {
        uint32_t sample_rate;
        uint32_t sai_audio_frequency;
    } sample_rate_config_tbl[] = {
        {192000, SAI_AUDIO_FREQUENCY_192K},
        {96000, SAI_AUDIO_FREQUENCY_96K},
        {48000, SAI_AUDIO_FREQUENCY_48K},
        {44100, SAI_AUDIO_FREQUENCY_44K},
        {32000, SAI_AUDIO_FREQUENCY_32K},
        {22050, SAI_AUDIO_FREQUENCY_22K},
        {16000, SAI_AUDIO_FREQUENCY_16K},
        {11025, SAI_AUDIO_FREQUENCY_11K},
        {8000, SAI_AUDIO_FREQUENCY_8K},
    };

    struct bits_per_sample_config {
        uint16_t bits_per_sample;
        uint32_t sai_datasize;
        uint32_t sai_framelength;
        uint32_t sai_slot_size;
        uint32_t dma_src_data_width;
        uint32_t dma_dest_data_width;
    } bits_per_sample_config_tbl[] = {
        {16, SAI_DATASIZE_16, 16, SAI_SLOTSIZE_16B, DMA_SRC_DATAWIDTH_HALFWORD, DMA_DEST_DATAWIDTH_HALFWORD},
        {24, SAI_DATASIZE_24, 32, SAI_SLOTSIZE_32B, DMA_SRC_DATAWIDTH_WORD, DMA_DEST_DATAWIDTH_WORD},
        {32, SAI_DATASIZE_32, 32, SAI_SLOTSIZE_32B, DMA_SRC_DATAWIDTH_WORD, DMA_DEST_DATAWIDTH_WORD},
    };

    for (i = 0; i < (sizeof(channels_config_tbl) / sizeof(channels_config_tbl[0])); i++)
    {
        if (channels_config_tbl[i].channels == channels)
        {
            sai_mono_stereo_mode = channels_config_tbl[i].sai_mono_stereo_mode;
            sai_framelength_factor = channels_config_tbl[i].sai_framelength_factor;
            sai_slot_number = channels_config_tbl[i].sai_slot_number;
            sai_slot_active = channels_config_tbl[i].sai_slot_active;
            break;
        }
    }
    if (i == (sizeof(channels_config_tbl) / sizeof(channels_config_tbl[0])))
    {
        return 1;
    }

    for (i = 0; i < (sizeof(sample_rate_config_tbl) / sizeof(sample_rate_config_tbl[0])); i++)
    {
        if (sample_rate_config_tbl[i].sample_rate == sample_rate)
        {
            sai_audio_frequency = sample_rate_config_tbl[i].sai_audio_frequency;
            break;
        }
    }
    if (i == (sizeof(sample_rate_config_tbl) / sizeof(sample_rate_config_tbl[0])))
    {
        return 1;
    }

    for (i = 0; i < (sizeof(bits_per_sample_config_tbl) / sizeof(bits_per_sample_config_tbl[0])); i++)
    {
        if (bits_per_sample_config_tbl[i].bits_per_sample == bits_per_sample)
        {
            sai_datasize = bits_per_sample_config_tbl[i].sai_datasize;
            sai_framelength = bits_per_sample_config_tbl[i].sai_framelength;
            sai_slot_size = bits_per_sample_config_tbl[i].sai_slot_size;
            dma_src_data_width = bits_per_sample_config_tbl[i].dma_src_data_width;
            dma_dest_data_width = bits_per_sample_config_tbl[i].dma_dest_data_width;
            break;
        }
    }
    if (i == (sizeof(bits_per_sample_config_tbl) / sizeof(bits_per_sample_config_tbl[0])))
    {
        return 1;
    }

    hsai_BlockA1.Instance = SAI1_Block_A;
    HAL_SAI_DeInit(&hsai_BlockA1);

    hsai_BlockA1.Instance = SAI1_Block_A;
    hsai_BlockA1.Init.Protocol = SAI_FREE_PROTOCOL;
    hsai_BlockA1.Init.AudioMode = SAI_MODEMASTER_TX;
    hsai_BlockA1.Init.DataSize = sai_datasize;
    hsai_BlockA1.Init.FirstBit = SAI_FIRSTBIT_MSB;
    hsai_BlockA1.Init.ClockStrobing = SAI_CLOCKSTROBING_FALLINGEDGE;
    hsai_BlockA1.Init.Synchro = SAI_ASYNCHRONOUS;
    hsai_BlockA1.Init.OutputDrive = SAI_OUTPUTDRIVE_ENABLE;
    hsai_BlockA1.Init.NoDivider = SAI_MASTERDIVIDER_ENABLE;
    hsai_BlockA1.Init.FIFOThreshold = SAI_FIFOTHRESHOLD_1QF;
    hsai_BlockA1.Init.AudioFrequency = sai_audio_frequency;
    hsai_BlockA1.Init.SynchroExt = SAI_SYNCEXT_DISABLE;
    hsai_BlockA1.Init.MckOutput = SAI_MCK_OUTPUT_ENABLE;
    hsai_BlockA1.Init.MonoStereoMode = sai_mono_stereo_mode;
    hsai_BlockA1.Init.CompandingMode = SAI_NOCOMPANDING;
    hsai_BlockA1.Init.TriState = SAI_OUTPUT_NOTRELEASED;
    hsai_BlockA1.Init.PdmInit.Activation = DISABLE;
    hsai_BlockA1.Init.PdmInit.MicPairsNbr = 1;
    hsai_BlockA1.Init.PdmInit.ClockEnable = SAI_PDM_CLOCK1_ENABLE;
    hsai_BlockA1.FrameInit.FrameLength = sai_framelength_factor * sai_framelength;
    hsai_BlockA1.FrameInit.ActiveFrameLength = hsai_BlockA1.FrameInit.FrameLength >> 1;
    hsai_BlockA1.FrameInit.FSDefinition = SAI_FS_CHANNEL_IDENTIFICATION;
    hsai_BlockA1.FrameInit.FSPolarity = SAI_FS_ACTIVE_LOW;
    hsai_BlockA1.FrameInit.FSOffset = SAI_FS_BEFOREFIRSTBIT;
    hsai_BlockA1.SlotInit.FirstBitOffset = 0;
    hsai_BlockA1.SlotInit.SlotSize = sai_slot_size;
    hsai_BlockA1.SlotInit.SlotNumber = sai_slot_number;
    hsai_BlockA1.SlotInit.SlotActive = sai_slot_active;
    HAL_SAI_Init(&hsai_BlockA1);

    HAL_DMA_DeInit(hsai_BlockA1.hdmatx);

    handle_GPDMA1_Channel1.Instance = GPDMA1_Channel1;
    HAL_DMA_DeInit(&handle_GPDMA1_Channel1);

    handle_GPDMA1_Channel1.Instance = GPDMA1_Channel1;
    handle_GPDMA1_Channel1.Init.Request = GPDMA1_REQUEST_SAI1_A;
    handle_GPDMA1_Channel1.Init.BlkHWRequest = DMA_BREQ_SINGLE_BURST;
    handle_GPDMA1_Channel1.Init.Direction = DMA_MEMORY_TO_PERIPH;
    handle_GPDMA1_Channel1.Init.SrcInc = DMA_SINC_INCREMENTED;
    handle_GPDMA1_Channel1.Init.DestInc = DMA_DINC_FIXED;
    handle_GPDMA1_Channel1.Init.SrcDataWidth = dma_src_data_width;
    handle_GPDMA1_Channel1.Init.DestDataWidth = dma_dest_data_width;
    handle_GPDMA1_Channel1.Init.Priority = DMA_LOW_PRIORITY_LOW_WEIGHT;
    handle_GPDMA1_Channel1.Init.SrcBurstLength = 1;
    handle_GPDMA1_Channel1.Init.DestBurstLength = 1;
    handle_GPDMA1_Channel1.Init.TransferAllocatedPort = DMA_SRC_ALLOCATED_PORT0 | DMA_DEST_ALLOCATED_PORT0;
    handle_GPDMA1_Channel1.Init.TransferEventMode = DMA_TCEM_BLOCK_TRANSFER;
    handle_GPDMA1_Channel1.Init.Mode = DMA_NORMAL;
    HAL_DMA_Init(&handle_GPDMA1_Channel1);
    __HAL_LINKDMA(&hsai_BlockA1, hdmatx, handle_GPDMA1_Channel1);
    HAL_DMA_ConfigChannelAttributes(&handle_GPDMA1_Channel1, DMA_CHANNEL_PRIV | DMA_CHANNEL_SEC | DMA_CHANNEL_SRC_SEC | DMA_CHANNEL_DEST_SEC);

    return 0;
}

/**
 * @brief   初始化ES8388 SAI接收
 * @param   channels: 通道数
 * @param   sample_rate: 采样率
 * @param   bits_per_sample: 采样位宽
 * @retval  初始化结果
 * @arg     0: 初始化成功
 * @arg     1: 初始化失败
 */
uint8_t es8388_sai_init_rx(uint16_t channels, uint32_t sample_rate, uint16_t bits_per_sample)
{
    uint32_t i;
    uint32_t sai_mono_stereo_mode;
    uint32_t sai_framelength_factor;
    uint32_t sai_slot_number;
    uint32_t sai_slot_active;
    uint32_t sai_audio_frequency;
    uint32_t sai_datasize;
    uint32_t sai_framelength;
    uint32_t dma_src_data_width;
    uint32_t dma_dest_data_width;

    struct channels_config {
        uint32_t channels;
        uint32_t sai_mono_stereo_mode;
        uint32_t sai_framelength_factor;
        uint32_t sai_slot_number;
        uint32_t sai_slot_active;
    } channels_config_tbl[] = {
        {1, SAI_MONOMODE, 1, 1, SAI_SLOTACTIVE_0},
        {2, SAI_STEREOMODE, 2, 2, SAI_SLOTACTIVE_0 | SAI_SLOTACTIVE_1},
    };

    struct sample_rate_config {
        uint32_t sample_rate;
        uint32_t sai_audio_frequency;
    } sample_rate_config_tbl[] = {
        {192000, SAI_AUDIO_FREQUENCY_192K},
        {96000, SAI_AUDIO_FREQUENCY_96K},
        {48000, SAI_AUDIO_FREQUENCY_48K},
        {44100, SAI_AUDIO_FREQUENCY_44K},
        {32000, SAI_AUDIO_FREQUENCY_32K},
        {22050, SAI_AUDIO_FREQUENCY_22K},
        {16000, SAI_AUDIO_FREQUENCY_16K},
        {11025, SAI_AUDIO_FREQUENCY_11K},
        {8000, SAI_AUDIO_FREQUENCY_8K},
    };

    struct bits_per_sample_config {
        uint16_t bits_per_sample;
        uint32_t sai_datasize;
        uint32_t sai_framelength;
        uint32_t dma_src_data_width;
        uint32_t dma_dest_data_width;
    } bits_per_sample_config_tbl[] = {
        {16, SAI_DATASIZE_16, 16, DMA_SRC_DATAWIDTH_HALFWORD, DMA_DEST_DATAWIDTH_HALFWORD},
    };

    for (i = 0; i < (sizeof(channels_config_tbl) / sizeof(channels_config_tbl[0])); i++)
    {
        if (channels_config_tbl[i].channels == channels)
        {
            sai_mono_stereo_mode = channels_config_tbl[i].sai_mono_stereo_mode;
            sai_framelength_factor = channels_config_tbl[i].sai_framelength_factor;
            sai_slot_number = channels_config_tbl[i].sai_slot_number;
            sai_slot_active = channels_config_tbl[i].sai_slot_active;
            break;
        }
    }
    if (i == (sizeof(channels_config_tbl) / sizeof(channels_config_tbl[0])))
    {
        return 1;
    }

    for (i = 0; i < (sizeof(sample_rate_config_tbl) / sizeof(sample_rate_config_tbl[0])); i++)
    {
        if (sample_rate_config_tbl[i].sample_rate == sample_rate)
        {
            sai_audio_frequency = sample_rate_config_tbl[i].sai_audio_frequency;
            break;
        }
    }
    if (i == (sizeof(sample_rate_config_tbl) / sizeof(sample_rate_config_tbl[0])))
    {
        return 1;
    }

    for (i = 0; i < (sizeof(bits_per_sample_config_tbl) / sizeof(bits_per_sample_config_tbl[0])); i++)
    {
        if (bits_per_sample_config_tbl[i].bits_per_sample == bits_per_sample)
        {
            sai_datasize = bits_per_sample_config_tbl[i].sai_datasize;
            sai_framelength = bits_per_sample_config_tbl[i].sai_framelength;
            dma_src_data_width = bits_per_sample_config_tbl[i].dma_src_data_width;
            dma_dest_data_width = bits_per_sample_config_tbl[i].dma_dest_data_width;
            break;
        }
    }
    if (i == (sizeof(bits_per_sample_config_tbl) / sizeof(bits_per_sample_config_tbl[0])))
    {
        return 1;
    }

    hsai_BlockB1.Instance = SAI1_Block_B;
    HAL_SAI_DeInit(&hsai_BlockB1);

    hsai_BlockB1.Instance = SAI1_Block_B;
    hsai_BlockB1.Init.Protocol = SAI_FREE_PROTOCOL;
    hsai_BlockB1.Init.AudioMode = SAI_MODESLAVE_RX;
    hsai_BlockB1.Init.DataSize = sai_datasize;
    hsai_BlockB1.Init.FirstBit = SAI_FIRSTBIT_MSB;
    hsai_BlockB1.Init.ClockStrobing = SAI_CLOCKSTROBING_FALLINGEDGE;
    hsai_BlockB1.Init.Synchro = SAI_SYNCHRONOUS;
    hsai_BlockB1.Init.OutputDrive = SAI_OUTPUTDRIVE_ENABLE;
    hsai_BlockB1.Init.NoDivider = SAI_MASTERDIVIDER_ENABLE;
    hsai_BlockB1.Init.FIFOThreshold = SAI_FIFOTHRESHOLD_1QF;
    hsai_BlockB1.Init.AudioFrequency = sai_audio_frequency;
    hsai_BlockB1.Init.SynchroExt = SAI_SYNCEXT_DISABLE;
    hsai_BlockB1.Init.MckOutput = SAI_MCK_OUTPUT_ENABLE;
    hsai_BlockB1.Init.MonoStereoMode = sai_mono_stereo_mode;
    hsai_BlockB1.Init.CompandingMode = SAI_NOCOMPANDING;
    hsai_BlockB1.Init.TriState = SAI_OUTPUT_NOTRELEASED;
    hsai_BlockB1.Init.PdmInit.Activation = DISABLE;
    hsai_BlockB1.Init.PdmInit.MicPairsNbr = 1;
    hsai_BlockB1.Init.PdmInit.ClockEnable = SAI_PDM_CLOCK1_ENABLE;
    hsai_BlockB1.FrameInit.FrameLength = sai_framelength_factor * sai_framelength;
    hsai_BlockB1.FrameInit.ActiveFrameLength = hsai_BlockA1.FrameInit.FrameLength >> 1;
    hsai_BlockB1.FrameInit.FSDefinition = SAI_FS_CHANNEL_IDENTIFICATION;
    hsai_BlockB1.FrameInit.FSPolarity = SAI_FS_ACTIVE_LOW;
    hsai_BlockB1.FrameInit.FSOffset = SAI_FS_BEFOREFIRSTBIT;
    hsai_BlockB1.SlotInit.FirstBitOffset = 0;
    hsai_BlockB1.SlotInit.SlotSize = SAI_SLOTSIZE_DATASIZE;
    hsai_BlockB1.SlotInit.SlotNumber = sai_slot_number;
    hsai_BlockB1.SlotInit.SlotActive = sai_slot_active;
    HAL_SAI_Init(&hsai_BlockB1);

    HAL_DMA_DeInit(hsai_BlockB1.hdmarx);

    handle_GPDMA1_Channel2.Instance = GPDMA1_Channel2;
    HAL_DMA_DeInit(&handle_GPDMA1_Channel2);

    handle_GPDMA1_Channel2.Instance = GPDMA1_Channel2;
    handle_GPDMA1_Channel2.Init.Request = GPDMA1_REQUEST_SAI1_B;
    handle_GPDMA1_Channel2.Init.BlkHWRequest = DMA_BREQ_SINGLE_BURST;
    handle_GPDMA1_Channel2.Init.Direction = DMA_PERIPH_TO_MEMORY;
    handle_GPDMA1_Channel2.Init.SrcInc = DMA_SINC_FIXED;
    handle_GPDMA1_Channel2.Init.DestInc = DMA_DINC_INCREMENTED;
    handle_GPDMA1_Channel2.Init.SrcDataWidth = dma_src_data_width;
    handle_GPDMA1_Channel2.Init.DestDataWidth = dma_dest_data_width;
    handle_GPDMA1_Channel2.Init.Priority = DMA_LOW_PRIORITY_LOW_WEIGHT;
    handle_GPDMA1_Channel2.Init.SrcBurstLength = 1;
    handle_GPDMA1_Channel2.Init.DestBurstLength = 1;
    handle_GPDMA1_Channel2.Init.TransferAllocatedPort = DMA_SRC_ALLOCATED_PORT0 | DMA_DEST_ALLOCATED_PORT0;
    handle_GPDMA1_Channel2.Init.TransferEventMode = DMA_TCEM_BLOCK_TRANSFER;
    handle_GPDMA1_Channel2.Init.Mode = DMA_NORMAL;
    HAL_DMA_Init(&handle_GPDMA1_Channel2);
    __HAL_LINKDMA(&hsai_BlockB1, hdmarx, handle_GPDMA1_Channel2);
    HAL_DMA_ConfigChannelAttributes(&handle_GPDMA1_Channel2, DMA_CHANNEL_PRIV | DMA_CHANNEL_SEC | DMA_CHANNEL_SRC_SEC | DMA_CHANNEL_DEST_SEC);

    return 0;
}

/**
 * @brief   ES8388 SAI 注册DMA发送完成回调
 * @param   cb: 回调
 * @retval  无
 */
void es8388_sai_tx_dma_cplt_user_cb_register(void (*cb)(void))
{
    es8388_sai_tx_dma_cplt_user_cb = cb;
}

/**
 * @brief   ES8388 SAI 注册DMA接收完成回调
 * @param   cb: 回调
 * @retval  无
 */
void es8388_sai_rx_dma_cplt_user_cb_register(void (*cb)(void))
{
    es8388_sai_rx_dma_cplt_user_cb = cb;
}

/**
 * @brief   ES8388 SAI DMA发送
 * @param   buffer: 数据
 * @param   data_amount: 数据量
 * @retval  发送结果
 * @arg     0: 发送成功
 * @arg     1: 发送失败
 */
uint8_t es8388_sai_tx_dma(uint8_t *buffer, uint32_t data_amount)
{
    es8388_sai_tx_dma_cplt_flag = 0;
    if (HAL_SAI_Transmit_DMA(&hsai_BlockA1, buffer, data_amount) != HAL_OK)
    {
        return 1;
    }

    return 0;
}

/**
 * @brief   ES8388 SAI DMA接收
 * @param   buffer: 数据
 * @param   data_amount: 数据量
 * @retval  接收结果
 * @arg     0: 接收成功
 * @arg     1: 接收失败
 */
uint8_t es8388_sai_rx_dma(uint8_t *buffer, uint32_t data_amount)
{
    es8388_sai_rx_dma_cplt_flag = 0;
    if (HAL_SAI_Receive_DMA(&hsai_BlockB1, buffer, data_amount) != HAL_OK)
    {
        return 1;
    }

    return 0;
}

/**
 * @brief   ES8388 SAI DMA发送完成处理函数
 * @param   无
 * @retval  无
 */
void es8388_sai_tx_dma_cplt_handler(void)
{
    es8388_sai_tx_dma_cplt_flag = 1;
    if (es8388_sai_tx_dma_cplt_user_cb != NULL)
    {
        es8388_sai_tx_dma_cplt_user_cb();
    }
}

/**
 * @brief   ES8388 SAI DMA接收完成处理函数
 * @param   无
 * @retval  无
 */
void es8388_sai_rx_dma_cplt_handler(void)
{
    es8388_sai_rx_dma_cplt_flag = 1;
    if (es8388_sai_rx_dma_cplt_user_cb != NULL)
    {
        es8388_sai_rx_dma_cplt_user_cb();
    }
}

/**
 * @brief   ES8388 SAI DMA发送是否忙
 * @param   无
 * @retval  结果
 * @arg     0: 否
 * @arg     1: 是
 */
uint8_t es8388_sai_tx_dma_is_busy(void)
{
    return es8388_sai_tx_dma_cplt_flag ? 0 : 1;
}

/**
 * @brief   ES8388 SAI DMA接收是否忙
 * @param   无
 * @retval  结果
 * @arg     0: 否
 * @arg     1: 是
 */
uint8_t es8388_sai_rx_dma_is_busy(void)
{
    return es8388_sai_rx_dma_cplt_flag ? 0 : 1;
}
