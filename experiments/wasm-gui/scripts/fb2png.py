#!/usr/bin/env python3
# Convert a raw secure-exec X framebuffer dump (BGRX, header-prefixed; the last W*H*4 bytes are the
# pixels) into a PNG. Used to export visual proof of the wasm desktop into ~/tmp/gui-progress/.
import sys
from PIL import Image

src, dst, W, H = sys.argv[1], sys.argv[2], int(sys.argv[3]), int(sys.argv[4])
data = open(src, "rb").read()
pix = data[len(data) - W * H * 4:]
img = Image.new("RGB", (W, H))
px = img.load()
for y in range(H):
    base = y * W * 4
    for x in range(W):
        i = base + x * 4
        b, g, r = pix[i], pix[i + 1], pix[i + 2]  # BGRX -> RGB
        px[x, y] = (r, g, b)
img.save(dst)
print(f"wrote {dst} ({W}x{H})")
