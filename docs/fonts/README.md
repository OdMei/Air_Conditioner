本工程的中文界面使用 `app_font_ui_16`（派生字体名称 AC UI 16）。字体源是仓库原有的 `Middlewares/LVGL/tests/src/test_files/fonts/noto/NotoSansSC-Regular.ttf`，许可证按原样保存在同目录的 `OFL-NotoSansSC.txt`。

内置 SimSun CJK 字体主要包含繁体及其他 CJK 字符，缺少本界面中的“调、湿、风、护”等简体字，因此采用 Noto Sans SC 的实际界面字符子集。生成字体包含可打印 ASCII 与 `app_ui.c` 字符串中的中文及标点，不复制完整字库，不修改 LVGL vendor。

修改界面文字后，从仓库根目录运行 `python scripts/generate_ui_font.py`。脚本使用 Pillow（本机现有 Python 运行时已提供），核对源字体 cmap 中的字符覆盖，再生成 16px、4bpp、未压缩、无字距调整的 LVGL 9 字体。输出文件为 `Project/Project/Air_Conditioner/Appli/Core/Src/app_font_ui_16.c`。字体源 SHA-256、字符数、位图字节数记录在生成文件头部。

运行 `python scripts/generate_ui_font.py --check` 可只读核对生成文件与当前界面字符集是否一致。

本仓库当前字体由 **Python 3.12.14、Pillow 12.3.0、FreeType 2.14.3** 生成。本机对应解释器为 `C:/Users/heal/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe`，可在仓库根目录执行：

```powershell
& 'C:/Users/heal/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe' scripts/generate_ui_font.py --check
```

`--check` 会重新光栅化并逐字比较整个字体 C 文件，因此可复现生成需要相同的 Pillow／FreeType 版本。已核对另一套 Python 3.12.4、Pillow 11.1.0、FreeType 2.13.3：字符集和 cmap 相同，但字形边界、字距和像素位图不同，会报 `stale`；这不代表当前界面新增了字符或缺少字形。请先核对生成运行时，避免仅因工具版本差异覆盖已验证的生产字体。

字体 C 文件与界面在 PC 和 MCU 工程中共同编译；用户界面的数字使用内置 Montserrat 48/20，LVGL 内存池仍为 96 KiB。生成脚本不访问网络、不安装字体或依赖，不进行硬件操作。
