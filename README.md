# BlueApple OS

BlueApple is an experimental PC operating system project. The goal is a graphical desktop OS that can boot from a USB drive in **Live Mode** without replacing the computer's existing operating system.

## Current milestone

The repository now contains the first Multiboot 1 kernel scaffold. When built and booted in an emulator or from a bootable image, it displays a simple text confirmation. This is a boot test, not the finished desktop.

## Safety design

- The initial USB experience is Live Mode.
- BlueApple must not format, repartition, or install to the internal drive automatically.
- Users should select the USB from their computer's one-time boot menu.
- To return to the previous OS, shut down, remove the USB, and start the computer again. This relies on the firmware returning to its normal internal boot device; users should avoid changing permanent boot order settings.
- Installation to an internal drive, if ever added, must be a separate, explicit feature with clear warnings.

## Project layout

- `load.ol` — BlueApple OS Loader configuration.
- `kernel/boot.asm` — Multiboot entry point and initial stack.
- `kernel/kernel.c` — First kernel boot test.
- `linker.ld` — Kernel memory layout.
- `boot/grub/grub.cfg` — GRUB boot menu.
- `Makefile` — Build and emulator commands.

## Build environment

The Makefile expects a Linux environment (Ubuntu or Debian, including WSL) with:

- NASM
- GCC with 32-bit compilation support
- GNU ld
- GRUB tools including `grub-mkrescue`
- xorriso
- QEMU system i386 (for emulator testing)

On Ubuntu/WSL, install the tools with:

```sh
sudo apt update
sudo apt install nasm gcc-multilib grub-pc-bin grub-common xorriso qemu-system-x86 make
```

Build the ISO:

```sh
make
```

Test it in QEMU:

```sh
make run
```

The ISO is created at `build/blueapple.iso`.

## Roadmap

1. Bootable kernel scaffold.
2. Framebuffer graphics and keyboard input.
3. BlueApple graphical desktop and taskbar.
4. File manager, settings, and basic apps.
5. USB Live Mode and hardware compatibility testing.

**Important:** This early kernel is not yet a daily-use operating system and should be tested in QEMU first. Do not write an experimental image to a USB containing files you need.
