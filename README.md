# BlueApple OS

BlueApple is an experimental PC operating system with its own visual direction: **Orbit Console**. The goal is a calm, useful system that feels like a small computer environment rather than a copy of a conventional desktop.

## Boot modes

The root `load.ol` file is included in the boot ISO and passed to the kernel as a Multiboot module.

```ini
# M1 = USB Live Mode
# M2 = Machine Mode
mode = M1

# Allowed values: 50, 100, 150
fps = 100
```

- **M1 / USB Live:** temporary live session intended for booting from USB. The current kernel does not write to storage.
- **M2 / Machine:** dedicated-machine boot profile. This currently changes the boot profile and screen, but does not install BlueApple or configure a disk.
- **FPS:** the selected value is read and displayed by the kernel. A real graphics frame limiter will use this setting after framebuffer graphics are implemented.

If the config is missing or invalid, the kernel defaults to M1 and 100 FPS.

## Current milestone

The Multiboot 1 kernel reads the boot mode and FPS from `load.ol`, then displays the Orbit Console boot status. This is still a text-mode kernel test, not a finished graphical operating system.

## Safety design

- M1 is designed as the USB Live profile.
- The current kernel does not contain disk-writing or installation code.
- Choose the USB from the computer's one-time boot menu; avoid changing permanent boot order settings.
- Shut down before removing the USB. Then remove it and restart to let the firmware boot the normal internal drive.
- M2 is not an installer. It does not currently write to, format, or repartition any drive.
- Any future installer must be a separate, explicit feature with clear warnings and confirmation.

## Project layout

- `load.ol` — boot mode and FPS configuration.
- `kernel/boot.asm` — Multiboot entry point; passes boot information to C.
- `kernel/kernel.c` — configuration parser and Orbit Console boot screen.
- `linker.ld` — kernel memory layout.
- `boot/grub/grub.cfg` — loads the kernel and `load.ol`.
- `Makefile` — builds the ISO and launches QEMU.

## Build environment

Use Ubuntu/Debian or WSL with:

```sh
sudo apt update
sudo apt install nasm gcc-multilib grub-pc-bin grub-common xorriso qemu-system-x86 make
```

Build and test:

```sh
make clean
make
make run
```

The ISO is created at `build/blueapple.iso`.

## Roadmap

1. Configuration-aware M1/M2 boot.
2. Framebuffer graphics and keyboard input.
3. Orbit desktop shell with its own navigation and window behavior.
4. File manager, settings, and original BlueApple apps.
5. USB hardware testing and persistent live-session options.

**Warning:** This is experimental software. Test in QEMU first. Do not use it as a daily operating system yet, and do not write experimental images to a USB containing files you need.
