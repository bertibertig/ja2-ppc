#!/usr/bin/env python3
"""Compare port screenshots with Windows reference screenshots (winref).

    compare_screens.py WINDOWS.png MAC.png OUT_PREFIX

Both are 640x480. Colours are compared at 5 bits per channel (the port
renders RGB555; the reference runs in 16-bit 565 under Wine), with a
tolerance of one 5-bit step per channel. Writes OUT_PREFIX-side.png
(Windows | Mac | difference mask, differing pixels in magenta) and prints
the share of matching pixels plus the bounding boxes of differing areas.
"""

import sys

import numpy as np
from PIL import Image


def load(path):
    image = Image.open(path).convert("RGB")
    if image.size != (640, 480):
        image = image.resize((640, 480), Image.NEAREST)
    return np.asarray(image, dtype=np.int16)


def regions(mask, cell=32):
    """Coarse grid cells (32x32) that contain differences, merged per row."""
    found = []
    for y in range(0, 480, cell):
        row = [x for x in range(0, 640, cell) if mask[y:y + cell, x:x + cell].mean() > 0.05]
        if row:
            found.append((y, row[0], row[-1] + cell))
    return found


def main():
    win, mac, prefix = sys.argv[1], sys.argv[2], sys.argv[3]
    a = load(win) >> 3
    b = load(mac) >> 3
    diff = np.abs(a - b).max(axis=2) > 1
    share = 100.0 * (1.0 - diff.mean())
    print(f"{prefix}: {share:.1f}% of pixels match")
    for y, x0, x1 in regions(diff):
        print(f"  differs in rows {y}-{y + 31}, x {x0}-{x1}")
    side = np.zeros((480, 1920, 3), dtype=np.uint8)
    side[:, :640] = load(win)
    side[:, 640:1280] = load(mac)
    mask = (load(mac) // 3).astype(np.uint8)
    mask[diff] = (255, 0, 255)
    side[:, 1280:] = mask
    Image.fromarray(side).save(prefix + "-side.png")


if __name__ == "__main__":
    main()
