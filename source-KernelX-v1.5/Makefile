CC = gcc
LD = ld

CFLAGS = -m64 -mcmodel=kernel -ffreestanding -fno-pie -fno-stack-protector -nostdlib -mno-red-zone
LDFLAGS = -m elf_x86_64 -T linker.ld

all: kernelx.bin

kernel.o: kernel/main.c
	$(CC) $(CFLAGS) -c kernel/main.c -o kernel.o

kernelx.bin: kernel.o
	$(LD) $(LDFLAGS) kernel.o -o kernelx.bin

KernelX.img: kernelx.bin
	@if [ ! -f KernelX.img ]; then \
		dd if=/dev/zero of=KernelX.img bs=1M count=64; \
		mkfs.vfat KernelX.img; \
		mmd -i KernelX.img ::/EFI; \
		mmd -i KernelX.img ::/EFI/BOOT; \
		mmd -i KernelX.img ::/boot; \
		mcopy -i KernelX.img BOOTX64.EFI ::/EFI/BOOT/BOOTX64.EFI; \
		mcopy -i KernelX.img limine.conf ::/limine.conf; \
	fi
	mcopy -o -i KernelX.img kernelx.bin ::/boot/kernelx.bin

run: KernelX.img
	qemu-system-x86_64 \
		-drive if=pflash,format=raw,readonly=on,file=/usr/share/edk2/ovmf/OVMF_CODE.fd \
		-drive if=ide,index=0,format=raw,file=KernelX.img

clean:
	rm -rf *.o *.bin iso KernelX.iso
