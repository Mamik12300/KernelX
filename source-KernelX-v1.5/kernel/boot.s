.set MAGIC, 0x1BADB002
.set FLAGS, 0x00000004
.set CHECKSUM, -(MAGIC + FLAGS)

.section .multiboot,"a"
.align 4
.long MAGIC
.long FLAGS
.long CHECKSUM

# required address fields
.long 0
.long 0
.long 0
.long 0
.long 0

# video mode request
.long 0
.long 1024
.long 768
.long 32

.section .text
.global _start
_start:
    push %ebx
    push %eax
    call kernel_main

hang:
    hlt
    jmp hang
