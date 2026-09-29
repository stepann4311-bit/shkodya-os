#!/usr/bin/env python3
"""Generate an antialiased 8x16 font for the kernel from a TrueType outline.

    python3 tools/gen_font_aa.py --out font_aa.inc

Why this exists: the kernel's original font is a 1-bit bitmap, one bit per pixel.
Every pixel is either fully painted or absent, so every diagonal and every curve
comes out as a staircase. There is no way to antialias that at draw time - 1 bit
carries no coverage information to blend with.

So the glyphs are re-rendered here from an outline font at 8x the target size and
box-downsampled to the 8x16 cell. Each output pixel then holds real coverage
(0..255), which draw_char alpha-blends into the framebuffer.

The result is written as a C include which is committed like any other asset:
the build needs no Pillow, and the glyphs cannot drift between machines.

Layout of the emitted tables, chosen to match the indexing draw_char already
uses for the bitmap font:

    font_aa_ascii[128][16]   index = byte value 0..127, one byte per row
    font_aa_ru[64][16]       0..47  -> CP866 0x80..0xAF  (A..Ya uppercase, a..p)
                             48..63 -> CP866 0xE0..0xEF  (r..ya lowercase)

The two CP866 ranges are exactly the ones the kernel's font and keyboard layout
use; bytes outside them are not renderable and draw_char skips them.
"""

import argparse
import os
import sys

try:
    from PIL import Image, ImageDraw, ImageFont
except ImportError:
    sys.exit("error: Pillow is required to regenerate the font.\n"
             "       Debian/Ubuntu: sudo apt install python3-pil\n"
             "       Note the build itself does not need it - font_aa.inc is\n"
             "       committed. Only run this when changing the font.")

# The cell the kernel's text layer expects. draw_string advances 8px per
# character and every row of text occupies 16px, so these are not negotiable.
CELL_W, CELL_H = 8, 16
SS = 8                                  # supersample factor before downsampling
BASELINE = 11                           # row the glyph baseline sits on

# Where the reference bitmap font puts its ink, which is what the rest of the
# layout (line pitch, baselines between rows) is already tuned around. Matching
# these keeps text the same size and position as before, only smoother.
TARGETS = {
    "H": (2, 10),                       # capitals: 9 rows tall
    "x": (4, 10),                       # x-height: 7 rows tall
    "g": (4, 13),                       # descender reaches row 13
}


def render(font, ch, em_px):
    """Rasterise one character into a CELL_W x CELL_H coverage bitmap."""
    w, h = CELL_W * SS, CELL_H * SS
    img = Image.new("L", (w, h), 0)
    d = ImageDraw.Draw(img)
    ascent, _descent = font.getmetrics()
    # Centre the advance in the cell: the outline font's advance is a little
    # narrower than the 8px cell the kernel assumes, so shift by half the slack.
    adv = font.getlength(ch)
    x = round((w - adv) / 2.0)
    if x < 0:
        x = 0
    d.text((x, BASELINE * SS - ascent), ch, font=font, fill=255)
    small = img.resize((CELL_W, CELL_H), Image.BOX)
    return small


def ink_rows(small):
    px = small.load()
    rows = [r for r in range(CELL_H)
            if any(px[c, r] > 40 for c in range(CELL_W))]
    return (rows[0], rows[-1]) if rows else (None, None)


def score(font_path, em):
    f = ImageFont.truetype(font_path, int(round(em * SS)))
    total = 0
    for ch, (want_top, want_bot) in TARGETS.items():
        t, b = ink_rows(render(f, ch, em))
        if t is None:
            return 10 ** 6
        total += abs(t - want_top) + abs(b - want_bot)
    return total


def pick_em(font_path, lo=10.0, hi=15.0, step=0.02):
    """Search the point size whose 8x16 rendering matches the reference metrics.

    Done by measurement rather than by reading the font's tables: what matters is
    where the ink lands after downsampling, not the nominal em size.
    """
    best, best_em = 10 ** 6, None
    em = lo
    while em <= hi:
        s = score(font_path, em)
        if s < best:
            best, best_em = s, em
        em += step
    return best_em, best


def emit(path, font_path, em, verbose=True):
    cells = CELL_W * SS
    font = ImageFont.truetype(font_path, int(round(em * SS)))

    def glyph_rows(ch):
        """16 rows, each a list of 8 coverage bytes."""
        small = render(font, ch, em)
        px = small.load()
        return [[px[c, r] for c in range(CELL_W)] for r in range(CELL_H)]

    # --- ASCII 32..126 ----------------------------------------------------- #
    # Control codes and DEL are never drawn, so they are emitted as all-zero
    # rows rather than rendered: it keeps the table the same shape while cutting
    # a quarter of the generated text.
    ascii_glyphs = {}
    for code in range(32, 127):
        ascii_glyphs[code] = glyph_rows(chr(code))

    # --- the two CP866 Cyrillic ranges ------------------------------------- #
    ru_codes = list(range(0x80, 0xB0)) + list(range(0xE0, 0xF0))
    ru_glyphs = []
    for code in ru_codes:
        ch = bytes([code]).decode("cp866", errors="replace")
        ru_glyphs.append(glyph_rows(ch))

    with open(path, "w") as fh:
        fh.write("/* Generated by tools/gen_font_aa.py - do not edit by hand.\n")
        fh.write(" *\n")
        fh.write(" * 8x16 glyphs with 8-bit coverage, rendered from an outline font at\n")
        fh.write(" * %dx and box-downsampled, so draw_char can alpha-blend edges instead\n" % SS)
        fh.write(" * of painting hard pixels. One byte per pixel, one row per line.\n")
        fh.write(" *\n")
        fh.write(" * Source font: %s\n" % os.path.basename(font_path))
        fh.write(" * Em size chosen by measurement: %.2f px (baseline on row %d)\n" % (em, BASELINE))
        fh.write(" */\n\n")

        fh.write("static const uint8_t font_aa_ascii[128][16][8] = {\n")
        for code in sorted(ascii_glyphs):
            fh.write("    [0x%02X] = {  /* %s */\n" % (
                code, chr(code) if code != 39 else "apostrophe"))
            for row in ascii_glyphs[code]:
                fh.write("        {%s},\n" % ",".join("0x%02X" % v for v in row))
            fh.write("    },\n")
        fh.write("};\n\n")

        fh.write("static const uint8_t font_aa_ru[64][16][8] = {\n")
        for i, g in enumerate(ru_glyphs):
            fh.write("    [%2d] = {  /* CP866 0x%02X */\n" % (i, ru_codes[i]))
            for row in g:
                fh.write("        {%s},\n" % ",".join("0x%02X" % v for v in row))
            fh.write("    },\n")
        fh.write("};\n")

    return len(ascii_glyphs), len(ru_glyphs)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default="font_aa.inc")
    ap.add_argument("--font", default="/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf")
    ap.add_argument("--em", type=float, default=None,
                    help="skip the search and use this em size")
    args = ap.parse_args()

    if not os.path.exists(args.font):
        sys.exit("error: font not found: %s\n"
                 "       Try: fc-list | grep -i mono" % args.font)

    em, err = (args.em, None) if args.em else pick_em(args.font)
    if err is not None:
        print("em search: best %.2f px, metric error %d" % (em, err))

    na, nr = emit(args.out, args.font, em)
    # Report the achieved metrics so the numbers are visible, not assumed.
    f = ImageFont.truetype(args.font, int(round(em * SS)))
    print("wrote %s: %d ascii + %d cyrillic glyphs" % (args.out, na, nr))
    for ch, want in TARGETS.items():
        got = ink_rows(render(f, ch, em))
        print("  '%s' ink rows %s (target %s)%s"
              % (ch, got, want, "" if got == want else "   <-- differs"))


if __name__ == "__main__":
    main()
