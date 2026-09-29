/*
 * demo.c - a Shkodya OS guest application, built into test.exe.
 *
 * Freestanding: no libc, no floating point, no SSE. The kernel loads the flat
 * image at EXEC_BASE (16 MiB) and calls shk_main() with the API table.
 *
 * Build:  see tools/pack_exe.py and the `guest` step in build64.sh
 *
 * Note: the executable format carries no bss_size field, so this program
 * declares no uninitialised globals - everything it needs lives in .data or on
 * the stack. (The loader zeroes a guard span after the image anyway, but not
 * relying on that is cheaper than debugging it.)
 */
#include <stdint.h>
#include "shk_exe.h"

/* The guest paints a full-screen overlay that the compositor blits last, so it
   sits above every window. The composition is therefore pushed below the
   Terminal window (which ends around y = 475) and kept clear of the taskbar
   (which starts at y = 696), so one screenshot shows both the command that
   launched the program and the program's own output. */
#define COLS 16
#define ROWS 10
#define CELL 20          /* 16 * 20 = 320 px wide, 10 * 20 = 200 px tall */
#define PAD_X 40         /* pattern occupies 40..360 x 480..680          */
#define PAD_Y 480
#define TEXT_X 380       /* text column, clear of the pattern            */
#define TEXT_Y 484

/* Deliberately in .data, so it is part of the loaded image. */
static const char banner[]  = "Hello from test.exe";
static const char subtext[] = "Shkodya EXE loader - ring 0 guest";
static const char api_note[] = "API: putpixel draw_string vfs_read sound_beep";
static const uint32_t palette[COLS] = {
    0xFFE06C75, 0xFFD19A66, 0xFFE5C07B, 0xFF98C379, 0xFF56B6C2, 0xFF61AFEF,
    0xFFC678DD, 0xFFF06292, 0xFF64B5F6, 0xFF4DB6AC, 0xFF9CCC65, 0xFFFFB74D,
    0xFFBA68C8, 0xFF90A4AE, 0xFFFFFFFF, 0xFF8FA4C4
};

static void draw_rect(shk_api_t *api, int x0, int y0, int w, int h, uint32_t c) {
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            api->putpixel(x0 + x, y0 + y, c);
        }
    }
}

/* A checkerboard of palette cells with a darker border, so the screenshot
   makes it obvious that guest code really did paint the framebuffer. */
static void draw_pattern(shk_api_t *api, int ox, int oy) {
    for (int r = 0; r < ROWS; r++) {
        for (int c = 0; c < COLS; c++) {
            uint32_t colour = palette[c];
            if (((r + c) & 1) == 0) {
                colour = (colour & 0x00FEFEFEu) >> 1;    /* darker square */
                colour |= 0xFF000000u;
            }
            draw_rect(api, ox + c * CELL, oy + r * CELL, CELL - 2, CELL - 2, colour);
        }
    }
}

/* Exercises vfs_read through the API and reports it on screen.
 *
 * The buffer must be at least as large as the file: fs_read_file() rejects a
 * short destination outright (SHK_EBUFFER) rather than truncating, and
 * readme.txt is 105 bytes. 256 leaves headroom for a longer file. */
#define README_CAP 256
static void readme_line(shk_api_t *api, int x, int y) {
    static uint8_t scratch[README_CAP] = { 0 };   /* initialised => .data */
    int n = api->vfs_read("readme.txt", scratch, sizeof(scratch) - 1);
    if (n > 0) {
        scratch[n] = 0;
        /* show just the first line, which is short enough to fit */
        for (int i = 0; i < n; i++) {
            if (scratch[i] == '\n') { scratch[i] = 0; break; }
        }
        api->draw_string(x, y, "vfs_read(readme.txt) ->", 0xFF6E7681);
        api->draw_string(x, y + 20, (const char *)scratch, 0xFF8FD9B0);
    } else {
        api->draw_string(x, y, "vfs_read(readme.txt) failed", 0xFFE47070);
    }
}

void shk_main(shk_api_t *api) {
    if (!api || api->version != SHK_API_VERSION) {
        return;                      /* nothing safe we can do */
    }

    draw_pattern(api, PAD_X, PAD_Y);

    api->draw_string(TEXT_X, TEXT_Y, banner, 0xFFFFFFFF);
    api->draw_string(TEXT_X, TEXT_Y + 24, subtext, 0xFF8FA4C4);
    readme_line(api, TEXT_X, TEXT_Y + 68);
    api->draw_string(TEXT_X, TEXT_Y + 152, api_note, 0xFF6E7681);

    /* a short rising chirp on the PC speaker */
    api->sound_beep(660, 2);
    api->sound_beep(880, 2);
    api->sound_beep(1320, 3);
}
