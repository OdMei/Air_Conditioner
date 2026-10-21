# 当前人体检测模型核查与升级建议

日期：2026-10-12。本次为源码与本地工具核查、官方模型资料对照；没有替换模型、修改固件或烧录。

## 核查结论

V9.5 仍使用 **Tiny YOLO v2、224×224、INT8 人体单类模型**。

| 项目 | 当前值 | 源码证据 |
|---|---|---|
| 生成来源 | `tiny_yolo_v2_224_int8`，STEdgeAI 2.0 | `Model/network.c:22-41` |
| 输入 | NHWC `[1,224,224,3]`，UINT8 RGB，150528 字节 | `Model/network.c:291-294,365-380` |
| 输出 | NHWC `[1,7,7,30]`，FLOAT32，5880 字节 | `Model/network.c:1436-1460` |
| 类别 | `person`，1 类 | `Appli/Core/Inc/app_config.h:37-43` |
| 网格和 anchors | 7×7、5 anchors，245 个候选 | `Appli/Core/Inc/postprocess_conf.h:17-30` |
| 后处理 | confidence 0.6、NMS IoU 0.3、最多 10 框 | `Appli/Core/Inc/app_config.h:45-49` |
| 执行 | NPU 主体计算，输出软件反量化；输入输出双缓冲 | `Model/network.c:179-218,249-274`；`Appli/Core/Src/app.c:186-189,752-768` |

INT8 指量化网络，并不表示应用输入输出均为有符号 INT8；当前输入是 UINT8，输出暴露为浮点。

V7 已优化单类别 softmax 和 NMS，摄像头已经直接生成推理输入；V9.3 稳框只改变绘制坐标，不能补上模型没有检测到的人；V9.5 统一显示与推理裁剪区域，仍未改变网络权重。此前约 30 Hz 的实板反馈是更新率，不是检测准确率。

## 推荐下一步

以 **YOLOv8n 人体模型 INT8** 为首个替换候选：先验证 256×256 完整链路，再与 320×320 做同场景对照。目标是评估远处人体、部分遮挡、坐姿和多人重叠的漏检与误检，不能根据模型名称预先宣称改善。

ST 模型库提供 COCO-Person 量化候选，第一轮可以使用其预训练模型，避免先建立自己的训练流程。[ST YOLOv8n 资料](https://github.com/STMicroelectronics/stm32ai-modelzoo/blob/main/object_detection/yolov8n/README.md)

官方 N6570-DK、STEdgeAI 4.0.0 的纯推理参考为 256 输入 23.83 ms、320 输入 29.93 ms。它们不是本正点原子 N647 工程的实测耗时，也不包含本项目的完整 UI、传感器和调度负载，不能保证升级后仍为 30 Hz。

YOLO11n 256 可作为第二候选，ST 也列出 STM32N6 支持与量化模型；ST YOLOXn 可用于考察速度和资源余量。最终选择依据同一场景的识别表现与端到端延迟，而不是代际数字。[ST YOLO11n 资料](https://github.com/STMicroelectronics/stm32ai-modelzoo/blob/main/object_detection/yolov11n/README.md)、[ST YOLOXn 资料](https://github.com/STMicroelectronics/stm32ai-modelzoo/blob/main/object_detection/st_yoloxn/README.md)

conf 0.6 与 NMS IoU 0.3 适合做可重复的校准：降低 confidence 可能减少漏检，也增加误检；提高 NMS IoU 可能保留重叠人体，也保留重复框。没有标注对照时不直接修改默认值并称之为准确率优化。人员逻辑已有 1 秒连续确认，后续融合须与现有确认区分，且不能用旧框冒充新检测。

## 本机准备与迁移工作

本机已找到并实际运行 `D:/STM32-Toolchain/Apps/STEdgeAI/4.0/Utilities/windows/stedgeai.exe --version`：**ST Edge AI Core v4.0.1-20581**。工具未进入当前 PowerShell PATH，可通过绝对路径调用。

当前工程只找到原 Tiny YOLO v2 TFLite 模型，没有现成的新模型接入产物或配套的本机场景标注评估集。可以先取得官方已量化模型验证转换与适配；证明真实场景提升仍需要同一批有标注的输入。

当前 LL_ATON 为 `1.0.0-dev16`；已安装工具的运行库为 `1.1.3-dev275`。旧生成代码严格核对版本，不能直接复制新 `network.c` 或只把 `NN_WIDTH` 改成 320。

迁移需依次完成：

1. 保存 Tiny YOLO v2 基线，核对候选模型实际输入类型、量化参数、通道顺序、输出布局和类别。
2. 同步适配 NPU 运行库与编译参数，以本板内存池生成网络；保留正点原子硬件启动、加载器和已验证摄像头／触摸代码。
3. 更新摄像头推理尺寸、输入／输出双缓冲和缓存维护，适配实际输出对应的后处理；同视野的显示映射仍须正确。
4. 重新生成应用和模型权重，检查当前模型起点 `0x70200000` 及与应用、RAM 的边界。新 `appli.hex` 必须与新 `network-data.hex` 配套，不能继续使用旧权重。
5. 对同一组正常、远处、遮挡、多人、空场景记录漏检、误检、人数错误；同时测 NN、PP、Frame、丢帧及 P95，检查触摸、DHT11、诊断滑动和长期运行。

PC LVGL 预览只模拟检测结果，不能用于证明新网络识别准确率或硬件性能。已有后处理差分与缓冲测试用于验证实现正确，不能代替真实模型评价。

官方移植参考：[STM32N6 ObjectDetection](https://github.com/STMicroelectronics/STM32N6-GettingStarted-ObjectDetection)。该项目支持的 N6570-DK／NUCLEO 硬件配置应作为参考，不能直接替代本板工程。
