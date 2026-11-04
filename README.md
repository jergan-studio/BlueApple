# BlueApple OS

BlueApple is an experimental PC operating system with its own visual direction: **Orbit Console**. The goal is a calm, useful system that feels like a small computer environment rather than a copy of a conventional desktop.

## Boot modes

The root `load.ol` file is included in the boot ISO and passed to the kernel as a Multiboot module. It also configures the local profile name shown in Orbit UI.

```ini
# LO = Local Mode (default for VS Code)
# M1 = USB Live Mode
# M2 = Machine Mode
mode = LO

# Allowed values: 50, 100, 150
fps = 100
username = GUEST
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
- **BlueTerm:** accepts basic keyboard input and supports `HELP`, `ABOUT`, `MODE`, `FPS`, and `CLEAR`. It is a small built-in command demo, not a Linux/host shell.
- **Settings:** displays the active boot profile and configured FPS. Interactive settings changes are not implemented.
- **Orbit Web:** displays an offline start page. Networking and web rendering are not implemented.
- **Orbit Paint:** lets you click inside its canvas to place simple marks. This is an early drawing prototype, not a full paint program.
- **About:** displays BlueApple/Orbit information.
- **Wi-Fi:** adds a status icon and a wireless status panel. A real Wi-Fi chipset driver, scanning, authentication, and internet networking are **not implemented yet**.
- **Users:** displays the local profile name configured in `load.ol`. This is not a multi-user login or account system.
- **Messages:** lets you type and send a short local demo message with the keyboard. Messages are temporary and are not sent over a network.

The Orbit Menu now uses a frame-stepped easing animation to slide upward from BlueDock when opened and slide away when closed. This is a visual prototype animation; its timing is currently a short CPU-delay loop rather than a calibrated timer.

App windows can be opened from the dock, desktop shortcuts, or Orbit Menu, dragged by their title bars, and closed with the X control. The PS/2 mouse driver is polled by the kernel; its relative movement packets are accumulated into guest-screen coordinates. These are **BlueApple/QEMU guest coordinates**, not the Linux host desktop's absolute pointer coordinates.

This remains an experimental kernel prototype, not a complete or daily-use operating system.

## QEMU networking milestone

The Local/QEMU launch now attaches a user-mode QEMU network backend and an Intel E1000 (82540EM-compatible) virtual PCI adapter. The kernel includes a polling E1000 driver and an initial IPv4 protocol layer that:
- scans PCI configuration space for vendor/device ID 8086:100E;
- enables PCI memory-space access and bus mastering;
- initializes receive/transmit descriptor rings and DMA buffers;
- exposes Ethernet frame send/receive functions and polls received frames.

The new `kernel/net.c` layer sends DHCP discovery/request messages, parses the offered address, subnet, gateway and DNS options, answers ARP requests for the guest address, validates IPv4 headers, and replies to ICMP echo requests. The actual E1000 MAC is read from the device registers. Both `make run` and `make local` attach QEMU user-mode networking.

The network layer now includes DHCP configuration, basic ARP caching and resolution, IPv4, ICMP echo replies, a one-request DNS A-record resolver, and an initial outgoing TCP client state machine (SYN/SYN-ACK/ACK, data send/read, and close). The public APIs are declared in `kernel/net.h`. This TCP implementation is an early prototype: it does not yet provide robust retransmission, congestion control, receive-window management, multiple simultaneous connections, or complete connection teardown. DNS currently supports one outstanding A-record query and basic uncompressed response names. **Orbit Web still cannot browse websites yet:** HTTP request handling, reliable full TCP behavior, and TLS/HTTPS remain to be implemented and tested. QEMU user networking supplies NAT/DHCP services, but the guest must implement the protocols itself.

The E1000 driver is polling-only and assumes the current no-paging kernel identity-maps its DMA buffers. It targets QEMU's -device e1000 model, not physical Wi-Fi adapters.
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
2. Reliable mouse and keyboard input, expanding BlueTerm's command support.
3. Window focus, resizing, and more complete Orbit desktop behavior.
4. Real filesystem access for BlueFiles and command execution in BlueTerm.
5. Real Wi-Fi chipset drivers, network scanning, and connectivity.
6. Persistent user accounts and network-backed messaging.
7. Drawing tools and more complete built-in apps.
6. USB hardware testing and persistent live-session options.

**Warning:** This is experimental software. Test in QEMU first. Do not use it as a daily operating system yet, and do not write experimental images to a USB containing files you need.


### Jref Browser integration

Orbit Web is being renamed to **Jref Browser**. The current native kernel UI provides a domain field prefilled with `jref-theta.vercel.app`; users can edit it with the keyboard and press Enter to request a DNS A-record lookup. When a result arrives, the UI attempts a TCP connection to port 443 and reports the connection state. This is a connectivity test only: the kernel does not yet implement TLS, HTTP requests/responses, HTML parsing, JavaScript, or page rendering, so it cannot display the hosted Jref website or search results yet. The next browser milestones are reliable TCP, TLS, HTTP(S), and a deliberately small HTML renderer/search interface.
