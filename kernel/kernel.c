/* BlueApple OS - Orbit framebuffer desktop, with VGA fallback */
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;

#define VGA_MEMORY ((volatile u16*)0xB8000)
#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define MULTIBOOT_BOOTLOADER_MAGIC 0x2BADB002
#define MULTIBOOT_INFO_MODS 0x00000008
#define MULTIBOOT_INFO_FRAMEBUFFER 0x00001000
#define FRAMEBUFFER_RGB 1

typedef struct {
    u32 mod_start;
    u32 mod_end;
    u32 string;
    u32 reserved;
} multiboot_module_t;

/* Multiboot 1 information structure, including its framebuffer extension. */
typedef struct __attribute__((packed)) {
    u32 flags;
    u32 mem_lower;
    u32 mem_upper;
    u32 boot_device;
    u32 cmdline;
    u32 mods_count;
    u32 mods_addr;
    u32 syms[4];
    u32 mmap_length;
    u32 mmap_addr;
    u32 drives_length;
    u32 drives_addr;
    u32 config_table;
    u32 boot_loader_name;
    u32 apm_table;
    u32 vbe_control_info;
    u32 vbe_mode_info;
    u16 vbe_mode;
    u16 vbe_interface_seg;
    u16 vbe_interface_off;
    u16 vbe_interface_len;
    u32 framebuffer_addr_low;
    u32 framebuffer_addr_high;
    u32 framebuffer_pitch;
    u32 framebuffer_width;
    u32 framebuffer_height;
    u8 framebuffer_bpp;
    u8 framebuffer_type;
    u8 color_info[6];
} multiboot_info_t;

typedef struct {
    volatile u8* address;
    u32 pitch;
    u32 width;
    u32 height;
    u8 bpp;
    u8 type;
    u8 red_position;
    u8 red_size;
    u8 green_position;
    u8 green_size;
    u8 blue_position;
    u8 blue_size;
} framebuffer_t;

static char boot_mode = 'L';
static u32 target_fps = 100;
static framebuffer_t fb;

static void clear_screen(void) {
    for (u32 i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++)
        VGA_MEMORY[i] = (u16)0x0700 | ' ';
}

static void print_at(const char* text, u32 row, u32 column, u8 color) {
    if (row >= VGA_HEIGHT || column >= VGA_WIDTH) return;
    volatile u16* cell = VGA_MEMORY + row * VGA_WIDTH + column;
    while (*text && column++ < VGA_WIDTH)
        *cell++ = ((u16)color << 8) | (u8)*text++;
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
                if (line[p] == 'L' && line[p + 1] == 'O') boot_mode = 'L';
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
                if (value == 50 || value == 100 || value == 150) target_fps = value;
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
            parse_config((const char*)modules[i].mod_start,
                         modules[i].mod_end - modules[i].mod_start);
            return;
        }
    }
}

static int setup_framebuffer(const multiboot_info_t* info) {
    if (!info || !(info->flags & MULTIBOOT_INFO_FRAMEBUFFER)) return 0;
    if (info->framebuffer_type != FRAMEBUFFER_RGB || info->framebuffer_bpp != 32) return 0;
    if (!info->framebuffer_width || !info->framebuffer_height || !info->framebuffer_pitch) return 0;

    fb.address = (volatile u8*)(u32)info->framebuffer_addr_low;
    fb.pitch = info->framebuffer_pitch;
    fb.width = info->framebuffer_width;
    fb.height = info->framebuffer_height;
    fb.bpp = info->framebuffer_bpp;
    fb.type = info->framebuffer_type;
    fb.red_position = info->color_info[0];
    fb.red_size = info->color_info[1];
    fb.green_position = info->color_info[2];
    fb.green_size = info->color_info[3];
    fb.blue_position = info->color_info[4];
    fb.blue_size = info->color_info[5];
    return fb.address != 0;
}

static u32 rgb(u8 red, u8 green, u8 blue) {
    return ((u32)red << fb.red_position) |
           ((u32)green << fb.green_position) |
           ((u32)blue << fb.blue_position);
}

static void pixel(u32 x, u32 y, u32 color) {
    if (x >= fb.width || y >= fb.height) return;
    volatile u32* row = (volatile u32*)(fb.address + y * fb.pitch);
    row[x] = color;
}

static void fill_rect(u32 x, u32 y, u32 width, u32 height, u32 color) {
    if (x >= fb.width || y >= fb.height) return;
    if (width > fb.width - x) width = fb.width - x;
    if (height > fb.height - y) height = fb.height - y;
    for (u32 yy = 0; yy < height; yy++)
        for (u32 xx = 0; xx < width; xx++)
            pixel(x + xx, y + yy, color);
}

static void draw_orbit_desktop(void) {
    /* Deep-blue Orbit wallpaper with a soft diagonal color shift. */
    for (u32 y = 0; y < fb.height; y++) {
        for (u32 x = 0; x < fb.width; x++) {
            u32 glow = (x * 24 / (fb.width ? fb.width : 1)) +
                       (y * 18 / (fb.height ? fb.height : 1));
            pixel(x, y, rgb((u8)(8 + glow / 3), (u8)(20 + glow / 2),
                            (u8)(55 + glow)));
        }
    }

    /* Orbit Bar */
    fill_rect(0, 0, fb.width, 42, rgb(10, 17, 36));
    fill_rect(18, 12, 18, 18, rgb(65, 155, 255));
    fill_rect(23, 16, 8, 10, rgb(235, 245, 255));
    fill_rect(fb.width > 170 ? fb.width - 150 : 20, 18, 110, 5, rgb(95, 125, 170));

    /* Desktop shortcut tiles */
    fill_rect(42, 82, 48, 48, rgb(30, 75, 135));
    fill_rect(52, 92, 28, 28, rgb(95, 175, 255));
    fill_rect(42, 150, 48, 48, rgb(30, 75, 135));
    fill_rect(53, 161, 26, 26, rgb(110, 220, 190));

    /* BlueDock */
    u32 dock_width = fb.width < 620 ? fb.width - 24 : 560;
    u32 dock_x = (fb.width - dock_width) / 2;
    u32 dock_y = fb.height > 100 ? fb.height - 76 : 0;
    fill_rect(dock_x, dock_y, dock_width, 54, rgb(14, 24, 48));
    for (u32 i = 0; i < 5; i++) {
        u32 icon_x = dock_x + 18 + i * 66;
        fill_rect(icon_x, dock_y + 11, 32, 32,
                  i == 0 ? rgb(65, 155, 255) :
                  (i == 1 ? rgb(90, 205, 175) :
                  (i == 2 ? rgb(225, 180, 85) :
                  (i == 3 ? rgb(175, 130, 245) : rgb(220, 230, 245)))));
    }
}

static void draw_text_console(void) {
    clear_screen();
    print_at(" .--------------------------------------------------------------. ", 2, 8, 0x09);
    print_at("|  BLUEAPPLE // ORBIT CONSOLE                                  |", 3, 8, 0x0B);
    print_at("|  Framebuffer unavailable - safe text fallback active.       |", 4, 8, 0x07);
    print_at(" '--------------------------------------------------------------' ", 5, 8, 0x09);
    print_at("SYSTEM PROFILE", 8, 12, 0x0B);
    print_at("Boot mode :", 10, 12, 0x07);
    print_at(boot_mode == 'L' ? "LO / LOCAL" : (boot_mode == '1' ? "M1 / USB LIVE" : "M2 / MACHINE"), 10, 29, 0x0F);
    print_at("Frame target :", 11, 12, 0x07);
    print_at(target_fps == 50 ? "50 FPS" : (target_fps == 150 ? "150 FPS" : "100 FPS"), 11, 29, 0x0F);
    print_at("ORBIT // graphics initialization needs a 32-bit RGB framebuffer", 14, 8, 0x0E);
}

void kernel_main(u32 magic, const multiboot_info_t* info) {
    load_config(magic, info);
    if (setup_framebuffer(info)) {
        draw_orbit_desktop();
    } else {
        draw_text_console();
    }
    for (;;) __asm__ volatile ("hlt");
}
