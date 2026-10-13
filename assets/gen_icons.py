"""Generate multi-size .ico files from Bootstrap SVGs.

Produces a classic Windows ICO:
  - 16/32/48/64/128: 32bpp BMP + 1bpp AND mask (best RC.exe compatibility)
  - 256: PNG-compressed (Vista+ requirement for 256)

Sizes: 16, 32, 48, 64, 128, 256
"""

from __future__ import annotations

import io
import os
import struct

import pymupdf as fitz
from PIL import Image

BASE = os.path.dirname(os.path.abspath(__file__))
NAMES = ("folder", "folder2-open", "folder-fill")
SIZES = (16, 32, 48, 64, 128, 256)


def render_svg(name: str) -> Image.Image:
    svg_path = os.path.join(BASE, f"{name}.svg")
    with open(svg_path, "rb") as f:
        svg = f.read()
    svg = svg.replace(b'fill="currentColor"', b'fill="#000000"')

    doc = fitz.open(stream=svg, filetype="svg")
    page = doc[0]
    zoom = 256 / max(page.rect.width, page.rect.height)
    pix = page.get_pixmap(matrix=fitz.Matrix(zoom, zoom), alpha=True)
    png_path = os.path.join(BASE, f"{name}.png")
    pix.save(png_path)

    src = Image.open(png_path).convert("RGBA")
    if src.size != (256, 256):
        src = src.resize((256, 256), Image.Resampling.LANCZOS)
        src.save(png_path)
    return src


def row_stride(width: int, bpp: int) -> int:
    return ((width * bpp + 31) // 32) * 4


def bmp_icon_bytes(im: Image.Image) -> bytes:
    """Encode one ICO image as BITMAPINFOHEADER + XOR + AND (no BITMAPFILEHEADER)."""
    im = im.convert("RGBA")
    w, h = im.size
    pixels = im.load()

    xor_stride = row_stride(w, 32)
    and_stride = row_stride(w, 1)
    xor = bytearray(xor_stride * h)
    # For 32bpp icons Windows uses the alpha channel; AND mask must be zero.
    and_bits = bytearray(and_stride * h)

    for y in range(h):
        # BMP is bottom-up
        src_y = h - 1 - y
        xor_row = y * xor_stride
        for x in range(w):
            r, g, b, a = pixels[x, src_y]
            o = xor_row + x * 4
            xor[o : o + 4] = bytes((b, g, r, a))

    header = struct.pack(
        "<IIIHHIIIIII",
        40,  # biSize
        w,
        h * 2,  # includes AND mask
        1,  # planes
        32,  # bit count
        0,  # BI_RGB
        len(xor) + len(and_bits),
        0,
        0,
        0,
        0,
    )
    return header + bytes(xor) + bytes(and_bits)


def png_bytes(im: Image.Image) -> bytes:
    buf = io.BytesIO()
    im.convert("RGBA").save(buf, format="PNG", optimize=True)
    return buf.getvalue()


def write_ico(path: str, images: list[Image.Image]) -> None:
    blobs: list[bytes] = []
    for im in images:
        if im.size[0] >= 256:
            blobs.append(png_bytes(im))
        else:
            blobs.append(bmp_icon_bytes(im))

    offset = 6 + 16 * len(images)
    entries = bytearray()
    for im, data in zip(images, blobs):
        w, h = im.size
        entry_w = 0 if w >= 256 else w
        entry_h = 0 if h >= 256 else h
        bpp = 32
        entries += struct.pack(
            "<BBBBHHII",
            entry_w,
            entry_h,
            0,
            0,
            1,
            bpp,
            len(data),
            offset,
        )
        offset += len(data)

    with open(path, "wb") as f:
        f.write(struct.pack("<HHH", 0, 1, len(images)))
        f.write(entries)
        for data in blobs:
            f.write(data)


def read_ico_sizes(path: str) -> list[tuple[int, int, int]]:
    with open(path, "rb") as f:
        _reserved, _itype, count = struct.unpack("<HHH", f.read(6))
        out = []
        for _ in range(count):
            w, h, _cl, _res, _planes, bits, size, _off = struct.unpack("<BBBBHHII", f.read(16))
            out.append((w or 256, h or 256, size))
        return out


def main() -> None:
    for name in NAMES:
        src = render_svg(name)
        images = [src.resize((s, s), Image.Resampling.LANCZOS) for s in SIZES]
        ico_path = os.path.join(BASE, f"{name}.ico")
        write_ico(ico_path, images)
        sizes = read_ico_sizes(ico_path)
        print(f"{name}.ico -> {len(sizes)} images: " + ", ".join(f"{w}x{h}" for w, h, _ in sizes))
        assert [w for w, _, _ in sizes] == list(SIZES), sizes


if __name__ == "__main__":
    main()
