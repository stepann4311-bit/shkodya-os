/* linker64.ld - Shkodya OS x86_64 (Long Mode), Multiboot 1 ELF at 1 MiB.
 *
 * The 32-bit map is reused almost unchanged: Multiboot 1 still requires the
 * header inside the first 8 KiB, and GRUB loads the image at 1 MiB. The only
 * additions are the section discards that GCC emits for x86_64 but not i386
 * (.note.gnu.property, .rela.*, .eh_frame_hdr), which would otherwise land as
 * orphan sections and bloat the image.
 */
ENTRY(_start)

SECTIONS
{
    . = 1M;

    /* Multiboot header first: GRUB scans the first 8 KiB of the image. */
    .multiboot : {
        KEEP(*(.multiboot))
    }

    .text : ALIGN(16) {
        *(.text*)
    }

    .rodata : ALIGN(16) {
        *(.rodata*)
    }

    .data : ALIGN(16) {
        *(.data*)
    }

    /* Page tables and the framebuffer live here; must stay 4 KiB aligned. */
    .bss : ALIGN(4096) {
        *(COMMON)
        *(.bss*)
    }

    /DISCARD/ : {
        *(.eh_frame)
        *(.eh_frame_hdr)
        *(.comment)
        *(.note*)
        *(.rela*)
        *(.rel.*)
        *(.got*)
        *(.plt*)
    }
}
