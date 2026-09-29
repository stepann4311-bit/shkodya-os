/*
 * shk_exe.h - the Shkodya EXE contract, shared by the kernel loader and by
 * guest programs so the two can never disagree about a field offset.
 *
 * This header deliberately does not include <stdint.h>: the kernel defines its
 * own uint8_t/uint32_t typedefs, guests get them from <stdint.h>. Include
 * exactly one of those first.
 *
 * On-disk / in-memory layout of a .exe:
 *
 *      +---------------------------+ 0
 *      | shk_exe_header_t (16 B)   |
 *      +---------------------------+ 16
 *      | .text   (text_size bytes) |   <- entry_offset points in here
 *      +---------------------------+
 *      | .rodata + .data           |
 *      | (data_size bytes)         |
 *      +---------------------------+
 */
#ifndef SHK_EXE_H
#define SHK_EXE_H

/* "SHKE" plus the legacy DOS-style "MZ\0\0"; both are accepted. */
#define SHK_EXE_MAGIC_SHK  "SHKE"
#define SHK_EXE_MAGIC_MZ   "MZ"

#define SHK_EXE_HEADER_SIZE 16u

typedef struct __attribute__((packed)) {
    uint8_t  magic[4];       /* "SHKE", or "MZ\0\0"                */
    uint32_t text_size;      /* bytes of code section              */
    uint32_t data_size;      /* bytes of data section              */
    uint32_t entry_offset;   /* entry point, offset from the header */
} shk_exe_header_t;

/* ---- the API table handed to the guest entry point ---- */

#define SHK_API_VERSION 1u

typedef struct {
    uint32_t version;        /* SHK_API_VERSION                        */
    uint32_t size;           /* sizeof(shk_api_t): guests can check it  */
    uint32_t screen_width;   /* back buffer dimensions, so a guest can  */
    uint32_t screen_height;  /* clip its own drawing                    */

    void (*putpixel)(int x, int y, uint32_t color);
    void (*draw_string)(int x, int y, const char *s, uint32_t color);
    int  (*vfs_read)(const char *name, uint8_t *dst, uint32_t capacity);
    void (*sound_beep)(uint32_t freq_hz, uint32_t ticks);
} shk_api_t;

/* Guest entry point: the loader jumps here with a pointer to the API table. */
typedef void (*shk_entry_t)(shk_api_t *api);

#endif /* SHK_EXE_H */
