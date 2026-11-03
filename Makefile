BUILD_DIR := build
ISO_DIR := $(BUILD_DIR)/iso
KERNEL := $(BUILD_DIR)/blueapple.elf
ISO := $(BUILD_DIR)/blueapple.iso

.PHONY: all clean run iso

all: iso

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/boot.o: kernel/boot.asm | $(BUILD_DIR)
	nasm -f elf32 $< -o $@

$(BUILD_DIR)/kernel.o: kernel/kernel.c | $(BUILD_DIR)
	gcc -m32 -ffreestanding -fno-pie -fno-stack-protector -O2 -Wall -Wextra -c $< -o $@

$(KERNEL): $(BUILD_DIR)/boot.o $(BUILD_DIR)/kernel.o linker.ld
	ld -m elf_i386 -T linker.ld -nostdlib -o $@ $(BUILD_DIR)/boot.o $(BUILD_DIR)/kernel.o

iso: $(KERNEL)
	mkdir -p $(ISO_DIR)/boot/grub
	cp $(KERNEL) $(ISO_DIR)/boot/blueapple.elf
	cp boot/grub/grub.cfg $(ISO_DIR)/boot/grub/grub.cfg
	cp load.ol $(ISO_DIR)/boot/load.ol
	grub-mkrescue -o $(ISO) $(ISO_DIR)

run: iso
	qemu-system-i386 -cdrom $(ISO)

clean:
	rm -rf $(BUILD_DIR)
