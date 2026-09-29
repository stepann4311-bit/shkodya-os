#!/usr/bin/env python3
"""Generate wallpaper.bin: a 1024x768 ARGB gradient for Shkodya OS.

The kernel does `memcpy(backbuffer, wallpaper_data, fb_width*fb_height*4)`,
treating the buffer as uint32 0xAARRGGBB. On little-endian x86 that means
each pixel is stored as the byte sequence B, G, R, A - which is exactly the
BGRX layout of the 32bpp VBE framebuffer, so no channel swap is needed.

This is a PLACEHOLDER desktop background: the taskbar, every desktop icon and
all application windows are still drawn by the real kernel code.
"""

import os

W, H = 1024, 768

# Dark blue diagonal gradient, in the palette family of the Midnight theme
# (C_BLACK 0xFF070B14 -> C_SURFACE 0xFF1C2233).
R0, G0, B0 = 0x18, 0x1E, 0x33     # top-left
R1, G1, B1 = 0x05, 0x08, 0x10     # bottom-right


def main():
    out = bytearray(W * H * 4)
    i = 0
    for y in range(H):
        ty = y / (H - 1)
        base_r = R0 + (R1 - R0) * ty
        base_g = G0 + (G1 - G0) * ty
        base_b = B0 + (B1 - B0) * ty
        for x in range(W):
            t = (x / (W - 1)) * 0.45 + ty * 0.55
            r = int(base_r + (R1 - base_r) * t * 0.6)
            g = int(base_g + (G1 - base_g) * t * 0.6)
            b = int(base_b + (B1 - base_b) * t * 0.6)
            out[i] = b
            out[i + 1] = g
            out[i + 2] = r
            out[i + 3] = 0xFF
            i += 4
    with open("wallpaper.bin", "wb") as fh:
        fh.write(out)
    print("wallpaper.bin: %d bytes (%dx%d ARGB) = %d" %
          (len(out), W, H, os.path.getsize("wallpaper.bin")))


if __name__ == "__main__":
    main()
