# 离线语音控制接入

语音控制通过 `app_voice_control` 接入现有空调动作枚举。离线识别芯片只需要输出关键词 ID 或 UTF-8 短语和置信度，再调用 `app_voice_apply_result()`，得到的动作与触摸、按键、红外共用 `app_climate_apply_action()`。

当前关键词合同（中英文共用同一 ID）：

| ID | 关键词 | 动作 |
|---:|---|---|
| 1 | 开机／关机 | 电源切换 |
| 2–3 | 升温／降温 | 设定温度 ±1°C |
| 4 | 制冷 | 模式切换 |
| 5 | 风速增加 | 下一风档 |
| 6–10 | 一档～五档 | 指定风档 |
| 11–15 | 强劲、摆风、灯光、睡眠、节能 | 对应开关切换 |

置信度低于 60% 的结果会被丢弃，未知短语不会改变状态。这样可以直接替换具体的麦克风和离线 ASR 芯片，而不会把芯片驱动耦合进控制状态机。STM32N6 工程清单已包含该模块；接入硬件时，在输入线程读取 ASR 结果后调用映射函数，再把返回的 `app_climate_action_t` 送入现有动作入口即可。

短语解析同时接受中文和英文，例如“升高温度”/“temperature up”、“风速三档”/“fan speed three”、“强劲”/“turbo”。Edge Impulse 模型建议使用语言无关的类别名（`power`、`temp_up`、`temp_down`、`fan_1`…`fan_5`、`turbo`、`swing`、`lamp`、`sleep`、`eco`、`noise`、`unknown`），中文和英文样本共同训练后映射到相同的 ID。

ST/Arm 的 STM32N6 参考实现使用板载麦克风、音频预处理和 Edge Impulse Keyword Spotting 模型；其推理结果在置信度超过 60% 时输出关键词。可将该模型的分类标签映射到本表 ID，再调用 `app_voice_apply_result()`。参考：[SDS-STM32N6 KeywordSpotting](https://github.com/Arm-Examples/SDS-STM32N6)。

`app_voice_kws` 已提供 16 kHz、16-bit PCM 的窗口适配：窗口 1 秒、步长 100 ms。SAI/麦克风 DMA 完成回调将采样块传给 `app_voice_kws_feed()`；Edge Impulse 生成的分类器通过 `app_voice_kws_infer_cb_t` 返回关键词 ID 和置信度，模块随后自动调用空调动作状态机。模型应至少训练目标关键词和 `noise`/`unknown` 类，并把分类结果映射到 1–15 的关键词 ID。

模型导出的字符串标签可通过 `app_voice_ei_label_to_keyword()` 统一映射；该映射同时接受语言无关标签（如 `temp_up`、`fan_3`）和中英文标签。

工程新增了默认关闭的 `app_voice_audio_es8388` 端口适配器。确认 SAI1 的 GPIO、MSP 初始化和 ES8388 硬件连接后，将 `APP_VOICE_ES8388_ENABLE` 设为 `1`，启动函数会以 16 kHz、16-bit、双声道 DMA 采样，并把 1600 个样本块送入 KWS 窗口。未确认硬件前保持关闭，避免误配置 DMA 或引脚。

DMA 完成回调只记录就绪缓冲区；输入线程需要周期性调用 `app_voice_audio_es8388_poll()` 才会执行 KWS 和启动下一块 DMA，避免在中断上下文运行模型。

另有 STM32N6 NPU 连续英文 ASR 参考实现 CynipsNet v1.0，但其模型约 7.3 MB、激活内存约 4.2 MB，且语言仅为英文；它适合做英文自由语音输入研究，不直接作为本空调的中英双语命令模型。参考：[stm32n6-speech-recognition](https://github.com/fotonik-sk/stm32n6-speech-recognition)。

训练数据可按 `voice_dataset/<label>/*.wav` 存放，标签使用 `power`、`temp_up`、`fan_1`…`eco`、`noise`、`unknown`。运行 `python scripts/prepare_voice_dataset.py voice_dataset` 会检查每个文件是否为单声道 16 kHz/16-bit PCM，并生成 `build/voice-dataset/manifest.csv` 和 `labels.json`。中文和英文录音放在同一个意图目录中，例如中文“升温”和英文“temperature up”都放入 `temp_up/`。
