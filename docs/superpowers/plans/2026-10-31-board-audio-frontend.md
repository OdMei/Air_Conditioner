# 板载音频前端实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在 STM32N647 上建立可测试的 16 kHz 实时音频前端，并为 ES8388 闭环、KWS、UAC 和联网语音接口提供稳定边界。

**Architecture:** 先实现无 HAL 依赖的固定帧 DSP 核心，再由音频管线适配 DMA 和 KWS。前端以配置和状态快照管理 AEC、双麦延时求和、去混响、噪声抑制、AGC 与 GCC-PHAT DoA；各算法失败时只旁路自身并累计诊断计数。

**Tech Stack:** C11/C11 host tests、STM32 HAL/SAI、ThreadX、ES8388、现有 `app_voice_kws`。

---

### Task 1: 音频帧合同

**Files:**
- Create: `Project/Project/Air_Conditioner/Appli/Core/Inc/audio_frame.h`
- Test: `tests/test_audio_frame.c`
- Test runner: `tests/test_audio_frame.ps1`

- [ ] **Step 1: 写失败测试**

测试构造 160 点双麦帧，确认采样率、通道数、参考有效位和输出缓冲区布局；传入空指针的校验函数必须返回错误。

- [ ] **Step 2: 运行测试确认失败**

运行 `powershell -ExecutionPolicy Bypass -File tests/test_audio_frame.ps1`，预期因为 `audio_frame.h` 和校验函数不存在而失败。

- [ ] **Step 3: 实现最小帧合同**

定义 `AUDIO_FRAME_SAMPLE_RATE_HZ=16000`、`AUDIO_FRAME_SAMPLES=160`、`AUDIO_FRAME_CHANNELS=2`，以及 `audio_frame_t`、`audio_frame_validate()`。函数只检查指针、采样率和样本数，不执行 DSP。

- [ ] **Step 4: 运行测试确认通过**

同一命令应以退出码 0 结束，且 `-Wall -Wextra -Werror` 无警告。

### Task 2: 前端策略与状态快照

**Files:**
- Create: `Project/Project/Air_Conditioner/Appli/Core/Inc/audio_frontend_config.h`
- Create: `Project/Project/Air_Conditioner/Appli/Core/Inc/audio_frontend.h`
- Create: `Project/Project/Air_Conditioner/Appli/Core/Src/audio_frontend.c`
- Create: `tests/test_audio_frontend.c`
- Create: `tests/test_audio_frontend.ps1`

- [ ] **Step 1: 写失败测试**

覆盖以下行为：无参考时 AEC 状态为旁路；双麦同相输入经过波束成形后保持幅度；静音帧不产生 AGC 爆增；配置的最大增益大于 60 dB 时被钳制；无能量输入时 DoA 无效；有固定样本延迟的双麦输入返回对应方向和有效置信度；处理统计计数递增。

- [ ] **Step 2: 运行测试确认失败**

运行 `powershell -ExecutionPolicy Bypass -File tests/test_audio_frontend.ps1`，预期因为前端头文件和实现不存在而失败。

- [ ] **Step 3: 实现固定点前端**

在 `audio_frontend_t` 中保存噪声底、AGC 增益、AEC 权重、上一帧输出和诊断计数。实现 `audio_frontend_init()`、`audio_frontend_process()`、`audio_frontend_get_status()`：

- AEC 使用有界 16 阶 NLMS，`reference_valid=0` 时逐样本旁路；双讲能量较高时冻结权重更新。
- 波束成形使用配置的有符号整数延时和两个通道平均，所有索引先做边界检查。
- 去混响使用上一帧输出反馈的固定点衰减，系数限制在 0 到 255。
- 噪声抑制先用噪声底估计和软门限；暴露 `audio_frontend_set_dnn_callback()`，回调返回失败时保留基线结果并增加 `dnn_fallback_count`。
- AGC 用平方和的移位近似 RMS，目标幅度、攻击/释放和最大增益均在配置时钳制；最大增益换算为 60 dB 的 Q8 上限。
- DoA 在有限延迟窗口内计算双麦互相关，选择绝对峰值并输出角度、置信度和有效位；能量低于门限时输出不可用。

- [ ] **Step 4: 运行测试确认通过**

同一 runner 应通过所有断言；使用 `-fsanitize=address,undefined` 的 host 编译再运行一次，确认无越界和未定义整数行为。

### Task 3: 音频管线与 KWS 边界

**Files:**
- Create: `Project/Project/Air_Conditioner/Appli/Core/Inc/app_audio_pipeline.h`
- Create: `Project/Project/Air_Conditioner/Appli/Core/Src/app_audio_pipeline.c`
- Create: `tests/test_audio_pipeline.c`
- Create: `tests/test_audio_pipeline.ps1`

- [ ] **Step 1: 写失败测试**

测试双缓冲提交、满队列丢弃计数、处理后调用 KWS 回调、播放输出回调，以及停止后拒绝新帧。

- [ ] **Step 2: 运行测试确认失败**

运行 `powershell -ExecutionPolicy Bypass -File tests/test_audio_pipeline.ps1`，预期因为管线接口不存在而失败。

- [ ] **Step 3: 实现非阻塞管线**

实现固定容量 2 的帧队列；`app_audio_pipeline_dma_complete()` 只发布缓冲区索引，`app_audio_pipeline_poll()` 在普通线程中处理前端、调用可选 KWS sink 和 TX sink。所有共享就绪标志使用 `volatile`，队列满时丢弃最旧帧并增加计数，绝不在回调中执行 DSP。

- [ ] **Step 4: 运行测试确认通过**

runner 通过并用 ThreadSanitizer 可用时再运行一次；确认回调上下文合同在头文件注释中明确。

### Task 4: 工程接入与诊断文档

**Files:**
- Modify: `Project/Project/Air_Conditioner/STM32CubeIDE/Appli/Release/Application/User/Core/subdir.mk`
- Modify: `Project/Project/Air_Conditioner/STM32CubeIDE/Appli/Debug/Application/User/Core/subdir.mk`
- Modify: `simulator/CMakeLists.txt`
- Create: `docs/board-audio-frontend.md`

- [ ] **Step 1: 添加源文件清单**

将 `audio_frontend.c` 和 `app_audio_pipeline.c` 加入 Debug、Release 和 host 可见的源文件列表，保持现有路径风格。

- [ ] **Step 2: 添加使用文档**

记录初始化顺序、10 ms 帧格式、DMA ISR 限制、状态快照字段、DNN 回调契约和 ES8388 硬件开关。明确 SAI GPIO、DMA 通道和回采连接未确认前不得打开硬件宏。

- [ ] **Step 3: 运行验证**

依次运行四个音频 runner、现有语音 runner 和 `python tests/test_build_firmware.py`；若完整固件工具链不可用，记录实际失败命令和缺失工具，不把 host 测试替代固件编译结论。

### Task 5: 安全审查

- [ ] **Step 1: 运行嵌入式 C 安全清单**

逐项检查编译、逻辑、内存、并发、硬件交互、资源生命周期、边界和影响分析；发现问题从 Task 1 的验证重新开始。

- [ ] **Step 2: 汇总证据**

使用 `git diff --check`、全部测试输出和构建输出形成阶段报告，明确哪些功能已经实现、哪些仍等待真实 DNN 模型、板级 SAI 映射、UAC 类描述符和 DeepSeek 网络凭据。
