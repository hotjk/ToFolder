"""Generate multi-size .ico files from Bootstrap SVGs.

Produces a compact Windows ICO with PNG-compressed frames only:
  sizes 16, 32, 48, 256 (tray + Explorer; no large uncompressed BMPs)

Black strokes only — white tray glyphs are recolored at runtime.
"""

from __future__ import annotations

import io
import os
import struct

import pymupdf as fitz
from PIL import Image

BASE = os.path.dirname(os.path.abspath(__file__))
NAMES = ("folder", "folder2-open", "folder-fill")
SIZES = (16, 32, 48, 256)


def render_svg(name: str, fill: str = "#000000") -> Image.Image:
    svg_path = os.path.join(BASE, f"{name}.svg")
    with open(svg_path, "rb") as f:
        svg = f.read()
    svg = svg.replace(b'fill="currentColor"', f'fill="{fill}"'.encode("ascii"))

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


def png_bytes(im: Image.Image) -> bytes:
    buf = io.BytesIO()
    im.convert("RGBA").save(buf, format="PNG", optimize=True)
    return buf.getvalue()


def write_ico(path: str, images: list[Image.Image]) -> None:
    blobs = [png_bytes(im) for im in images]

    offset = 6 + 16 * len(images)
    entries = bytearray()
    for im, data in zip(images, blobs):
        w, h = im.size
        entry_w = 0 if w >= 256 else w
        entry_h = 0 if h >= 256 else h
        entries += struct.pack(
            "<BBBBHHII",
            entry_w,
            entry_h,
            0,
            0,
            1,
            32,
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
        src = render_svg(name, "#000000")
        images = [src.resize((s, s), Image.Resampling.LANCZOS) for s in SIZES]
        ico_path = os.path.join(BASE, f"{name}.ico")
        write_ico(ico_path, images)
        sizes = read_ico_sizes(ico_path)
        total = sum(sz for _, _, sz in sizes)
        print(
            f"{name}.ico -> {len(sizes)} PNG frames ({total} bytes): "
            + ", ".join(f"{w}x{h}" for w, h, _ in sizes)
        )
        assert [w for w, _, _ in sizes] == list(SIZES), sizes


if __name__ == "__main__":
    main()
