"""Make src/fonts/inter.h: smooth (4 bits per pixel) fonts from the Inter typeface.

Needs Pillow and the Inter .ttf files (https://github.com/rsms/inter/releases, folder extras/ttf).

    pip install pillow
    python make_fonts.py path/to/extras/ttf

Inter is under the SIL Open Font License (see src/fonts/LICENSE-Inter.txt).
"""

import os
import sys

from PIL import Image, ImageDraw, ImageFont

ASCII = "".join(chr(c) for c in range(32, 127)) + "°"   # the degree sign is stored as character 127

# name, file, pixel size, characters
FONTS = [
    ("TINY", "Inter-SemiBold.ttf", 11, ASCII),
    ("SMALL", "Inter-Medium.ttf", 15, ASCII),
    ("MEDIUM", "Inter-SemiBold.ttf", 20, ASCII),
    ("LARGE", "InterDisplay-Bold.ttf", 29, ASCII),
    ("GIANT", "InterDisplay-SemiBold.ttf", 46, "0123456789:"),
    ("CLOCK", "InterDisplay-Medium.ttf", 104, "0123456789:"),
]


def glyph(font, ch):
    """(width, height, x offset, y offset from the baseline, advance, alpha values 0..15)"""
    pad = font.size * 2
    img = Image.new("L", (pad * 3, pad * 3), 0)
    ImageDraw.Draw(img).text((pad, pad), ch, font=font, fill=255, anchor="ls")
    box = img.getbbox()
    adv = int(round(font.getlength(ch)))
    if not box:
        return 0, 0, 0, 0, adv, []
    crop = img.crop(box)
    w, h = crop.size
    px = [min(15, (v + 8) // 17) for v in crop.getdata()]
    return w, h, box[0] - pad, box[1] - pad, adv, px


def main():
    src = sys.argv[1] if len(sys.argv) > 1 else "."
    here = os.path.dirname(os.path.abspath(__file__))
    out = os.path.join(here, "..", "..", "src", "fonts", "inter.h")
    lines = [
        "// inter.h",
        "// Smooth fonts made from Inter (SIL Open Font License) by tools/fonts/make_fonts.py.",
        "// Each glyph is a block of 4-bit coverage values, two per byte, high half first.",
        "#pragma once",
        "",
        "struct Glyph {",
        "  uint16_t off;        // start of the glyph in the font's data",
        "  uint8_t w, h;        // size in pixels",
        "  int8_t xo, yo;       // top-left corner, from the pen position on the baseline",
        "  uint8_t adv;         // pen movement",
        "};",
        "struct Font {",
        "  const uint8_t *bits;",
        "  const Glyph *glyph;",
        "  uint8_t first, last; // character range. Others are skipped",
        "  uint8_t size, cap;   // pixel size, height of a capital letter",
        "};",
        "",
    ]
    total = 0
    for name, file, size, chars in FONTS:
        font = ImageFont.truetype(os.path.join(src, file), size)
        first, last = (32, 127) if len(chars) > 20 else (ord(min(chars)), ord(max(chars)))
        table = {(127 if ch == "°" else ord(ch)): glyph(font, ch) for ch in chars}
        data, rows = bytearray(), []
        for code in range(first, last + 1):
            w, h, xo, yo, adv, px = table.get(code, (0, 0, 0, 0, 0, []))
            rows.append("{%d, %d, %d, %d, %d, %d}" % (len(data), w, h, xo, yo, adv))
            if len(px) % 2:
                px.append(0)
            data.extend((px[i] << 4) | px[i + 1] for i in range(0, len(px), 2))
        cap = table[ord("0")][1] if len(chars) < 20 else table[ord("H")][1]
        lines.append("// %s: %s, %d px" % (name, file, size))
        lines.append("static const uint8_t %s_BITS[] PROGMEM = {" % name)
        for i in range(0, len(data), 24):
            lines.append("  " + ", ".join("0x%02X" % b for b in data[i:i + 24]) + ",")
        lines.append("};")
        lines.append("static const Glyph %s_GLYPHS[] PROGMEM = {" % name)
        for i in range(0, len(rows), 4):
            lines.append("  " + ", ".join(rows[i:i + 4]) + ",")
        lines.append("};")
        lines.append("static const Font %s_FONT = {%s_BITS, %s_GLYPHS, %d, %d, %d, %d};" % (name, name, name, first, last, size, cap))
        lines.append("")
        total += len(data) + len(rows) * 8
    with open(out, "w", newline="\n") as f:
        f.write("\n".join(lines))
    print("wrote %s (%d KB of font data)" % (os.path.normpath(out), total // 1024))


if __name__ == "__main__":
    main()
