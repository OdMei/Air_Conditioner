# Smartwatch／Bento 原生 LVGL 资源

96px数字子集保持V11内容；2026-10-14的Bento界面采用28px原生图标和统一2px圆端线。用户接受Bento布局后要求恢复V11鲜艳风格，并进一步加强蓝色波浪，当前背景提升蓝／青亮度与饱和度。鲜艳融合主页引用背景；原深色稿不引用。背景和图标为工程自绘；不下载商业图标、不复制Windows字体，不启用PNG、SVG、文件系统或Lottie解码器。

| 资源 | 运行时格式 | 像素／位图 Flash 字节 |
| --- | --- | ---: |
| 200×120 静态波纹背景 | RGB565，小端，每行 400 字节 | 48000 |
| AC Watch Digits 96，仅 `0..9` | LVGL plain 4bpp，无字距表 | 19373 |
| 14 个 28×28 自绘统一线宽图标 | A8 alpha mask，每行 28 字节 | 10976 |
| 合计，不含少量描述符 | 只读 C 数组，无运行时整屏 canvas | 78349 |

字体使用仓库原有 `NotoSansSC-Regular.ttf`，原始许可证见 [OFL-NotoSansSC.txt](OFL-NotoSansSC.txt)。派生字体名为 AC Watch Digits 96；源 SHA-256 为 `ae82f4e2a55e1316a55bcc1d05e9555ce08d8bda07e893b486896b626fd852ff`。以 96px 光栅化，数字推进 53px，实际像素高度 72～74px。数字子集采用紧凑 `line_height=96`、`base_line=12`，即有效 ascent 84px；原始 Noto 行度量 112+28 不用于此数字显示。摄氏符号、小数点、百分号与中文继续使用已有字体，避免额外大字号字库。

背景参考仓库自己的 F 草案 SVG，在生成时一次性光栅化和 RGB565 量化。LVGL 创建单个 `lv_image`，设置 `app_ui_watch_background`、`lv_image_set_inner_align(image, LV_IMAGE_ALIGN_TOP_LEFT)`、`lv_image_set_pivot(image,0,0)`、`lv_image_set_scale(image,APP_UI_WATCH_BACKGROUND_SCALE)` 并放置于 `(0,0)`。1024 是 LVGL 4×缩放值；200×120 源资源呈现为 800×480，仍由原生软件绘制器绘制，可开启图像抗锯齿。不要将对象再扩成 800×480 后按默认中心 pivot 缩放。背景不创建定时器，也不移动波纹。

图标通过原有 `app_ui_watch_icon(APP_UI_WATCH_ICON_...)` 取得，枚举和函数签名不变；`APP_UI_WATCH_ICON_SIZE`现为28。只读描述符可由多个控件复用。图形在24单位设计网格上直接光栅化为28×28，采用统一实际2px线宽和圆端，通过4倍采样后降采样形成抗锯齿边缘。每个图标按实际轮廓包围盒居中，不把窄图形强行拉伸到相同宽度。风扇改为三片较宽曲线叶片，摆风减少密集通风线和箭头，雪花去除过细分叉；所有图标在LVGL中保持1:1显示，不用运行时缩放补尺寸。

A8图像设置 `lv_obj_set_style_image_recolor(icon,color,0)` 和 `lv_obj_set_style_image_recolor_opa(icon,LV_OPA_COVER,0)` 即使用界面统一颜色；图标本身不响应事件，事件交给所属按钮。相对旧24px图标增加 `2912`字节，只占只读Flash，不建立临时canvas或增加解码缓存。主页不透明背景上的边缘最终受到ARGB4444的4位颜色量化，透明视觉叠层还受4位alpha量化，因此保留清楚的2px笔画。

可复现命令（与现有中文字体生成使用同一 Pillow／FreeType 版本）：

```powershell
& 'C:/Users/heal/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe' scripts/generate_watch_assets.py
& 'C:/Users/heal/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe' scripts/generate_watch_assets.py --check
```

唯一 MCU 资源 C 文件为 `app_ui_watch_assets.c`，公共 API 为 `app_ui_watch_assets.h`。生成器仅写资源 C、此说明和设计背景预览 PNG；预览 PNG 不进入固件。
