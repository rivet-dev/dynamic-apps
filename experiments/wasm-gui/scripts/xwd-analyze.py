#!/usr/bin/env python3
# Parse an XWD (X Window Dump) framebuffer — what Xvfb.wasm writes via -fbdir — to a PNG, and report
# render coverage (non-black pixels, distinct colors, a bottom-strip "panel" region, a center "window"
# region). Used to verify the M8.x GUI proofs without needing ImageMagick.
#   usage: xwd-analyze.py <xwd-file> [out.png]
import struct, sys
from PIL import Image

src = sys.argv[1]
dst = sys.argv[2] if len(sys.argv) > 2 else (src.rsplit(".", 1)[0] + ".real.png")
data = open(src, "rb").read()
hdr = struct.unpack(">25I", data[:100])
header_size, ver, pixfmt, depth, W, H = hdr[0], hdr[1], hdr[2], hdr[3], hdr[4], hdr[5]
byte_order, bpp, bpl = hdr[7], hdr[11], hdr[12]
rmask, gmask, bmask, ncolors = hdr[14], hdr[15], hdr[16], hdr[19]


def shift(m):
    s = 0
    while m and not (m & 1):
        m >>= 1
        s += 1
    return s


rs, gs, bs = shift(rmask), shift(gmask), shift(bmask)
off = header_size + ncolors * 12
img = Image.new("RGB", (W, H))
px = img.load()
step = bpp // 8
for y in range(H):
    row = off + y * bpl
    for x in range(W):
        p = row + x * step
        val = int.from_bytes(data[p:p + step], "little" if byte_order == 0 else "big")
        px[x, y] = ((val & rmask) >> rs, (val & gmask) >> gs, (val & bmask) >> bs)
img.save(dst)

nb = sum(1 for y in range(H) for x in range(W) if px[x, y] != (0, 0, 0))
colors = len(set(px[x, y] for y in range(H) for x in range(W)))
panel = sum(1 for y in range(H - 26, H) for x in range(W) if px[x, y] != (0, 0, 0))
cen = sum(1 for y in range(H // 5, 4 * H // 5) for x in range(W // 5, 4 * W // 5) if px[x, y] != (0, 0, 0))
print(f"{src} -> {dst}")
print(f"  {W}x{H} depth={depth} bpp={bpp} ver={ver}")
print(f"  nonblack={nb} ({100*nb//(W*H)}%) colors={colors}")
print(f"  panel-strip(bottom26)={panel}/{26*W} ({100*panel//(26*W)}%)  center-window-band={cen}")
