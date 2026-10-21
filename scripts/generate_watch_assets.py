"""Generate small, native LVGL assets for the Watch/Bento home page.

No network, external icon downloads, runtime image decoders or installed fonts.
Use the same Pillow/FreeType runtime as scripts/generate_ui_font.py for --check.
"""
from pathlib import Path
import hashlib
import math
import sys

from PIL import Image, ImageDraw, ImageFont

from generate_ui_font import has_glyph

ROOT = Path(__file__).resolve().parents[1]
CORE = ROOT / "Project/Project/Air_Conditioner/Appli/Core"
SOURCE = ROOT / "Middlewares/LVGL/tests/src/test_files/fonts/noto/NotoSansSC-Regular.ttf"
OUTPUT = CORE / "Src/app_ui_watch_assets.c"
PREVIEW = ROOT / "docs/images/ui-options-20261013/watch-background.png"
DESCRIPTION = ROOT / "docs/fonts/watch-resources.md"
ICON_NAMES = ["home", "data", "history", "alert", "vision", "fan", "swing",
              "moon", "eco", "turbo", "light", "power", "snow", "more"]
ICON_SIZE = 28
ICON_STROKE = 2.0


def cubic(start, control1, control2, end, count=96):
    points = []
    for step in range(count + 1):
        t = step / count
        u = 1 - t
        points.append((u**3 * start[0] + 3*u*u*t*control1[0] +
                       3*u*t*t*control2[0] + t**3*end[0],
                       u**3 * start[1] + 3*u*u*t*control1[1] +
                       3*u*t*t*control2[1] + t**3*end[1]))
    return points


def wave(points):
    contour = [points[0]]
    for index in range(1, len(points), 3):
        contour.extend(cubic(contour[-1], *points[index:index + 3])[1:])
    contour.extend([(830, 540), (-40, 540)])
    return contour


def background():
    """Static F landscape, with the user's brighter blue/cyan wave palette."""
    result = Image.new("RGB", (800, 480), "#06172e")
    draw = ImageDraw.Draw(result)
    draw.polygon(wave([(-40,165),(92,93),(146,186),(273,159),
                       (400,132),(498,89),(625,151),
                       (752,213),(765,103),(830,100)]), fill="#093668")
    # The original SVG's smooth cubic commands are made explicit here.
    points = [(-40,260),(112,167),(192,290),(340,218),
              (488,146),(492,211),(626,218),
              (760,225),(781,203),(830,255)]
    mask = Image.new("L", result.size, 0)
    ImageDraw.Draw(mask).polygon(wave(points), fill=255)
    gradient = Image.new("RGB", result.size)
    pixels = gradient.load()
    stops = [(0, (7,70,150)), (.52, (8,110,157)), (1, (67,62,143))]
    for y in range(480):
        for x in range(800):
            position = (x/799 + y/479)/2
            lo, hi = (stops[0], stops[1]) if position <= .52 else (stops[1], stops[2])
            ratio = (position-lo[0])/(hi[0]-lo[0])
            pixels[x,y] = tuple(round(a+(b-a)*ratio) for a,b in zip(lo[1],hi[1]))
    result.paste(gradient, mask=mask)
    draw = ImageDraw.Draw(result)
    draw.polygon(wave([(-40,348),(129,247),(241,381),(398,319),
                       (555,257),(581,268),(830,330)]), fill="#076ea4")
    draw.polygon(wave([(-40,434),(113,318),(226,447),(405,390),
                       (584,333),(612,339),(830,449)]), fill="#038eb1")
    draw.polygon(wave([(-40,480),(120,399),(242,487),(386,448),
                       (530,409),(584,409),(830,485)]), fill="#10adb7")
    reduced = result.resize((200,120), Image.Resampling.LANCZOS)
    packed = bytearray()
    raw = reduced.tobytes()
    for red, green, blue in zip(raw[0::3], raw[1::3], raw[2::3]):
        rgb565 = ((red >> 3) << 11) | ((green >> 2) << 5) | (blue >> 3)
        packed.extend((rgb565 & 255, rgb565 >> 8))
    return bytes(packed), reduced.resize((800,480), Image.Resampling.BILINEAR)


def icon(name):
    """Original 28px drawings with 2px round strokes and centered ink bounds.

    Geometry uses a shared 24-unit design grid. It is rasterized directly at
    28px, so LVGL displays the final A8 mask at 1:1 without runtime scaling.
    """
    scale = 4
    geometry_scale = ICON_SIZE / 24.0
    image = Image.new("L", (ICON_SIZE*scale,ICON_SIZE*scale), 0)
    draw = ImageDraw.Draw(image)

    def line(points):
        coords = [(round(x*geometry_scale*scale),round(y*geometry_scale*scale))
                  for x,y in points]
        stroke = round(ICON_STROKE*scale)
        draw.line(coords, fill=255, width=stroke, joint="curve")
        radius = stroke/2
        for x,y in (coords[0],coords[-1]):
            draw.ellipse((round(x-radius),round(y-radius),
                          round(x+radius)-1,round(y+radius)-1),fill=255)

    def arc(box, start, end):
        left,top,right,bottom = box
        center_x,center_y = (left+right)/2,(top+bottom)/2
        radius_x,radius_y = (right-left)/2,(bottom-top)/2
        count = max(12, round(abs(end-start)/4))
        line([(center_x+radius_x*math.cos(math.radians(start+(end-start)*i/count)),
               center_y+radius_y*math.sin(math.radians(start+(end-start)*i/count)))
              for i in range(count+1)])

    def ellipse(box, fill=None):
        if fill is None:
            arc(box,0,360)
        else:
            draw.ellipse(tuple(round(value*geometry_scale*scale) for value in box),fill=fill)

    if name == "home":
        for x,y in [(5,8),(12,15),(19,9)]:
            line([(x,3),(x,21)])
            line([(x-3,y),(x+3,y)])
    elif name == "data":
        line([(3,3),(3,21),(22,21)])
        line([(6,16),(11,11),(15,14),(21,6)])
    elif name == "history":
        arc((3,3,21,21),210,510)
        line([(3,3),(3,9),(9,9)])
        line([(12,7),(12,13),(16,15)])
    elif name == "alert":
        line([(5,18),(7,15),(7,9)])
        arc((7,4,17,14),180,360)
        line([(17,9),(17,15),(19,18),(5,18)])
        arc((10,17,14,22),0,180)
        line([(12,2),(12,4)])
    elif name == "vision":
        upper = cubic((2,12),(7,3),(17,3),(22,12),24)
        lower = cubic((22,12),(17,21),(7,21),(2,12),24)
        line(upper+lower)
        ellipse((8,8,16,16))
    elif name == "fan":
        ellipse((10,10,14,14))
        # Three broad petals retain the fan silhouette without the old
        # narrow six-segment blades and their crowded inner crossings.
        petal = (cubic((11,9),(8,5),(10,2),(13,3),16) +
                 cubic((13,3),(17,4),(16,8),(14,10),16)[1:] +
                 cubic((14,10),(13,9),(12,9),(11,9),12)[1:])
        for angle in (0,120,240):
            values = []
            for x,y in petal:
                radians = math.radians(angle)
                values.append((12+(x-12)*math.cos(radians)-(y-12)*math.sin(radians),
                               12+(x-12)*math.sin(radians)+(y-12)*math.cos(radians)))
            line(values)
    elif name == "swing":
        # A clean vent and one wide swing arrow replace the cramped pair
        # of downward arrows and an extra line inside the old vent.
        line([(3,4),(21,4),(21,9),(3,9),(3,4)])
        line([(4,17),(20,17)])
        line([(7,14),(4,17),(7,20)])
        line([(17,14),(20,17),(17,20)])
    elif name == "moon":
        outer = cubic((13,3),(1,1),(0,21),(13,21),36)
        outer += cubic((13,21),(18,21),(20,18),(21,15),20)[1:]
        inner = cubic((21,15),(12,17),(8,9),(13,3),36)
        line(outer+inner[1:])
    elif name == "eco":
        line(cubic((5,18),(1,7),(15,5),(21,3),32)+
             cubic((21,3),(22,14),(18,22),(5,18),32)[1:])
        line([(3,21),(16,9)])
    elif name == "turbo":
        line([(14,2),(4,14),(11,14),(9,22),(21,10),(14,10),(14,2)])
    elif name == "light":
        arc((6,2,18,14),180,360)
        line([(6,8),(6,11),(9,17),(15,17),(18,11),(18,8)])
        line([(9,20),(15,20)])
        line([(10,22),(14,22)])
    elif name == "power":
        # Keep the ring opening around its top stem. The old 225..495
        # angles left the opening at the upper-left side of the symbol.
        arc((3,4,21,22),310,590)
        line([(12,2),(12,12)])
    elif name == "snow":
        for angle in (0,60,120):
            radians = math.radians(angle)
            dx,dy = math.cos(radians),math.sin(radians)
            line([(12-8*dx,12-8*dy),(12+8*dx,12+8*dy)])
        # Six spokes plus their terminal dots stay legible next to CJK
        # text; the old twelve tiny branches merged at this display size.
        for angle in range(0,360,60):
            x,y = 12+8*math.cos(math.radians(angle)),12+8*math.sin(math.radians(angle))
            ellipse((x-.8,y-.8,x+.8,y+.8),fill=255)
    elif name == "more":
        for x in (5,12,19):
            ellipse((x-1.3,10.7,x+1.3,13.3),fill=255)
    else:
        raise ValueError(name)
    # Match the center of the visible stroke bounds, rather than assuming
    # that dissimilar contours fill their entire 28px container equally.
    # Center before reduction to retain subpixel alignment and 2px weight.
    bounds = image.getbbox()
    if bounds is None:
        raise RuntimeError(f"Empty icon: {name}")
    ink = image.crop(bounds)
    centered = Image.new("L",image.size,0)
    centered.paste(ink,((image.width-ink.width)//2,(image.height-ink.height)//2))
    return centered.resize((ICON_SIZE,ICON_SIZE), Image.Resampling.LANCZOS).tobytes()


def c_bytes(name, data):
    rows = ["    " + ", ".join(f"0x{value:02X}" for value in data[index:index+16]) + ","
            for index in range(0,len(data),16)]
    return (f"static const LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_LARGE_CONST uint8_t {name}[] = {{\n"
            + "\n".join(rows) + "\n};\n")


def image_dsc(name, data_name, width, height, color_format, stride, data_size, storage="const"):
    return f'''{storage} lv_image_dsc_t {name} = {{
    .header = {{ .magic = LV_IMAGE_HEADER_MAGIC, .cf = {color_format}, .flags = 0,
                .w = {width}, .h = {height}, .stride = {stride}, .reserved_2 = 0 }},
    .data_size = {data_size}, .data = {data_name}, .reserved = NULL
}};
'''


def digit_font():
    source_bytes = SOURCE.read_bytes()
    if not all(has_glyph(source_bytes,ord(character)) for character in "0123456789"):
        raise RuntimeError("Digit font source lacks a requested glyph")
    font = ImageFont.truetype(str(SOURCE),96,layout_engine=ImageFont.Layout.BASIC)
    bitmap = bytearray()
    descriptors = ["    {0, 0, 0, 0, 0, 0}, /* Missing glyph. */"]
    for character in "0123456789":
        mask,(left,top) = font.getmask2(character,mode="L",anchor="ls")
        width,height = mask.size
        index = len(bitmap)
        pixels = bytes(mask)
        for offset in range(0,len(pixels),2):
            bitmap.append((pixels[offset]&0xf0)|
                          (pixels[offset+1]>>4 if offset+1<len(pixels) else 0))
        advance = round(font.getlength(character)*16)
        descriptors.append(f"    {{{index}, {advance}, {width}, {height}, {left}, {-top-height}}}, /* {character} */")
    content = c_bytes("watch_glyph_bitmap",bitmap) + f'''
static const lv_font_fmt_txt_glyph_dsc_t watch_glyph_dsc[] = {{
{chr(10).join(descriptors)}
}};

static const lv_font_fmt_txt_cmap_t watch_cmaps[] = {{
    {{ .range_start = 48, .range_length = 10, .glyph_id_start = 1,
       .unicode_list = NULL, .glyph_id_ofs_list = NULL,
       .list_length = 0, .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY }}
}};

static const lv_font_fmt_txt_dsc_t watch_font_dsc = {{
    .glyph_bitmap = watch_glyph_bitmap, .glyph_dsc = watch_glyph_dsc,
    .cmaps = watch_cmaps, .kern_dsc = NULL, .kern_scale = 0,
    .cmap_num = 1, .bpp = 4, .kern_classes = 0,
    .bitmap_format = LV_FONT_FMT_TXT_PLAIN
}};

const lv_font_t app_font_watch_96 = {{
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,
    .line_height = 96, .base_line = 12, .subpx = LV_FONT_SUBPX_NONE,
    .underline_position = -2, .underline_thickness = 1,
    .dsc = &watch_font_dsc
}};
'''
    return content,len(bitmap),hashlib.sha256(source_bytes).hexdigest()


def main():
    packed,preview = background()
    font_source,font_bytes,font_sha = digit_font()
    icon_data = [(name,icon(name)) for name in ICON_NAMES]
    total_bytes = len(packed) + font_bytes + sum(len(data) for _,data in icon_data)
    if total_bytes > 100*1024:
        raise RuntimeError(f"Assets exceed the 100 KiB pixel-data budget: {total_bytes}")
    generated = f'''/* Generated by scripts/generate_watch_assets.py; do not edit bitmap data.
 * Static original blue landscape and original 28px, 2px-stroke alpha-mask icons.
 * Font subset: AC Watch Digits 96; Noto Sans SC Regular, SIL OFL 1.1.
 * Copyright 2014-2021 Adobe, Reserved Font Name 'Source'.
 * Source SHA-256: {font_sha}
 * License/provenance: docs/fonts/OFL-NotoSansSC.txt and watch-resources.md.
 * Pixel data: RGB565 {len(packed)} + font4bpp {font_bytes} + iconsA8 {len(icon_data)*ICON_SIZE*ICON_SIZE}
 * = {total_bytes} bytes; descriptors are additional read-only metadata.
 */
#include "app_ui_watch_assets.h"

'''
    generated += c_bytes("watch_background_pixels",packed)
    generated += "\n" + image_dsc("app_ui_watch_background","watch_background_pixels",
                                  200,120,"LV_COLOR_FORMAT_RGB565",400,len(packed))
    generated += "\n" + font_source
    for name,data in icon_data:
        generated += "\n" + c_bytes(f"watch_icon_{name}_pixels",data)
        generated += image_dsc(f"watch_icon_{name}",f"watch_icon_{name}_pixels",
                               ICON_SIZE,ICON_SIZE,"LV_COLOR_FORMAT_A8",ICON_SIZE,
                               ICON_SIZE*ICON_SIZE,"static const")
    generated += "\nstatic const lv_image_dsc_t * const watch_icons[APP_UI_WATCH_ICON_COUNT] = {\n"
    generated += "\n".join(f"    &watch_icon_{name}," for name in ICON_NAMES) + "\n};\n"
    generated += '''
const lv_image_dsc_t *app_ui_watch_icon(app_ui_watch_icon_t icon)
{
    if((unsigned int)icon >= APP_UI_WATCH_ICON_COUNT) return NULL;
    return watch_icons[(unsigned int)icon];
}
'''
    if "--check" in sys.argv:
        if not OUTPUT.exists() or OUTPUT.read_text(encoding="utf-8") != generated:
            raise RuntimeError("Watch assets are stale; rerun scripts/generate_watch_assets.py")
        print(f"Watch assets are current: {total_bytes} pixel-data bytes, 10 digits, 14 icons")
        return
    OUTPUT.write_text(generated,encoding="utf-8",newline="\n")
    PREVIEW.parent.mkdir(parents=True,exist_ok=True)
    preview.save(PREVIEW)
    DESCRIPTION.write_text(f'''# Smartwatch／Bento 原生 LVGL 资源

96px数字子集保持V11内容；2026-10-14的Bento界面采用28px原生图标和统一2px圆端线。用户接受Bento布局后要求恢复V11鲜艳风格，并进一步加强蓝色波浪，当前背景提升蓝／青亮度与饱和度。鲜艳融合主页引用背景；原深色稿不引用。背景和图标为工程自绘；不下载商业图标、不复制Windows字体，不启用PNG、SVG、文件系统或Lottie解码器。

| 资源 | 运行时格式 | 像素／位图 Flash 字节 |
| --- | --- | ---: |
| 200×120 静态波纹背景 | RGB565，小端，每行 400 字节 | {len(packed)} |
| AC Watch Digits 96，仅 `0..9` | LVGL plain 4bpp，无字距表 | {font_bytes} |
| 14 个 28×28 自绘统一线宽图标 | A8 alpha mask，每行 28 字节 | {len(icon_data)*ICON_SIZE*ICON_SIZE} |
| 合计，不含少量描述符 | 只读 C 数组，无运行时整屏 canvas | {total_bytes} |

字体使用仓库原有 `NotoSansSC-Regular.ttf`，原始许可证见 [OFL-NotoSansSC.txt](OFL-NotoSansSC.txt)。派生字体名为 AC Watch Digits 96；源 SHA-256 为 `{font_sha}`。以 96px 光栅化，数字推进 53px，实际像素高度 72～74px。数字子集采用紧凑 `line_height=96`、`base_line=12`，即有效 ascent 84px；原始 Noto 行度量 112+28 不用于此数字显示。摄氏符号、小数点、百分号与中文继续使用已有字体，避免额外大字号字库。

背景参考仓库自己的 F 草案 SVG，在生成时一次性光栅化和 RGB565 量化。LVGL 创建单个 `lv_image`，设置 `app_ui_watch_background`、`lv_image_set_inner_align(image, LV_IMAGE_ALIGN_TOP_LEFT)`、`lv_image_set_pivot(image,0,0)`、`lv_image_set_scale(image,APP_UI_WATCH_BACKGROUND_SCALE)` 并放置于 `(0,0)`。1024 是 LVGL 4×缩放值；200×120 源资源呈现为 800×480，仍由原生软件绘制器绘制，可开启图像抗锯齿。不要将对象再扩成 800×480 后按默认中心 pivot 缩放。背景不创建定时器，也不移动波纹。

图标通过原有 `app_ui_watch_icon(APP_UI_WATCH_ICON_...)` 取得，枚举和函数签名不变；`APP_UI_WATCH_ICON_SIZE`现为28。只读描述符可由多个控件复用。图形在24单位设计网格上直接光栅化为28×28，采用统一实际2px线宽和圆端，通过4倍采样后降采样形成抗锯齿边缘。每个图标按实际轮廓包围盒居中，不把窄图形强行拉伸到相同宽度。风扇改为三片较宽曲线叶片，摆风减少密集通风线和箭头，雪花去除过细分叉；所有图标在LVGL中保持1:1显示，不用运行时缩放补尺寸。

A8图像设置 `lv_obj_set_style_image_recolor(icon,color,0)` 和 `lv_obj_set_style_image_recolor_opa(icon,LV_OPA_COVER,0)` 即使用界面统一颜色；图标本身不响应事件，事件交给所属按钮。相对旧24px图标增加 `{len(icon_data)*(ICON_SIZE*ICON_SIZE-24*24)}`字节，只占只读Flash，不建立临时canvas或增加解码缓存。主页不透明背景上的边缘最终受到ARGB4444的4位颜色量化，透明视觉叠层还受4位alpha量化，因此保留清楚的2px笔画。

可复现命令（与现有中文字体生成使用同一 Pillow／FreeType 版本）：

```powershell
& 'C:/Users/heal/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe' scripts/generate_watch_assets.py
& 'C:/Users/heal/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe' scripts/generate_watch_assets.py --check
```

唯一 MCU 资源 C 文件为 `app_ui_watch_assets.c`，公共 API 为 `app_ui_watch_assets.h`。生成器仅写资源 C、此说明和设计背景预览 PNG；预览 PNG 不进入固件。
''',encoding="utf-8",newline="\n")
    print(f"Generated {OUTPUT.name}: {total_bytes} pixel-data bytes "
          f"(background={len(packed)}, font={font_bytes}, icons={len(icon_data)*ICON_SIZE*ICON_SIZE}); "
          "font line_height=96/base_line=12, digit advance=53px")


if __name__ == "__main__":
    main()
