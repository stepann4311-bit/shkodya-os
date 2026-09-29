/*
 * ai_model.S - embeds the trained Ai Chat model into the kernel image.
 *
 * Same trick as testexe.S, but the payload is architecture independent data, so
 * both the i386 and the x86_64 build include this one file unchanged and share
 * the single artifact produced by tools/train_ai.py.
 *
 * Requires ai/ai_model.bin to exist, i.e. the `ai` step must run before the
 * kernel is assembled. Run it by hand with:
 *
 *     python3 tools/train_ai.py --out ai/ai_model.bin
 *
 * The blob starts with a 64-byte header (magic "AIM1"); kernel.c validates that
 * header, and the section offsets it carries, before touching anything.
 */
.section .rodata
.align 16
.global ai_model_blob
ai_model_blob:
    .incbin "ai/ai_model.bin"
ai_model_blob_end:

.global ai_model_size
ai_model_size:
    .long ai_model_blob_end - ai_model_blob

/* Non-executable stack marker, purely to keep ld quiet. */
.section .note.GNU-stack, "", @progbits
