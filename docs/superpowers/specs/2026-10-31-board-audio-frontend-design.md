# 板载实时音频前端与 ES8388 闭环设计

日期：2026-10-31

## 目标

在 STM32N647 工程中建立可运行的板载实时音频路径：双麦克风输入、扬声器参考回采、实时前端处理、语音控制和扬声器输出。处理链为可替换模块，支持后续接入真实 AEC、波束成形、去混响、DoA、DNN 噪声抑制和 60 dB AGC 实现；首阶段先完成帧级合同、DMA 生命周期、确定性基线和硬件闭环验证。

## 当前工程事实

- `Drivers/BSP/ES8388` 已有 I2C 编解码器和 SAI DMA 收发驱动。
- `app_voice_audio_es8388` 已有默认关闭的 16 kHz、16-bit 接收适配器。
- `app_voice_kws` 使用 16 kHz PCM 窗口并复用 `app_climate_apply_action()`。
- `stm32n6xx_hal_conf.h` 当前没有启用 `HAL_SAI_MODULE_ENABLED`，主程序也没有完整的 SAI GPIO/MSP 初始化。
- ThreadX 已是应用运行时基础，音频处理不得放在 DMA ISR 中。

## 架构

音频系统分为四层：

1. **设备层**：ES8388 I2C 配置、SAI RX/TX DMA、双缓冲和 DMA 完成回调。设备层只搬运数据和报告状态。
2. **帧调度层**：ThreadX 音频任务从无锁就绪队列取出 10 ms 帧，绑定采样时间戳和通道信息，处理溢出、欠载和错误恢复。
3. **前端策略层**：`audio_frontend` 以不透明上下文和函数表暴露处理接口。每帧依次执行校准、AEC、波束成形、去混响、噪声抑制、AGC 和 DoA；每个策略都能旁路并报告能力和状态。
4. **业务层**：将增强后的单声道帧送入 KWS；将播放数据送入 TX 环形缓冲。业务层只依赖音频帧接口，不直接调用 HAL。

数据流：

```text
ES8388 RX (mic0, mic1) + TX reference
       -> 10 ms frame queue
       -> calibration
       -> AEC
       -> beamformer
       -> dereverb
       -> DNN NS / deterministic NS fallback
       -> AGC (<= 60 dB) + limiter
       -> DoA / source localization
       -> KWS + climate actions
       -> ES8388 TX / speaker
```

## 音频合同

- 采样率：16,000 Hz。
- 样本格式：有符号 16-bit PCM。
- 帧时长：10 ms，160 个采样点。
- 输入通道：至少两个麦克风通道；播放参考单独提供，缺失时标记 `REFERENCE_UNAVAILABLE`。
- 输出：一路增强语音、DoA 角度（-90 到 +90 度）、置信度和处理状态。
- 音频线程每帧必须在下一帧到达前完成；超时只允许丢弃当前帧并累计诊断计数，不得阻塞 DMA。
- DMA ISR 只记录完成缓冲区和错误标志，不调用 KWS、DSP、I2C 或内存分配。

## 算法策略

- **AEC**：采用可替换的自适应滤波接口。首版提供受限阶数 NLMS 基线；没有有效 TX 参考时旁路并上报状态。双讲检测用于冻结更新，避免把近端语音消除。
- **波束成形**：双麦延时求和，接口包含通道增益、样本延时和目标方向；未校准时使用零延时并标记降级。
- **去混响**：首版使用短时能量衰减基线；接口预留 WPE/多通道模型后端。
- **噪声抑制**：接口接受每频带增益或时域掩码。未接 DNN 时使用噪声底估计和软门限；DNN 回调必须在音频线程外部提供确定的输出大小和超时边界。
- **AGC**：目标 RMS、攻击/释放时间、峰值限幅；最大增益固定为 60 dB，任何配置值都需在初始化时钳制。
- **DoA**：首版使用双麦 GCC-PHAT，输出角度、置信度和有效标志；信号能量不足或互相关峰不可靠时输出不可用。

## 错误和降级

设备初始化失败时系统保持业务线程可运行，并报告 `DEVICE_ERROR`；音频队列满时丢弃最旧帧并累计 `overrun_count`；算法策略失败时只旁路该策略，保留后续处理；所有错误通过状态快照和诊断计数暴露。关闭顺序为停止 TX、停止 RX、注销 DMA 回调、释放策略上下文。

## 文件边界

首阶段新增：

- `Project/Project/Air_Conditioner/Appli/Core/Inc/audio_frame.h`
- `Project/Project/Air_Conditioner/Appli/Core/Inc/audio_frontend.h`
- `Project/Project/Air_Conditioner/Appli/Core/Src/audio_frontend.c`
- `Project/Project/Air_Conditioner/Appli/Core/Inc/audio_frontend_config.h`
- `Project/Project/Air_Conditioner/Appli/Core/Inc/app_audio_pipeline.h`
- `Project/Project/Air_Conditioner/Appli/Core/Src/app_audio_pipeline.c`
- `tests/test_audio_frontend.c`
- `tests/test_audio_pipeline.c`
- `docs/board-audio-frontend.md`

修改：

- `Project/Project/Air_Conditioner/Appli/Core/Inc/stm32n6xx_hal_conf.h`：启用 SAI 模块，仅在板级初始化合同满足后生效。
- `Project/Project/Air_Conditioner/Appli/Core/Src/app_voice_audio_es8388.c`：从单麦 KWS 适配扩展为帧队列生产者和 TX/RX 生命周期管理。
- `Project/Project/Air_Conditioner/STM32CubeIDE/Appli/Release/Application/User/Core/subdir.mk`：加入新增源文件。

真实 DNN 权重、UAC 描述符和 DeepSeek 网络传输不属于本阶段实现，但 `audio_frontend` 和业务事件接口会保留稳定接入点。

## 验收

1. native 测试验证帧边界、旁路行为、AEC 无参考降级、AGC 60 dB 上限、DoA 无效输入和队列溢出。
2. 工程编译无新增错误和警告。
3. 启用硬件后，RX DMA 连续运行，TX DMA 可回放，停止顺序无死锁。
4. 麦克风到扬声器闭环在无参考和有参考两种模式下均能运行；无参考时诊断明确显示 AEC 旁路。
5. KWS 仍能触发既有空调动作，音频算法不直接修改空调状态。
6. 诊断快照可读取处理耗时、队列深度、溢出次数、AEC/NS/AGC/DoA 状态。

## 未决硬件输入

板级 SAI1 引脚、DMA 通道、时钟源、ES8388 麦克风数量和扬声器回采连接必须根据实际开发板原理图确认。未确认前保持硬件开关关闭，native 测试和软件帧链路可独立运行。
