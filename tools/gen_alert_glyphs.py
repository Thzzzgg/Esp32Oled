#!/usr/bin/env python3
"""產生警報畫面用的漢字點陣字模（src/alert_glyphs.h）。

為什麼要自己產生：TFT_eSPI 內建字型只有 ASCII，畫面上要有「警告」「緊急」這類漢字，
只能把需要的幾個字先轉成點陣放進韌體。只需要 8 個字，約 2.5KB。

字體：Noto Serif JP（SIL OFL 1.1，可自由使用與嵌入），取 Black 字重，再水平壓縮成「長体」，
模仿 EVA 標題字那種又黑又瘦高的明朝體。

用法（需要 Pillow：pip install pillow）：
    python tools/gen_alert_glyphs.py
    python tools/gen_alert_glyphs.py --font C:/path/NotoSerifJP-VF.ttf --preview preview.png
"""
import argparse
import math
import os
import sys

from PIL import Image, ImageDraw, ImageFont

# 順序要和 src/ui.cpp 的警報表一致：每兩個字一組（警告、緊急、異常、危険）
GLYPHS = [
    ("KEI", "警"), ("KOKU", "告"),
    ("KIN", "緊"), ("KYU", "急"),
    ("I", "異"), ("JO", "常"),
    ("KI", "危"), ("KEN", "険"),
]

GLYPH_W = 40   # 輸出字模寬（像素），壓縮後的寬度
GLYPH_H = 64   # 輸出字模高（像素）
WEIGHT = 780   # 接近 Black，但保留「警」「緊」下半部的筆劃空隙
THRESHOLD = 100  # 二值化門檻，越低越粗


def load_font(path, size):
    font = ImageFont.truetype(path, size)
    try:
        axes = font.get_variation_axes()
        font.set_variation_by_axes([WEIGHT if a.get("name") in (b"Weight", "Weight") else a["default"] for a in axes])
    except Exception as exc:  # 不是可變字型時就用預設字重
        print(f"注意：無法設定字重（{exc}），使用字型預設字重", file=sys.stderr)
    return font


def render_ink(font, ch, canvas=400):
    """把字畫在大畫布上，回傳（影像, 墨水外框）。畫布要夠大，字才不會被邊緣切掉。"""
    img = Image.new("L", (canvas, canvas), 0)
    ImageDraw.Draw(img).text((100, 100), ch, font=font, fill=255)
    return img, img.getbbox()


def build_bitmaps(font_path):
    # 先用大字畫出所有字，取共同的墨水外框，這樣八個字的基線與大小一致
    font = load_font(font_path, 120)
    rendered = [render_ink(font, ch) for _, ch in GLYPHS]
    boxes = [b for _, b in rendered]
    if any(b is None for b in boxes):
        raise SystemExit("字型缺少所需的字，請換一個含日文漢字的字型")
    left = min(b[0] for b in boxes)
    top = min(b[1] for b in boxes)
    right = max(b[2] for b in boxes)
    bottom = max(b[3] for b in boxes)

    bitmaps = []
    for (img, _), (name, ch) in zip(rendered, GLYPHS):
        crop = img.crop((left, top, right, bottom))
        # 縮成固定大小：寬 GLYPH_W、高 GLYPH_H，寬度比例被壓縮就是「長体」
        small = crop.resize((GLYPH_W, GLYPH_H), Image.LANCZOS)
        bw = small.point(lambda v: 255 if v >= THRESHOLD else 0)
        bitmaps.append((name, ch, bw))
    squeeze = GLYPH_W / (right - left) / (GLYPH_H / (bottom - top))
    return bitmaps, squeeze


def to_bytes(img):
    """1 位元點陣，每列補到整數位元組，最高位元在左（TFT_eSPI drawBitmap 的格式）。"""
    row_bytes = (GLYPH_W + 7) // 8
    px = img.load()
    out = []
    for y in range(GLYPH_H):
        for bx in range(row_bytes):
            byte = 0
            for bit in range(8):
                x = bx * 8 + bit
                if x < GLYPH_W and px[x, y]:
                    byte |= 0x80 >> bit
            out.append(byte)
    return out


def write_header(path, bitmaps, font_name):
    lines = [
        "// 警報畫面用的漢字點陣字模。由 tools/gen_alert_glyphs.py 產生，請勿手動修改。",
        f"// 字體：{font_name}（SIL OFL 1.1），Black 字重，水平壓縮成長体。",
        "#pragma once",
        "#include <Arduino.h>",
        "",
        f"constexpr int GLYPH_W = {GLYPH_W};",
        f"constexpr int GLYPH_H = {GLYPH_H};",
        "",
        "// 每個字 (GLYPH_W + 7) / 8 * GLYPH_H 位元組，最高位元在左",
    ]
    for name, ch, img in bitmaps:
        data = to_bytes(img)
        lines.append(f"// {ch}")
        lines.append(f"const uint8_t GLYPH_{name}[] PROGMEM = {{")
        row_bytes = (GLYPH_W + 7) // 8
        for i in range(0, len(data), row_bytes):
            lines.append("    " + ", ".join(f"0x{b:02X}" for b in data[i:i + row_bytes]) + ",")
        lines.append("};")
        lines.append("")
    lines.append("// 依序：警 告 緊 急 異 常 危 険（兩個字一組）")
    lines.append("const uint8_t *const ALERT_GLYPHS[] = {")
    for name, _, _ in bitmaps:
        lines.append(f"    GLYPH_{name},")
    lines.append("};")
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lines) + "\n")


def write_preview(path, bitmaps):
    scale = 4
    gap = 8
    w = len(bitmaps) * (GLYPH_W * scale + gap) + gap
    sheet = Image.new("RGB", (w, GLYPH_H * scale + 2 * gap), (0, 0, 0))
    for i, (_, _, img) in enumerate(bitmaps):
        big = img.resize((GLYPH_W * scale, GLYPH_H * scale), Image.NEAREST).convert("RGB")
        orange = Image.new("RGB", big.size, (255, 110, 0))
        sheet.paste(orange, (gap + i * (GLYPH_W * scale + gap), gap), big.convert("L"))
    sheet.save(path)


def main():
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--font", default="C:/Windows/Fonts/NotoSerifJP-VF.ttf")
    parser.add_argument("--out", default=os.path.join(root, "src", "alert_glyphs.h"))
    parser.add_argument("--preview", help="另外輸出一張放大的預覽圖（PNG）")
    args = parser.parse_args()

    bitmaps, squeeze = build_bitmaps(args.font)
    write_header(args.out, bitmaps, os.path.basename(args.font).split("-")[0])
    print(f"已輸出 {args.out}（{len(bitmaps)} 個字，每個 {GLYPH_W}x{GLYPH_H}，水平壓縮比約 {squeeze:.2f}）")
    if args.preview:
        write_preview(args.preview, bitmaps)
        print(f"已輸出預覽 {args.preview}")


if __name__ == "__main__":
    main()
