#!/usr/bin/env bash
#
# run.sh - boot myos64.iso in QEMU.
#
#   ./run.sh              boot with a 512 MiB guest and a 64 MiB disk
#   MEM=1024 ./run.sh     more RAM
#   DISK= ./run.sh        no hard disk at all
#   KVM=0 ./run.sh        force pure emulation
#
# The disk is the guest's ShkodyaFS volume. On first boot the kernel formats it
# and writes readme.txt, todo.txt and test.exe. The system boots fine without
# one - the file manager and `ls` are simply empty.
#
# Overrides: ISO, DISK, MEM, SIZE_MB, KVM, QEMU.

set -euo pipefail
cd "$(dirname "$0")"

ISO=${ISO:-myos64.iso}
DISK=${DISK-disk.img}
MEM=${MEM:-512}
SIZE_MB=${SIZE_MB:-64}
KVM=${KVM:-auto}
QEMU=${QEMU:-qemu-system-x86_64}

die() { printf 'run.sh: error: %s\n' "$*" >&2; exit 1; }

command -v "$QEMU" >/dev/null 2>&1 || die "$QEMU not found.
    Debian/Ubuntu: sudo apt install qemu-system-x86
    Fedora:        sudo dnf install qemu-system-x86
    Arch:          sudo pacman -S qemu-system-x86
    macOS:         brew install qemu"

[ -f "$ISO" ] || die "$ISO not found - build it first with ./build64.sh"

# --- hard disk ------------------------------------------------------------- #
DISK_ARGS=()
if [ -n "$DISK" ]; then
    if [ ! -f "$DISK" ]; then
        printf '[*] creating %s (%s MiB)\n' "$DISK" "$SIZE_MB"
        # qemu-img ships in qemu-utils, not in qemu-system-x86, so a minimal
        # install will not have it. A sparse raw file is all QEMU needs, and
        # truncate is far more likely to be present. Only if both are missing do
        # we give up on the disk - and even then the system boots without one.
        if command -v qemu-img >/dev/null 2>&1; then
            qemu-img create -f raw "$DISK" "${SIZE_MB}M" >/dev/null
        elif command -v truncate >/dev/null 2>&1; then
            truncate -s "${SIZE_MB}M" "$DISK"
        else
            printf '[*] no qemu-img and no truncate: booting without a disk\n' >&2
            DISK=
        fi
    fi
    [ -n "$DISK" ] && DISK_ARGS=(-drive "file=$DISK,format=raw,if=ide,index=0")
fi

# --- accelerator ----------------------------------------------------------- #
# Auto by default: KVM makes a big difference, but it is unavailable inside
# containers and in VMs without nested virtualisation, and passing -accel kvm
# there is a hard failure rather than a fallback.
ACCEL_ARGS=()
case "$KVM" in
    1|yes|on)  ACCEL_ARGS=(-accel kvm) ;;
    0|no|off)  ACCEL_ARGS=() ;;
    auto)
        if [ -r /dev/kvm ] && [ -w /dev/kvm ]; then
            ACCEL_ARGS=(-accel kvm)
            printf '[*] KVM available, using it\n'
        else
            printf '[*] /dev/kvm not usable, falling back to pure emulation\n'
        fi
        ;;
    *) die "KVM must be auto, 0 or 1 (got '$KVM')" ;;
esac

# --- notes ----------------------------------------------------------------- #
# -m 512        : the kernel identity-maps 4 GiB and touches the framebuffer
#                 high in the 4 GiB window; 256 MiB is the practical floor.
# -vga std      : must supply a linear 1024x768x32 VBE mode. -vga cirrus or
#                 -vga none leaves the compositor with no valid framebuffer.
# -serial stdio : the kernel drives COM1 (0x3F8) for its host bridge, so this
#                 is where its output shows up.
# if=ide,index=0: the ATA driver talks to the primary channel only (0x1F0).
#                 A disk on SATA/AHCI is invisible to it.
# -boot d       : the ISO has a BIOS El Torito record only - there is no UEFI
#                 boot image, so do not point firmware or OVMF at it.

printf '[*] %s  %s MiB RAM  disk=%s\n' "$ISO" "$MEM" "${DISK:-none}"

exec "$QEMU" \
    -m "$MEM" \
    -vga std \
    -serial stdio \
    -no-reboot \
    "${ACCEL_ARGS[@]}" \
    "${DISK_ARGS[@]}" \
    -cdrom "$ISO" \
    -boot d
