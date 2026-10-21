# 克隆、构建与配套固件

当前源码基线为2026-10-15实板确认的V12：温湿度一行两张148×70px小卡、40px读数、拖环及加减调温。用户已确认新界面、调温、触摸与摄像头正常。PC预览使用模拟数据。

## 直接使用配套固件

`Project/Project/Air_Conditioner/Binary/`保存同一版本的烧录文件：

| 文件 | 地址 | 用途 |
| --- | --- | --- |
| `fsbl.hex` | `0x70000000` | 原FSBL启动程序 |
| `appli.hex` | `0x70100400` | 最新V12并排小卡应用 |
| `network-data.hex` | `0x70200000` | 配套YOLOv8n人体模型权重 |

应用BIN有效长度991232B，SHA256为`10741743c796c5e17bd7fb5415a328f95d23dac1da3a5633e066e651c05f79d7`；应用HEX SHA256为`78b2f3e78d8ffcc25e72ad709e7fd62b041d6823c78fa914c30264d8e882a0ba`。权重BIN为3011681B，SHA256为`b6e81e4337aae81d38243f5423d9c0d81bf9f9bf616432153385a0cefad9aa96`。

使用仓库`External_Loader/MX25UM25645G_ATK-CNN647B/Binary/`中的开发板加载器。按[正点原子流程](https://wiki.alientek.com/docs/Boards/STM32/DNN647/start-guide/start-development/step-by-step/)进入开发烧录模式，烧录后将BOOT0、BOOT1都接GND并重新上电。应用、模型生成代码与权重需要配套使用。

## 重新构建板端应用

安装Python 3和ARM GCC。脚本支持显式指定本机编译器，示例中的路径需要替换为自己的安装位置：

```powershell
python scripts/build_firmware.py --phase final --label local_v12 --gcc 'C:/your-toolchain/bin/arm-none-eabi-gcc.exe' --jobs 8
```

构建结果位于`build/firmware/local_v12/`。每次使用新的`--label`，完整日志、指纹与应用地址检查记录在该目录。脚本读取已纳入Git的33份Release文本配方，再重新编译当前源码，不复用旧对象。`.o`、ELF、MAP、缓存及构建输出继续留在本机。

模型输入`Model/network_data.xSPI2.bin`与候选TFLite、生成代码和运行库已纳入仓库，供模型契约检查使用。`.gitattributes`保留模型生成代码、权重HEX及SDK快照原始字节，避免换行转换影响SHA核验。更换模型的完整过程见[模型接入记录](model-upgrade-20261013-v10.md)。

## 在其他电脑运行PC预览

依赖CMake、Ninja、Windows GCC与SDL2。当前`simulator/CMakePresets.json`使用`C:/msys64/mingw64`，专用VS Code工作区中的CMake命令指向原开发电脑的Visual Studio安装目录；在其他电脑上需要调整这两处配置。

也可将CMake加入PATH后手动配置，按本机位置修改下面的工具路径：

```powershell
cmake -S simulator -B simulator/build -G Ninja -DCMAKE_C_COMPILER=C:/msys64/mingw64/bin/gcc.exe -DCMAKE_MAKE_PROGRAM=C:/msys64/mingw64/bin/ninja.exe -DSDL2_ROOT=C:/msys64/mingw64
cmake --build simulator/build --parallel 8
./simulator/build/lvgl_preview.exe
```

当前源码直接使用已提交的中文字体与图标C资源。重新生成资源时，按[资源说明](fonts/watch-resources.md)使用固定的Pillow／FreeType版本。

## 验证记录

该基线此前通过438项PC回归、546个板端源文件全量编译，以及应用和模型完整回读。本次上传还在独立导出的Git暂存快照中通过了模型契约校验、4项构建脚本测试，并成功生成546个源文件、1549个输入的构建计划；没有复用本机忽略的对象文件。实板确认覆盖主页、调温、触摸与摄像头，AI准确率与推理更新率没有新增测量。
