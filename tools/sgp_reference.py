#!/usr/bin/env python3
"""Independent reference for JA2SGPGraphicsSelfTest (retro68/ja2_sgp_selftest.c).

Decodes the same STCI images straight from the SLF archives, draws the same
scene into a 640x480 RGB555 frame buffer as the SGP blitters should, and
prints the FNV-1a checksum the self-test reports. Optionally writes a PNG.

    sgp_reference.py JA2_DATA_DIR [out.png] [--compare host_fb.raw]
"""

import os
import struct
import sys
import zlib

W, H = 640, 480


def slf_entry(data_dir, archive, name):
    for f in os.listdir(data_dir):
        if f.lower() == archive.lower():
            data = open(os.path.join(data_dir, f), "rb").read()
            break
    count = struct.unpack_from("<i", data, 512)[0]
    base = len(data) - count * 280
    for i in range(count):
        e = data[base + i * 280: base + (i + 1) * 280]
        entry = e[:256].split(b"\0")[0].decode("latin-1")
        off, length, state = struct.unpack_from("<IIB", e, 256)
        # Entries are stored relative to the library's own path ("LOADSCREENS\\").
        if state == 0 and entry.lower() == name.lower().replace("\\", "/").split("/")[-1]:
            return data[off:off + length]
    raise KeyError(name)


def load_sti(blob):
    assert blob[:4] == b"STCI"
    stored, flags = struct.unpack_from("<I", blob, 8)[0], struct.unpack_from("<I", blob, 16)[0]
    colours, subimages = struct.unpack_from("<IH", blob, 24)
    assert flags & 0x08 and flags & 0x20 and colours == 256
    pal = blob[64:64 + 768]
    table = 64 + 768
    subs = [struct.unpack_from("<IIhhHH", blob, table + i * 16) for i in range(subimages)]
    pixels = blob[table + subimages * 16: table + subimages * 16 + stored]
    palette = []
    for i in range(256):
        r, g, b = pal[i * 3], pal[i * 3 + 1], pal[i * 3 + 2]
        c = ((r << 7) & 0x7C00) | ((g << 2) & 0x03E0) | ((b >> 3) & 0x001F)
        if c == 0 and r + g + b != 0:
            c = 0x0001
        palette.append(c)
    return palette, subs, pixels


def blit(fb, sti, index, x, y):
    palette, subs, pixels = sti
    offset, _, ox, oy, height, width = subs[index]
    x += ox
    y += oy
    pos = offset
    row = col = 0
    while row < height:
        n = pixels[pos]
        pos += 1
        if n & 0x80:
            col += n & 0x7F
        elif n == 0:
            row += 1
            col = 0
        else:
            for k in range(n):
                px, py = x + col + k, y + row
                if 0 <= px < W and 0 <= py < H:
                    fb[py * W + px] = palette[pixels[pos + k]]
            pos += n
            col += n


def main():
    data_dir = os.path.join(sys.argv[1], "Data")
    fb = [0] * (W * H)
    background = load_sti(slf_entry(data_dir, "LoadScreens.slf", "LOADSCREENS\\MAINMENUBACKGROUND.STI"))
    help_screen = load_sti(slf_entry(data_dir, "Interface.slf", "INTERFACE\\HelpScreen.sti"))
    blit(fb, background, 0, 0, 0)
    blit(fb, help_screen, 8, 100, 60)
    blit(fb, help_screen, 9, 560, 300)
    for py in range(400, 440):
        for px in range(20, 120):
            fb[py * W + px] = 0x7C00
    h = 2166136261
    for v in fb:
        h = ((h ^ (v >> 8)) * 16777619) & 0xFFFFFFFF
        h = ((h ^ (v & 0xFF)) * 16777619) & 0xFFFFFFFF
    print(f"reference checksum {h:08x}")
    if "--compare" in sys.argv:
        raw = open(sys.argv[sys.argv.index("--compare") + 1], "rb").read()
        other = struct.unpack("<%dH" % (W * H), raw)
        diff = [i for i in range(W * H) if other[i] != fb[i]]
        print(f"{len(diff)} pixels differ")
        if diff:
            xs = [i % W for i in diff]
            ys = [i // W for i in diff]
            print(f"  bounds x {min(xs)}..{max(xs)} y {min(ys)}..{max(ys)}")
            for i in diff[:5]:
                print(f"  ({i % W},{i // W}) sgp {other[i]:04x} reference {fb[i]:04x}")
    if len(sys.argv) > 2 and not sys.argv[2].startswith("--"):
        raw = b"".join(b"\0" + bytes(
            c for v in fb[r * W:(r + 1) * W]
            for c in (((v >> 10) & 31) << 3, ((v >> 5) & 31) << 3, (v & 31) << 3))
            for r in range(H))
        def chunk(t, d):
            return struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d))
        png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", W, H, 8, 2, 0, 0, 0)) \
            + chunk(b"IDAT", zlib.compress(raw)) + chunk(b"IEND", b"")
        open(sys.argv[2], "wb").write(png)


if __name__ == "__main__":
    main()
