#!/bin/bash
set -e

as --64 kernel/boot64.s -o boot64.o

gcc -m64 -mcmodel=large -ffreestanding -fno-pie \
  -fno-stack-protector -nostdlib -mno-red-zone \
  -c kernel/main.c -o kernel.o

ld -m elf_x86_64 -T linker_grub.ld boot64.o kernel.o -o kernelx.bin

echo "Built kernelx.bin"
