/* .COM startup: tiny model, CS=DS=ES=SS. Requires a 386 (gcc -m16 code). */
    .code16
    .section .text.start,"ax"
    .globl _start
_start:
    cld
    movl $0xFFFC, %esp          /* clear upper ESP bits: gcc uses 32-bit stack addressing */
    xorl %ebp, %ebp
    movw $__bss_start, %di
    movw $__bss_end, %cx
    subw %di, %cx
    xorb %al, %al
    rep stosb
    calll main
    movb $0x4C, %ah
    int  $0x21
