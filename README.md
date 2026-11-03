# BlueApple OS

BlueApple is an experimental PC operating system with its own visual direction: **Orbit Console**. The goal is a calm, useful system that feels like a small computer environment rather than a copy of a conventional desktop.

## Boot modes

The root `load.ol` file is included in the boot ISO and passed to the kernel as a Multiboot module.

```ini
# LO = Local Mode (default for VS Code)
# M1 = USB Live Mode
# M2 = Machine Mode
mode = LO

# Allowed values: 50, 100, 150
fps = 100
```

- **LO / Local:** default developer profile. Build and run locally from VS Code using QEMU; it does not install BlueApple.
- **M1 / USB Live:** temporary live session intended for booting from USB. The current kernel does not write to storage.
- **M2 / Machine:** dedicated-machine boot profile. This currently changes the boot profile and screen, but does not install BlueApple or configure a disk.
- **FPS:** the selected value is read and displayed by the kernel. A real graphics frame limiter will use this setting after framebuffer graphics are implemented.

If the config is missing or invalid, the kernel defaults to LO and 100 FPS.

## Current milestone

BlueApple now has an early framebuffer-based **Orbit desktop prototype**. It draws the Orbit Bar, desktop shortcuts, BlueDock, Orbit Menu, app windows, a guest mouse cursor, and an estimated X/Y coordinate HUD.

The built-in app prototypes are:

- **BlueFiles:** displays a small virtual system-file listing. Real disk and filesystem drivers are not implemented.
- **BlueTerm:** displays terminal session status. Keyboard input and command execution are not implemented.
- **Settings:** displays the active boot profile and configured FPS. Interactive settings changes are not implemented.
- **Orbit Web:** displays an offline start page. Networking and web rendering are not implemented.
- **Orbit Paint:** lets you click inside its canvas to place simple marks. This is an early drawing prototype, not a full paint program.
- **About:** displays BlueApple/Orbit information.

App windows can be opened from the dock, desktop shortcuts, or Orbit Menu, dragged by their title bars, and closed with the X control. The PS/2 mouse driver is polled by the kernel; its relative movement packets are accumulated into guest-screen coordinates. These are **BlueApple/QEMU guest coordinates**, not the Linux host desktop's absolute pointer coordinates.

This remains an experimental kernel prototype, not a complete or daily-use operating system.

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

1. Configuration-aware LO/M1/M2 boot.
2. Reliable mouse and keyboard input, including keyboard-driven terminal use.
3. Window focus, resizing, and more complete Orbit desktop behavior.
4. Real filesystem access for BlueFiles and command execution in BlueTerm.
5. Drawing tools, networking, and more complete built-in apps.
6. USB hardware testing and persistent live-session options.

**Warning:** This is experimental software. Test in QEMU first. Do not use it as a daily operating system yet, and do not write experimental images to a USB containing files you need.
