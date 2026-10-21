# 应用结构整理

本次按 [Ponytail 的简化方法](https://github.com/DietrichGebert/ponytail/blob/main/skills/ponytail/SKILL.md) 重构现有应用：复用已有类型和 ThreadX 能力，删除没有消费者的代码。日期：2026-10-07。

| 文件 | 职责 |
|---|---|
| `Appli/Core/Src/app.c` | 组织线程、采集输入、维护共享快照、连接视觉与 UI |
| `Appli/Core/Src/app_climate_logic.c` | 不依赖硬件的空调状态逻辑 |
| `Appli/Core/Src/app_input_logic.c` | 将板载按键映射为统一的 `app_climate_action_t` |
| `Appli/Core/Src/app_ui.c` | LVGL 显示与触摸事件，读取快照 |
| `Appli/Core/Src/app_camera.c`、`app_lcd.c` | 相机、显示管线及各自诊断数据 |
| `Drivers/BSP/REMOTE/remote.c` | 红外接收、解码及 HAL 定时器回调 |

表中 `Appli` 路径相对于 `Project/Project/Air_Conditioner`。

共享状态直接使用 `app_ui_snapshot_t`，空调电源、模式和目标温度只保存在其中的 `climate`。相机和 LCD 诊断分别嵌套已有的 `app_camera_display_diag_t`、`app_lcd_display_diag_t`，避免重复声明字段及逐项搬运。显示线程在锁内复制快照，解锁后采集诊断数据，保持硬件采集与共享状态锁分离。

相机启动结果通过一个 `camera_startup` 事件组广播。输入和推理线程使用 `TX_OR` 等待同一完成位，不清除该位；先等待和稍后到达的线程都能看到成功、失败或禁用结果。推理线程醒来后仍检查相机状态。显示维持原来的周期刷新，删除了仅被写入、从未被读取的 `display.update` 信号量。

删除了当前界面未使用的检测框副本、推理/后处理/显示耗时及最后按键字段；人数与 FPS 保留。模型内部后处理仍会使用检测框，删除的只是应用中的第二份副本。红外回调移动后只有驱动中保留定义，协议逻辑不变。

未增加生产源码文件、依赖或目录层级，因此无需更改 CubeIDE 的链接资源。原工程的既有未提交修改没有还原。操作前文件副本位于 `D:/Learning/AC-N647/tmp/ponytail_refactor/before`。

本次保持现有业务策略及硬件配置。触摸/相机共享 I2C2 的运行时冲突、相机线程优先级与输入的关系、AI 数据有效性、真实负载输出仍需单独处理。静态检查中的这些问题会保留为失败，不能通过改写断言将它们隐藏。

验证记录位于 `D:/Learning/AC-N647/tmp/ponytail_refactor`：

- 6 个受改生产文件合计从 1341 行减至 1131 行，净减少 210 行；其中 `app.c` 从 694 行减至 478 行。
- 使用 GNU Tools for STM32 14.3.1，按 Release 的 Cortex-M55/TrustZone/硬浮点参数重新编译 4 个受改 C 文件，复用 485 个历史对象后完成 ELF 链接。编译和链接无告警、无错误，未定义符号为零；两个红外回调各有一个定义。没有执行全部源码的 clean build。
- 同编译器比较重构前后：代码段增加 264 字节，初始化数据大小不变，BSS 减少 160 字节。结构更简洁不等同于固件更小或运行更快。
- 两个主机 C 测试编译并运行通过。20 个 PowerShell 检查中 18 个通过，2 个失败分别揭示既有的相机优先级和共享 I2C2 问题。实时模式下彩条启用检查明确跳过，彩条实现检查仍通过。
- 原应用 Binary 和历史对象未覆盖，没有烧录或实板测试。新 ELF 位于 `build/after/appli.elf`，编译与链接命令记录在同目录的 `manifest.json`。

主机测试汇总：`tests/summary.txt`；字段和文件差异清单：`changes.json`。相机启动广播、失败分支和诊断快照连线的源码保护已加入既有 `test_touch_startup_contract.ps1`；阈值测试现在检查 10/30 分钟之前 1 ms 与恰好阈值。

可复跑目标编译（写入验证目录，不更新烧录镜像）：

```powershell
& 'C:\Users\heal\AppData\Local\Programs\Python\Python312\python.exe' 'D:\Learning\AC-N647\tmp\ponytail_refactor\build\verify_arm.py' --label after
```
