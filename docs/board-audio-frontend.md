
## 板级接入证据（2026-10-31）

本工程使用本机正点原子 STM32N647 软件包中的 `45_Recorder` 官方例程作为板级依据。该例程的 SAI1 映射为：SAI1 Block A 发送使用 PB2（SD_A）、PC2（SCK_A）、PB0（FS_A）、PE2（MCLK_A）；SAI1 Block B 接收使用 PE3（SD_B）；DMA 使用 GPDMA1 Channel1/2。当前工程已启用 `HAL_SAI_MODULE_ENABLED`，加入对应 MSP、GPDMA 句柄、NVIC、IRQ 和 HAL 完成回调，并将 `es8388.c`、`es8388_i2c.c`、`es8388_sai.c` 纳入 Release 构建。

默认构建仍关闭 `APP_AUDIO_ES8388_SERVICE_ENABLE`，因此不会自动产生声学回路。启用该宏后，应用会启动采集服务；当前 KWS 已绑定 `app_voice_model_infer()` 弱钩子，默认安全返回“无命令”，真实 DNN 只需提供同名强实现即可替换，不需要改动音频线程。软件构建不等于实板闭环，仍需在有供电且 ST-Link 可识别目标芯片的条件下烧录并验证 ES8388 初始化、RX/TX DMA 连续运行、扬声器输出和回声路径。

## 启动音频服务

将 `APP_AUDIO_ES8388_SERVICE_ENABLE` 设为 `1` 后，应用会在 `app_run()` 初始化服务，并由 Input Thread 周期调用 `app_audio_es8388_service_poll()`。服务把 ES8388 双声道 DMA 解包为 10 ms 帧，送入 `audio_frontend`，再将处理结果复制到双声道 TX DMA；同一处理帧会送入已初始化的 KWS sink。KWS 窗口为 16 kHz、1 秒，步长 100 ms，模型入口就是 `app_voice_model_infer()`。

启用前应确认扬声器音量和麦克风距离，避免直接声学反馈；服务默认关闭就是为了让固件构建和 UI 调试不自动产生声学回路。

## 启用服务的构建命令

```powershell
python scripts/build_firmware.py --phase final --label audio_enabled_service_model_final --define APP_AUDIO_ES8388_SERVICE_ENABLE=1 --gcc E:/ST/STM32CubeCLT_1.18.0/GNU-tools-for-STM32/bin/arm-none-eabi-gcc.exe --jobs 6
```

该命令生成的镜像已经包含 SAI HAL、ES8388 DMA 驱动和应用音频服务；烧录前仍需确认目标板供电和 ST-Link 能读到目标芯片。

## 本次上板记录（2026-11-01）

ST-Link 已识别到 STM32N647（Device ID `0x486`、3.24–3.25 V、Cortex-M55）。使用 `MX25UM25645G_ATK-CNN647B_ExtMemLoader.stldr` 下载 `build/firmware/audio_enabled_service_model_final/appli.hex`，CubeProgrammer 报告应用区 `0x70100400`、981.03 KB，外部扇区 256–501 擦写完成，整段校验通过；重新连接仍可读到目标芯片。CLI 最后的自动复位返回错误码 32，串口 COM10/115200 暂未捕获运行日志，因此麦克风、扬声器声学闭环和 USB 枚举仍需在板上切到 Flash boot 后观察确认。
