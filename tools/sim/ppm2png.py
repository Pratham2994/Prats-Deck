"""Convert out/*.ppm to PNG (2x scaled) using only the standard library."""
import glob
import struct
import zlib


def to_png(src, dst, scale=2):
    with open(src, "rb") as f:
        data = f.read()
    parts = data.split(b"\n", 3)
    w, h = (int(v) for v in parts[1].split())
    px = parts[3]
    rows = []
    for y in range(h):
        row = px[y * w * 3:(y + 1) * w * 3]
        big = b"".join(row[i:i + 3] * scale for i in range(0, len(row), 3))
        rows.extend([b"\x00" + big] * scale)
    raw = b"".join(rows)

    def chunk(t, d):
        return struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d) & 0xFFFFFFFF)

    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w * scale, h * scale, 8, 2, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(raw, 6)) + chunk(b"IEND", b"")
    with open(dst, "wb") as f:
        f.write(png)


for p in sorted(glob.glob("out/*.ppm")):
    to_png(p, p[:-4] + ".png")
print("converted")
