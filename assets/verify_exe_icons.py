"""List RT_ICON sizes embedded in ToFolder.exe."""

from __future__ import annotations

import struct
import sys


def u16(data: bytes, off: int) -> int:
    return struct.unpack_from("<H", data, off)[0]


def u32(data: bytes, off: int) -> int:
    return struct.unpack_from("<I", data, off)[0]


def main() -> int:
    path = sys.argv[1] if len(sys.argv) > 1 else r"..\out\ToFolder.exe"
    with open(path, "rb") as f:
        pe = f.read()

    if pe[:2] != b"MZ":
        print("not a PE")
        return 1
    e_lfanew = u32(pe, 0x3C)
    if pe[e_lfanew : e_lfanew + 4] != b"PE\0\0":
        print("bad PE")
        return 1

    # COFF + optional header
    coff = e_lfanew + 4
    num_sections = u16(pe, coff + 2)
    size_opt = u16(pe, coff + 16)
    opt = coff + 20
    magic = u16(pe, opt)
    # DataDirectory starts at opt+112 (PE32+) or opt+96 (PE32)
    dd = opt + (112 if magic == 0x20B else 96)
    res_rva = u32(pe, dd + 2 * 8)
    if res_rva == 0:
        print("no resources")
        return 1

    # section table
    sec = opt + size_opt
    rva_to_off = None
    for i in range(num_sections):
        s = sec + i * 40
        virt = u32(pe, s + 12)
        raw_size = u32(pe, s + 16)
        raw_ptr = u32(pe, s + 20)
        virt_size = u32(pe, s + 8)
        size = max(virt_size, raw_size)
        if virt <= res_rva < virt + size:
            rva_to_off = lambda rva, v=virt, p=raw_ptr: p + (rva - v)
            break
    if rva_to_off is None:
        print("resource section not found")
        return 1

    root = rva_to_off(res_rva)

    def walk_dir(offset: int, level: int = 0, path: list = None):
        path = path or []
        named = u16(pe, offset + 12)
        ids = u16(pe, offset + 14)
        entries = named + ids
        base = offset + 16
        for i in range(entries):
            e = base + i * 8
            name_or_id = u32(pe, e)
            data_or_subdir = u32(pe, e + 4)
            is_dir = (data_or_subdir & 0x80000000) != 0
            nid = name_or_id & 0x7FFFFFFF
            child = root + (data_or_subdir & 0x7FFFFFFF)
            new_path = path + [nid]
            if is_dir:
                yield from walk_dir(child, level + 1, new_path)
            else:
                yield new_path, child

    # Type 3 = RT_ICON, Type 14 = RT_GROUP_ICON
    icons = []
    groups = []
    for pth, data_entry in walk_dir(root):
        if len(pth) < 1:
            continue
        rva = u32(pe, data_entry)
        size = u32(pe, data_entry + 4)
        off = rva_to_off(rva)
        blob = pe[off : off + size]
        if pth[0] == 3:  # RT_ICON
            # BMP header or PNG
            if blob[:8] == b"\x89PNG\r\n\x1a\n":
                # IHDR
                w = struct.unpack(">I", blob[16:20])[0]
                h = struct.unpack(">I", blob[20:24])[0]
            elif len(blob) >= 40:
                w = u32(blob, 4)
                h = u32(blob, 8) // 2
            else:
                w = h = 0
            icons.append((pth[1] if len(pth) > 1 else 0, w, h, size))
        elif pth[0] == 14:  # GROUP
            groups.append((pth[1] if len(pth) > 1 else 0, blob))

    print(f"EXE: {path}")
    print(f"RT_ICON images: {len(icons)}")
    for iid, w, h, size in sorted(icons):
        print(f"  icon#{iid}: {w}x{h} ({size} bytes)")

    print(f"RT_GROUP_ICON: {len(groups)}")
    for gid, blob in groups:
        if len(blob) < 6:
            continue
        _r, _t, count = struct.unpack_from("<HHH", blob, 0)
        print(f"  group#{gid}: {count} entries")
        for i in range(count):
            o = 6 + i * 14
            if o + 14 > len(blob):
                break
            w, h, _cl, _res, planes, bits, bytes_in_res, id_ = struct.unpack_from("<BBBBHHIH", blob, o)
            print(f"    {w or 256}x{h or 256} id={id_} bits={bits} bytes={bytes_in_res}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
