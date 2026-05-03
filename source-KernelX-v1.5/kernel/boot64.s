.set MAGIC, 0x1BADB002
.set FLAGS, 0x00000007
.set CHECKSUM, -(MAGIC + FLAGS)

.section .multiboot,"a"
.align 4
.long MAGIC
.long FLAGS
.long CHECKSUM
.long 0
.long 0
.long 0
.long 0
.long 0
.long 0
.long 1024
.long 768
.long 32

.section .bss
.align 4096
pml4: .skip 4096
pdpt: .skip 4096
pds:  .skip 4096 * 4

.align 16
stack_bottom:
.skip 32768
stack_top:

.section .text
.global _start
.extern kernel_main

.code32
_start:
    mov $stack_top, %esp
    mov %eax, %edi
    mov %ebx, %esi

    mov $0, %ecx
.setup_pdpt:
    mov $pds, %eax
    mov %ecx, %edx
    shl $12, %edx
    add %edx, %eax
    or $0x03, %eax
    mov %eax, pdpt(,%ecx,8)
    inc %ecx
    cmp $4, %ecx
    jne .setup_pdpt

    mov $0, %ecx
.map_4gb:
    mov %ecx, %eax
    shl $21, %eax
    or $0x83, %eax
    mov %eax, pds(,%ecx,8)
    inc %ecx
    cmp $2048, %ecx
    jne .map_4gb

    mov $pdpt, %eax
    or $0x03, %eax
    mov %eax, pml4

    mov $pml4, %eax
    mov %eax, %cr3

    mov %cr4, %eax
    or $0x20, %eax
    mov %eax, %cr4

    mov $0xC0000080, %ecx
    rdmsr
    or $0x100, %eax
    wrmsr

    lgdt gdt64_ptr

    mov %cr0, %eax
    or $0x80000001, %eax
    mov %eax, %cr0

    ljmp $0x08, $long_mode

.code64
long_mode:
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %ss
    mov $stack_top, %rsp
    call kernel_main

.hang:
    cli
    hlt
    jmp .hang

.align 8
gdt64:
    .quad 0
    .quad 0x00AF9A000000FFFF
    .quad 0x00AF92000000FFFF
gdt64_end:

gdt64_ptr:
    .word gdt64_end - gdt64 - 1
    .quad gdt64
