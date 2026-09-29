/* guest64.ld - link map for Shkodya guest executables.
 *
 * The image is linked at EXEC_BASE (16 MiB), which is exactly where the kernel
 * copies it, so the guest may use plain -fno-pic absolute addressing. Keeping
 * the two in step is what makes a flat binary jumpable.
 *
 * .bss is discarded: a flat binary cannot carry an uninitialised section, and
 * silently packing it would place garbage. The loader zeroes a guard span after
 * the image instead, and guest programs are written to avoid relying on it.
 */
ENTRY(shk_main)

EXEC_BASE = 0x1000000;

SECTIONS
{
    . = EXEC_BASE;

    .text : ALIGN(16) {
        *(.text*)
        *(.rodata*)
    }

    .data : ALIGN(16) {
        *(.data*)
    }

    /* Allocated but NOT part of the flat image (a flat binary cannot carry a
       NOBITS section). It therefore lands immediately after the loaded image,
       which is exactly the span the loader zeroes before calling in. */
    .bss : ALIGN(16) {
        *(.bss*)
        *(COMMON)
    }

    /DISCARD/ : {
        *(.eh_frame)
        *(.eh_frame_hdr)
        *(.comment)
        *(.note*)
        *(.rela*)
        *(.rel.*)
    }
}
