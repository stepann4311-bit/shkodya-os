/*
 * testexe.S - embeds the packed demo executable into the kernel image.
 *
 * The kernel writes this blob to ShkodyaFS the first time it formats a disk, so
 * `exec test.exe` works on a fresh disk with no upload step.
 *
 * The blob path comes from -DEXE_PATH=... so the same file serves both
 * architectures: the 64-bit build embeds build64/test.exe, the 32-bit build
 * embeds build32/test.exe. That path must exist before the kernel is assembled,
 * i.e. the guest step runs first in build.sh / build64.sh.
 *
 * (.S, not .s, so the C preprocessor expands EXE_PATH before `.incbin` sees it.)
 */
#ifndef EXE_PATH
#define EXE_PATH "build64/test.exe"
#endif

.section .rodata
.align 16
.global test_exe_blob
test_exe_blob:
    .incbin EXE_PATH
test_exe_blob_end:

.global test_exe_size
test_exe_size:
    .long test_exe_blob_end - test_exe_blob

/* Non-executable stack marker, purely to keep ld quiet. */
.section .note.GNU-stack, "", @progbits
