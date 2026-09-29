#!/usr/bin/env bash
#
# build64.sh - build the x86_64 (Long Mode) Shkodya OS image.
#
#   ./build64.sh
#
# Produces myos64.bin (Multiboot 1 ELF) and myos64.iso (BIOS-bootable CD image).
#
# Run it from the repository root: boot64.S uses .incbin "wallpaper.bin", and
# the assembler resolves that against the current directory.
#
# The build has eight stages because the image is self-contained: the desktop
# wallpaper, the demo guest executable and the Ai Chat model are all generated
# and then embedded in the kernel as raw data. Nothing is loaded from disk at
# runtime except the filesystem the guest itself creates.
#
# Environment overrides: CC, CMODEL, OUT, ISO, JOBS.

set -euo pipefail
cd "$(dirname "$0")"

CC=${CC:-gcc}
LD=${LD:-ld}
CMODEL=${CMODEL:-small}
OUT=${OUT:-myos64.bin}
ISO=${ISO:-myos64.iso}
STAGE=isodir

# --------------------------------------------------------------------------- #
# helpers
# --------------------------------------------------------------------------- #

die() { printf 'build64.sh: error: %s\n' "$*" >&2; exit 1; }
have() { command -v "$1" >/dev/null 2>&1; }
step() { printf '\n[%s] %s\n' "$1" "$2"; }

# --------------------------------------------------------------------------- #
# preflight
# --------------------------------------------------------------------------- #
# Checked up front rather than discovered halfway through a build.

for tool in "$CC" "$LD" python3 objcopy readelf grub-mkrescue xorriso; do
    have "$tool" || die "$tool not found. On Debian/Ubuntu:
    sudo apt install build-essential python3 binutils grub-pc-bin grub-common xorriso mtools"
done

for src in kernel.c boot64.S linker64.ld testexe.S ai_model.S \
           font_aa.inc grub.cfg gen_wallpaper.py \
           guest/demo.c guest/shk_exe.h guest64.ld \
           tools/pack_exe.py tools/train_ai.py \
           ai/corpus_qa.txt ai/corpus_chat.txt; do
    [ -f "$src" ] || die "required source file missing: $src"
done

# --------------------------------------------------------------------------- #
# flags
# --------------------------------------------------------------------------- #
# -O2 : without any -O flag GCC compiles at -O0, and this kernel is full of hot
#       inner loops (per-frame blits, string walks, the model's dot products).
#       Measured on a fixed Ai Chat workload this alone is worth ~2.5x.
# -fno-strict-aliasing : the kernel does its own type punning over raw memory
#       (page tables, framebuffer, the embedded model blob). Strict aliasing is
#       the one -O2 default that can silently miscompile that kind of code.
# -mno-red-zone : an interrupt would otherwise clobber the red zone below RSP.
# -mno-sse/-mno-mmx/-mno-80387 : the kernel is integer-only, so nothing needs
#       CR4.OSFXSR and no misaligned SSE spill can fault.
# -fno-pie/-fno-pic : a freestanding kernel at a fixed address has no loader to
#       apply GOT relocations.
# -mcmodel=small : NOT kernel. -mcmodel=kernel assumes the kernel sits in the
#       negative 2 GiB (0xFFFF8000_00000000+); GRUB loads us at 1 MiB, so that
#       model emits relocations which cannot resolve.
CFLAGS="-m64 -O2 -fno-strict-aliasing -ffreestanding -fno-builtin \
        -fno-stack-protector -nostdlib \
        -Wall -Wextra -mno-red-zone -mcmodel=${CMODEL} \
        -fno-pie -fno-pic -fno-asynchronous-unwind-tables \
        -mno-sse -mno-sse2 -mno-mmx -mno-80387"

# Same treatment for the guest: it is real dispatchable code, so it should be
# built the way a hand-written .exe would be.
GCFLAGS="-m64 -O2 -fno-strict-aliasing -ffreestanding -fno-builtin \
         -fno-stack-protector -nostdlib \
         -Wall -Wextra -mno-red-zone -fno-pie -fno-pic \
         -fno-asynchronous-unwind-tables -mno-sse -mno-sse2 -mno-mmx -mno-80387"

mkdir -p build64 "$STAGE/boot/grub"

# --------------------------------------------------------------------------- #
step 1/8 "desktop wallpaper"
# 1024x768 ARGB, embedded into the kernel by boot64.S.
python3 gen_wallpaper.py || die "wallpaper generation failed"
[ -f wallpaper.bin ] || die "gen_wallpaper.py did not produce wallpaper.bin"

# --------------------------------------------------------------------------- #
step 2/8 "demo guest executable -> build64/test.exe"
# A freestanding flat binary linked at 0x1000000, which is where the kernel's
# Shkodya EXE loader copies it. pack_exe.py reads the entry symbol out of the
# ELF symbol table so moving the entry point cannot silently produce an .exe
# that jumps into the middle of a function.
$CC $GCFLAGS -Iguest -c guest/demo.c -o build64/demo.o \
    || die "guest/demo.c failed to compile"
$LD -m elf_x86_64 -T guest64.ld -nostdlib build64/demo.o -o build64/demo.elf \
    || die "guest link failed"
python3 tools/pack_exe.py --elf build64/demo.elf --out build64/test.exe \
        --symbol shk_main --base 0x1000000 || die "packing test.exe failed"

# --------------------------------------------------------------------------- #
step 3/8 "Ai Chat model -> ai/ai_model.bin"
# Trains two bag-of-words retrievers over the corpora and writes the weights,
# the corpus text and the vocabulary. Also emits ai/stopwords.inc, which
# kernel.c includes, so the trainer's tokeniser and the kernel's cannot drift.
# Architecture independent: both build scripts embed the same file.
python3 tools/train_ai.py --out ai/ai_model.bin || die "model training failed"
[ -f ai/ai_model.bin ] || die "train_ai.py did not produce ai/ai_model.bin"

# --------------------------------------------------------------------------- #
step 4/8 "embed the .exe and the model as kernel data"
# The outer single quotes survive into gcc's argv as literal double quotes, so
# cpp defines EXE_PATH as the string literal "build64/test.exe" that .incbin
# needs. Without them the assembler sees an unquoted path and fails.
$CC -m64 -DEXE_PATH='"build64/test.exe"' -c testexe.S -o build64/testexe.o \
    || die "assembling testexe.S failed"
$CC -m64 -c ai_model.S -o build64/ai_model.o \
    || die "assembling ai_model.S failed"

# --------------------------------------------------------------------------- #
step 5/8 "kernel.o"
$CC $CFLAGS -c kernel.c -o build64/kernel64.o || die "kernel.c failed to compile"

# --------------------------------------------------------------------------- #
step 6/8 "boot64.o"
$CC -m64 -c boot64.S -o build64/boot64.o || die "assembling boot64.S failed"

# --------------------------------------------------------------------------- #
step 7/8 "link $OUT"
$LD -m elf_x86_64 -T linker64.ld -nostdlib -o "$OUT" \
    build64/boot64.o build64/kernel64.o \
    build64/testexe.o build64/ai_model.o || die "link failed"
[ -f "$OUT" ] || die "$OUT was not produced"
ls -l "$OUT"

# Verify GRUB will actually accept the image before burning an ISO around it.
if have grub-file; then
    grub-file --is-x86-multiboot "$OUT" \
        || die "$OUT has no valid Multiboot header in its first 8 KiB"
fi

# --------------------------------------------------------------------------- #
step 8/8 "$ISO"
mkdir -p "$STAGE/boot/grub"
cp "$OUT" "$STAGE/boot/$OUT"
cp grub.cfg "$STAGE/boot/grub/grub.cfg"

# No --quiet flag: not every grub-mkrescue has one, and it is not worth
# depending on. Capture the output instead and only print it when it failed,
# so a successful build stays readable without hiding a real error.
if ! mkrescue_log=$(grub-mkrescue -o "$ISO" "$STAGE" 2>&1); then
    printf '%s\n' "$mkrescue_log" >&2
    die "grub-mkrescue failed"
fi
[ -f "$ISO" ] || die "$ISO was not produced"
ls -l "$ISO"

printf '\nOK  %s + %s\n' "$OUT" "$ISO"
printf 'Run it with:  ./run.sh\n'
