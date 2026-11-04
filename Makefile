BUILD_DIR := build
ISO_DIR := $(BUILD_DIR)/iso
KERNEL := $(BUILD_DIR)/blueapple.elf
ISO := $(BUILD_DIR)/blueapple.iso

.PHONY: all clean run local iso

all: iso

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/boot.o: kernel/boot.asm | $(BUILD_DIR)
	nasm -f elf32 $< -o $@

$(BUILD_DIR)/kernel.o: kernel/kernel.c kernel/e1000.h | $(BUILD_DIR)
	gcc -m32 -ffreestanding -fno-pie -fno-stack-protector -O2 -Wall -Wextra -c $< -o $@

$(BUILD_DIR)/e1000.o: kernel/e1000.c kernel/e1000.h | $(BUILD_DIR)
	gcc -m32 -ffreestanding -fno-pie -fno-stack-protector -O2 -Wall -Wextra -c $< -o $@

$(KERNEL): $(BUILD_DIR)/boot.o $(BUILD_DIR)/kernel.o $(BUILD_DIR)/e1000.o linker.ld
	ld -m elf_i386 -T linker.ld -nostdlib -o $@ $(BUILD_DIR)/boot.o $(BUILD_DIR)/kernel.o $(BUILD_DIR)/e1000.o

iso: $(KERNEL)
	mkdir -p $(ISO_DIR)/boot/grub
	cp $(KERNEL) $(ISO_DIR)/boot/blueapple.elf
	cp boot/grub/grub.cfg $(ISO_DIR)/boot/grub/grub.cfg
	cp load.ol $(ISO_DIR)/boot/load.ol
	grub-mkrescue -o $(ISO) $(ISO_DIR)

run: iso
	qemu-system-i386 -netdev user,id=ba_net -device e1000,netdev=ba_net -cdrom $(ISO)

# Default developer workflow: build and run BlueApple in local LO mode.
local: iso
	qemu-system-i386 -cdrom $(ISO)

clean:
	rm -rf $(BUILD_DIR)
