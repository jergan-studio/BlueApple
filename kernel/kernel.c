/* BlueApple OS - configuration-aware boot kernel */
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;

#define VGA_MEMORY ((volatile u16*)0xB8000)
#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define MULTIBOOT_BOOTLOADER_MAGIC 0x2BADB002
#define MULTIBOOT_INFO_MODS 0x00000008

typedef struct {
    u32 mod_start;
    u32 mod_end;
    u32 string;
    u32 reserved;
} multiboot_module_t;

typedef struct {
    u32 flags;
    u32 mem_lower;
    u32 mem_upper;
    u32 boot_device;
    u32 cmdline;
    u32 mods_count;
    u32 mods_addr;
} multiboot_info_t;

static char boot_mode = 'L';
static u32 target_fps = 100;

static void clear_screen(void) {
    for (u32 i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++)
        VGA_MEMORY[i] = (u16)0x0700 | ' ';
}

static void print_at(const char* text, u32 row, u32 column, u8 color) {
    if (row >= VGA_HEIGHT || column >= VGA_WIDTH) return;
    volatile u16* cell = VGA_MEMORY + row * VGA_WIDTH + column;
    while (*text && column++ < VGA_WIDTH) {
        *cell++ = ((u16)color << 8) | (u8)*text++;
    }
}

static int starts_with(const char* text, const char* prefix) {
    while (*prefix) {
        if (*text++ != *prefix++) return 0;
    }
    return 1;
}

static void parse_config(const char* data, u32 length) {
    u32 i = 0;
    while (i < length) {
        char line[48];
        u32 n = 0;
        while (i < length && data[i] != '\n' && n < sizeof(line) - 1)
            line[n++] = data[i++];
        while (i < length && data[i] != '\n') i++;
        if (i < length && data[i] == '\n') i++;
        line[n] = 0;

        u32 p = 0;
        while (line[p] == ' ' || line[p] == '\t' || line[p] == '\r') p++;
        if (line[p] == '#' || line[p] == 0) continue;

        if (starts_with(&line[p], "mode")) {
            while (line[p] && line[p] != '=') p++;
            if (line[p] == '=') {
                p++;
                while (line[p] == ' ' || line[p] == '\t') p++;
                if (line[p] == 'L' && line[p + 1] == 'O')
                    boot_mode = 'L';
                else if (line[p] == 'M' && (line[p + 1] == '1' || line[p + 1] == '2'))
                    boot_mode = line[p + 1];
            }
        } else if (starts_with(&line[p], "fps")) {
            while (line[p] && line[p] != '=') p++;
            if (line[p] == '=') {
                p++;
                while (line[p] == ' ' || line[p] == '\t') p++;
                u32 value = 0;
                while (line[p] >= '0' && line[p] <= '9')
                    value = value * 10 + (u32)(line[p++] - '0');
                if (value == 50 || value == 100 || value == 150)
                    target_fps = value;
            }
        }
    }
}

static void load_config(u32 magic, const multiboot_info_t* info) {
    if (magic != MULTIBOOT_BOOTLOADER_MAGIC || !info) return;
    if (!(info->flags & MULTIBOOT_INFO_MODS) || info->mods_count == 0) return;

    const multiboot_module_t* modules = (const multiboot_module_t*)info->mods_addr;
    for (u32 i = 0; i < info->mods_count; i++) {
        if (modules[i].mod_end > modules[i].mod_start) {
            const char* data = (const char*)modules[i].mod_start;
            u32 length = modules[i].mod_end - modules[i].mod_start;
            parse_config(data, length);
            return;
        }
    }
}

void kernel_main(u32 magic, const multiboot_info_t* info) {
    load_config(magic, info);
    clear_screen();

    /* A deliberately different identity: BlueApple's "Orbit Console". */
    print_at(" .--------------------------------------------------------------. ", 2, 8, 0x09);
    print_at("|  BLUEAPPLE // ORBIT CONSOLE                                  |", 3, 8, 0x0B);
    print_at("|  A small, calm operating system built around your machine.  |", 4, 8, 0x07);
    print_at(" '--------------------------------------------------------------' ", 5, 8, 0x09);

    print_at("SYSTEM PROFILE", 8, 12, 0x0B);
    print_at("Boot mode :", 10, 12, 0x07);
    print_at(boot_mode == 'L' ? "LO / LOCAL" : (boot_mode == '1' ? "M1 / USB LIVE" : "M2 / MACHINE"), 10, 29, 0x0F);
    print_at("Frame target :", 11, 12, 0x07);
    print_at(target_fps == 50 ? "50 FPS" : (target_fps == 150 ? "150 FPS" : "100 FPS"), 11, 29, 0x0F);

    if (boot_mode == 'L') {
        print_at("LOCAL DEVELOPMENT SESSION", 14, 12, 0x0B);
        print_at("BlueApple is running locally for development and testing.", 16, 12, 0x0F);
        print_at("This profile is intended for VS Code and QEMU.", 17, 12, 0x0E);
        print_at("No internal disk installation is performed.", 18, 12, 0x07);
    } else if (boot_mode == '1') {
        print_at("LIVE SESSION", 14, 12, 0x0B);
        print_at("BlueApple is running as a temporary USB session.", 16, 12, 0x0F);
        print_at("This kernel does not write to the internal drive.", 17, 12, 0x0E);
        print_at("Shut down before removing the USB device.", 18, 12, 0x07);
    } else {
        print_at("MACHINE SESSION", 14, 12, 0x0B);
        print_at("BlueApple is running in dedicated-machine mode.", 16, 12, 0x0F);
        print_at("This mode does not install or modify any disk.", 17, 12, 0x0E);
        print_at("Disk installation is not implemented.", 18, 12, 0x07);
    }

    print_at("ORBIT // boot configuration loaded", 22, 12, 0x09);
    for (;;) __asm__ volatile ("hlt");
}
