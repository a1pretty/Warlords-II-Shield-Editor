#!/bin/sh
set -e
CF="-m16 -march=i386 -Os -ffreestanding -fno-pic -fno-pie -nostdlib -fno-asynchronous-unwind-tables -fno-builtin -fno-stack-protector -mno-sse -mno-mmx -fno-jump-tables -Wall -Wno-unused-function -Wno-misleading-indentation -Wno-parentheses"
gcc $CF -c crt0.S -o crt0.o
gcc $CF -c shield.c -o shield.o
ld -m elf_i386 -T com.ld -Map=shield.map -o SHLDEDIT.COM crt0.o shield.o
ls -la SHLDEDIT.COM
grep -E "__bss_(start|end)" shield.map
