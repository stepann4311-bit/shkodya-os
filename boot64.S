/*
 * boot64.S - Multiboot 1 boot glue for the x86_64 (Long Mode) build.
 *
 * GRUB always hands control over in 32-bit protected mode, so the first part of
 * this file is 32-bit code that builds an identity map and flips the CPU into
 * IA-32e (Long Mode) before jumping to a 64-bit code segment.
 *
 * Transition sequence, in order:
 *   1. identity-map the first 4 GiB with 2 MiB huge pages (PML4 -> PDPT -> PD)
 *   2. CR4.PAE  = 1                     (PAE is required before LME)
 *   3. CR3      = PML4 physical address
 *   4. EFER.LME = 1                     (MSR 0xC0000080, bit 8)
 *   5. CR0.PG   = 1                     (paging on => IA-32e compatibility mode)
 *   6. lgdt + far jump to selector 0x08 (a 64-bit code segment => Long Mode)
 *
 * 4 GiB rather than the 1-2 GiB minimum: QEMU's `-vga std` linear framebuffer
 * lives at 0xFD000000, so a 2 GiB map would fault the first time the compositor
 * touches the screen. Four page directories cost 16 KiB.
 *
 * Provides the external symbols kernel.c declares:
 *   backbuffer[], wallpaper_data[], load_idt,
 *   keyboard_handler_asm, mouse_handler_asm, timer_handler_asm
 *
 * Assemble:  gcc -m64 -c boot64.S -o boot64.o
 * (wallpaper.bin must exist in the current directory for .incbin)
 */

.set MB_MAGIC,    0x1BADB002
.set MB_FLAGS,    0x00000007          /* page-align | mem info | video mode */
.set MB_CHECKSUM, -(MB_MAGIC + MB_FLAGS)

.set EFER_MSR,    0xC0000080
.set EFER_LME,    0x00000100          /* bit 8 */
.set CR4_PAE,     0x00000020          /* bit 5 */
.set CR0_PG,      0x80000000          /* bit 31 */

/* ---- Multiboot header: must sit in the first 8 KiB of the image ---- */
.section .multiboot, "a", @progbits
.align 4
    .long MB_MAGIC
    .long MB_FLAGS
    .long MB_CHECKSUM
    /* a.out kludge fields - unused because header flags bit 16 is clear */
    .long 0
    .long 0
    .long 0
    .long 0
    .long 0
    /* video request: 0 = linear graphics, 1024x768x32 */
    .long 0                            /* mode_type */
    .long 1024                         /* width      */
    .long 768                          /* height     */
    .long 32                           /* depth      */

.section .data
.align 8
/* Multiboot register state, parked here while the CPU changes mode. */
mb_magic:   .long 0
mb_info:    .long 0

.section .text
.code32
.global _start
.type _start, @function
_start:
    cli
    movl %eax, mb_magic                /* multiboot magic */
    movl %ebx, mb_info                 /* multiboot info pointer */
    movl $stack_top, %esp
    movl %esp, %ebp

    /* ---- 1. page tables ------------------------------------------------- */
    movl $pdpt, %eax
    orl  $0x3, %eax                    /* present | writable */
    movl %eax, pml4                    /* PML4[0] -> PDPT */

    movl $pd0, %eax
    orl  $0x3, %eax
    movl %eax, pdpt + 0                /* PDPT[0] -> PD0 (0..1 GiB)   */
    movl $pd1, %eax
    orl  $0x3, %eax
    movl %eax, pdpt + 8                /* PDPT[1] -> PD1 (1..2 GiB)   */
    movl $pd2, %eax
    orl  $0x3, %eax
    movl %eax, pdpt + 16               /* PDPT[2] -> PD2 (2..3 GiB)   */
    movl $pd3, %eax
    orl  $0x3, %eax
    movl %eax, pdpt + 24               /* PDPT[3] -> PD3 (3..4 GiB)   */

    /* pd0..pd3 are contiguous, so one flat loop fills all 2048 huge pages. */
    movl $pd0, %edi
    xorl %ecx, %ecx
fill_pd:
    movl %ecx, %eax
    shll $21, %eax                     /* entry index * 2 MiB */
    orl  $0x83, %eax                   /* present | writable | PS (huge) */
    movl %eax, (%edi, %ecx, 8)
    incl %ecx
    cmpl $2048, %ecx
    jne  fill_pd

    /* ---- 2. PAE ---- */
    movl %cr4, %eax
    orl  $CR4_PAE, %eax
    movl %eax, %cr4

    /* ---- 3. CR3 -> PML4 ---- */
    movl $pml4, %eax
    movl %eax, %cr3

    /* ---- 4. EFER.LME ---- */
    movl $EFER_MSR, %ecx
    rdmsr
    orl  $EFER_LME, %eax
    wrmsr

    /* ---- 5. paging on (enters IA-32e compatibility mode) ---- */
    movl %cr0, %eax
    orl  $CR0_PG, %eax
    orl  $0x1, %eax                    /* keep PE set */
    movl %eax, %cr0

    /* ---- 6. GDT + far jump into 64-bit code ---- */
    lgdt gdt64_desc                    /* 6-byte pointer: base is < 4 GiB */
    ljmp $0x08, $long_mode_entry

/* ---- 64-bit entry point ---- */
.code64
long_mode_entry:
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %ss
    /* fs/gs stay zero; the kernel never uses them */
    movq $stack_top, %rsp
    movq $0, %rbp

    movl mb_magic(%rip), %edi          /* arg 1: multiboot magic */
    movl mb_info(%rip), %esi           /* arg 2: mbi address (always < 4 GiB) */
    call kernel_main

.global hang64
hang64:
    cli
    hlt
    jmp hang64

/* void load_idt(uintptr_t idt_ptr_addr) */
.global load_idt
.type load_idt, @function
load_idt:
    lidt (%rdi)
    ret
.size load_idt, . - load_idt

/* IRQ stubs (64-bit).
 * IRQs push no error code, so nothing needs fixing up before iretq. The C
 * handlers acknowledge the PIC themselves, and the build emits no SSE/FP, so
 * saving the general registers is enough.
 *
 * 15 pushes = 120 bytes, which leaves RSP 16-byte aligned at the `call` when
 * the interrupted stack was aligned - exactly what the SysV ABI expects.
 */
.macro IRQ_STUB64 name, handler
.global \name
.type \name, @function
\name:
    pushq %rax
    pushq %rcx
    pushq %rdx
    pushq %rbx
    pushq %rbp
    pushq %rsi
    pushq %rdi
    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15
    cld
    call \handler
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rdi
    popq %rsi
    popq %rbp
    popq %rbx
    popq %rdx
    popq %rcx
    popq %rax
    iretq
.size \name, . - \name
.endm

IRQ_STUB64 timer_handler_asm,    handle_timer
IRQ_STUB64 keyboard_handler_asm, handle_keyboard
IRQ_STUB64 mouse_handler_asm,    handle_mouse

/* ---- 64-bit GDT: null, code (L=1), data ---- */
.section .rodata
.align 16
gdt64:
    .quad 0x0000000000000000           /* 0x00 null            */
    .quad 0x00AF9A000000FFFF           /* 0x08 code: P|DPL0|S|X|R, G=1, L=1 */
    .quad 0x00CF92000000FFFF           /* 0x10 data: P|DPL0|S|W,   G=1, D=1 */
gdt64_end:
gdt64_desc:
    .word gdt64_end - gdt64 - 1        /* limit */
    .quad gdt64                        /* base  */

/* ---- Desktop wallpaper: raw 1024x768 ARGB, consumed by draw_wallpaper() ---- */
.align 16
.global wallpaper_data
wallpaper_data:
    .incbin "wallpaper.bin"
.size wallpaper_data, . - wallpaper_data

/* ---- Page tables, 4 KiB aligned; the linker keeps them in .bss ---- */
.section .bss
.align 4096
.global pml4
pml4:
    .skip 4096
.align 4096
pdpt:
    .skip 4096
.align 4096
pd0:
    .skip 4096
pd1:
    .skip 4096
pd2:
    .skip 4096
pd3:
    .skip 4096

/* ---- Framebuffer back buffer (1024x768 x 4 bytes) ---- */
.align 16
.global backbuffer
backbuffer:
    .skip 1024 * 768 * 4

.align 16
stack_bottom:
    .skip 65536
stack_top:

/* Mark the stack non-executable so ld does not warn. */
.section .note.GNU-stack, "", @progbits
