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


/* Compact 5x7 font for readable Orbit UI labels (uppercase and digits). */
static const u8 font[36][5] = {
    {0x7E,0x11,0x11,0x11,0x7E}, {0x7F,0x49,0x49,0x49,0x36},
    {0x3E,0x41,0x41,0x41,0x22}, {0x7F,0x41,0x41,0x22,0x1C},
    {0x7F,0x49,0x49,0x49,0x41}, {0x7F,0x09,0x09,0x09,0x01},
    {0x3E,0x41,0x49,0x49,0x7A}, {0x7F,0x08,0x08,0x08,0x7F},
    {0x00,0x41,0x7F,0x41,0x00}, {0x20,0x40,0x41,0x3F,0x01},
    {0x7F,0x08,0x14,0x22,0x41}, {0x7F,0x40,0x40,0x40,0x40},
    {0x7F,0x02,0x0C,0x02,0x7F}, {0x7F,0x04,0x08,0x10,0x7F},
    {0x3E,0x41,0x41,0x41,0x3E}, {0x7F,0x09,0x09,0x09,0x06},
    {0x3E,0x41,0x51,0x21,0x5E}, {0x7F,0x09,0x19,0x29,0x46},
    {0x46,0x49,0x49,0x49,0x31}, {0x01,0x01,0x7F,0x01,0x01},
    {0x3F,0x40,0x40,0x40,0x3F}, {0x1F,0x20,0x40,0x20,0x1F},
    {0x3F,0x40,0x38,0x40,0x3F}, {0x63,0x14,0x08,0x14,0x63},
    {0x07,0x08,0x70,0x08,0x07}, {0x61,0x51,0x49,0x45,0x43},
    {0x3E,0x51,0x49,0x45,0x3E}, {0x00,0x42,0x7F,0x40,0x00},
    {0x62,0x51,0x49,0x49,0x46}, {0x22,0x41,0x49,0x49,0x36},
    {0x18,0x14,0x12,0x7F,0x10}, {0x2F,0x49,0x49,0x49,0x31},
    {0x3E,0x49,0x49,0x49,0x32}, {0x01,0x71,0x09,0x05,0x03},
    {0x36,0x49,0x49,0x49,0x36}, {0x26,0x49,0x49,0x49,0x3E}
};

static int glyph_index(char c) {
    if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= '0' && c <= '9') return 26 + c - '0';
    return -1;
}

static void draw_char(char c, u32 x, u32 y, u32 scale, u32 color) {
    int index = glyph_index(c);
    if (index < 0) return;
    for (u32 column = 0; column < 5; column++) {
        for (u32 row = 0; row < 7; row++) {
            if (font[index][column] & (1u << row))
                fill_rect(x + column * scale, y + row * scale, scale, scale, color);
        }
    }
}

static void draw_string(const char* text, u32 x, u32 y, u32 scale, u32 color) {
    while (*text) {
        if (*text == ' ') x += 4 * scale;
        else {
            draw_char(*text, x, y, scale, color);
            x += 7 * scale;
        }
        text++;
        if (x >= fb.width) break;
    }
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
    draw_string("BLUEAPPLE", 48, 16, 2, rgb(225, 238, 255));
    draw_string("ORBIT", fb.width > 170 ? fb.width - 112 : 20, 16, 2, rgb(145, 180, 230));

    /* Desktop shortcut tiles */
    fill_rect(42, 82, 48, 48, rgb(30, 75, 135));
    fill_rect(52, 92, 28, 28, rgb(95, 175, 255));
    fill_rect(42, 150, 48, 48, rgb(30, 75, 135));
    fill_rect(53, 161, 26, 26, rgb(110, 220, 190));
    draw_string("FILES", 42, 136, 1, rgb(245, 248, 255));
    draw_string("SETTINGS", 35, 204, 1, rgb(245, 248, 255));

    /* BlueDock */
    u32 dock_width = fb.width < 620 ? fb.width - 24 : 560;
    u32 dock_x = (fb.width - dock_width) / 2;
    u32 dock_y = fb.height > 100 ? fb.height - 76 : 0;
    fill_rect(dock_x, dock_y, dock_width, 54, rgb(14, 24, 48));
    for (u32 i = 0; i < 5; i++) {
        u32 icon_x = dock_x + 18 + i * 66;
        fill_rect(icon_x, dock_y + 8, 32, 32,
                  i == 0 ? rgb(65, 155, 255) :
                  (i == 1 ? rgb(90, 205, 175) :
                  (i == 2 ? rgb(225, 180, 85) :
                  (i == 3 ? rgb(175, 130, 245) : rgb(220, 230, 245)))));
    }
    draw_string("MENU", dock_x + 17, dock_y + 43, 1, rgb(205, 220, 245));
    draw_string("FILES", dock_x + 82, dock_y + 43, 1, rgb(205, 220, 245));
    draw_string("TERM", dock_x + 148, dock_y + 43, 1, rgb(205, 220, 245));
    draw_string("WEB", dock_x + 214, dock_y + 43, 1, rgb(205, 220, 245));
    draw_string("SET", dock_x + 280, dock_y + 43, 1, rgb(205, 220, 245));
}


/* Simple interactive app/window state for the first Orbit desktop prototype. */
enum {
    APP_NONE = 0,
    APP_FILES = 1,
    APP_SETTINGS = 2,
    APP_TERMINAL = 3,
    APP_WEB = 4
};
static int active_app = APP_NONE;
static int launcher_open = 0;

static void open_app(int app) {
    active_app = app;
    launcher_open = 0;
}

static void draw_active_window(void) {
    if (active_app == APP_NONE) return;

    u32 width = fb.width > 620 ? 500 : fb.width - 40;
    u32 height = fb.height > 390 ? 300 : fb.height - 100;
    if (width < 220 || height < 130) return;
    u32 x = (fb.width - width) / 2;
    u32 y = (fb.height - height) / 2;
    u32 panel = rgb(20, 31, 55);
    u32 title = rgb(35, 57, 91);

    fill_rect(x, y, width, height, panel);
    fill_rect(x, y, width, 38, title);
    fill_rect(x + width - 34, y + 7, 24, 24, rgb(190, 65, 78));
    draw_string("X", x + width - 27, y + 15, 2, rgb(255, 255, 255));

    if (active_app == APP_FILES) {
        draw_string("BLUEFILES", x + 18, y + 14, 2, rgb(240, 246, 255));
        draw_string("FILE MANAGER", x + 24, y + 68, 2, rgb(125, 195, 255));
        draw_string("YOUR FILES WILL APPEAR HERE", x + 24, y + 112, 1, rgb(225, 235, 250));
        draw_string("STORAGE SUPPORT IS COMING", x + 24, y + 142, 1, rgb(175, 195, 225));
    } else if (active_app == APP_SETTINGS) {
        draw_string("SETTINGS", x + 18, y + 14, 2, rgb(240, 246, 255));
        draw_string("CONTROL CENTER", x + 24, y + 68, 2, rgb(125, 195, 255));
        draw_string("DISPLAY", x + 24, y + 112, 1, rgb(225, 235, 250));
        draw_string("INPUT", x + 24, y + 142, 1, rgb(225, 235, 250));
        draw_string("SYSTEM PROFILE", x + 24, y + 172, 1, rgb(225, 235, 250));
    } else if (active_app == APP_TERMINAL) {
        draw_string("BLUETERM", x + 18, y + 14, 2, rgb(240, 246, 255));
        draw_string("TERMINAL READY", x + 24, y + 68, 2, rgb(105, 225, 175));
        draw_string("COMMAND SUPPORT IS COMING", x + 24, y + 112, 1, rgb(225, 235, 250));
    } else if (active_app == APP_WEB) {
        draw_string("ORBIT WEB", x + 18, y + 14, 2, rgb(240, 246, 255));
        draw_string("BROWSER", x + 24, y + 68, 2, rgb(125, 195, 255));
        draw_string("NETWORK SUPPORT IS COMING", x + 24, y + 112, 1, rgb(225, 235, 250));
    }
}

static void draw_launcher(void) {
    if (!launcher_open) return;
    u32 width = fb.width > 360 ? 300 : fb.width - 24;
    u32 height = 238;
    u32 x = 18;
    u32 y = fb.height > height + 70 ? fb.height - height - 70 : 48;
    fill_rect(x, y, width, height, rgb(18, 29, 53));
    fill_rect(x, y, width, 36, rgb(38, 62, 98));
    draw_string("ORBIT MENU", x + 14, y + 13, 2, rgb(240, 246, 255));

    fill_rect(x + 12, y + 50, width - 24, 42, rgb(34, 66, 105));
    fill_rect(x + 12, y + 102, width - 24, 42, rgb(34, 66, 105));
    fill_rect(x + 12, y + 154, width - 24, 42, rgb(34, 66, 105));
    draw_string("FILES", x + 26, y + 66, 2, rgb(240, 246, 255));
    draw_string("TERMINAL", x + 26, y + 118, 2, rgb(240, 246, 255));
    draw_string("SETTINGS", x + 26, y + 170, 2, rgb(240, 246, 255));
}

static void render_desktop(void) {
    draw_orbit_desktop();
    draw_active_window();
    draw_launcher();
}

static void desktop_click(int mouse_x, int mouse_y) {
    u32 dock_width = fb.width < 620 ? fb.width - 24 : 560;
    u32 dock_x = (fb.width - dock_width) / 2;
    u32 dock_y = fb.height > 100 ? fb.height - 76 : 0;

    /* Dock clicks are handled first so the dock remains a navigation surface. */
    if ((u32)mouse_y >= dock_y && (u32)mouse_y < dock_y + 54) {
        int item = (mouse_x - (int)dock_x - 8) / 66;
        if (mouse_x >= (int)dock_x + 8 && item >= 0 && item < 5) {
            if (item == 0) {
                active_app = APP_NONE;
                launcher_open = !launcher_open;
            } else if (item == 1) open_app(APP_FILES);
            else if (item == 2) open_app(APP_TERMINAL);
            else if (item == 3) open_app(APP_WEB);
            else if (item == 4) open_app(APP_SETTINGS);
            return;
        }
    }

    if (launcher_open) {
        u32 menu_width = fb.width > 360 ? 300 : fb.width - 24;
        u32 menu_x = 18;
        u32 menu_y = fb.height > 308 ? fb.height - 308 : 48;
        if (mouse_x >= (int)menu_x + 12 && mouse_x < (int)(menu_x + menu_width - 12)) {
            if (mouse_y >= (int)menu_y + 50 && mouse_y < (int)menu_y + 92)
                open_app(APP_FILES);
            else if (mouse_y >= (int)menu_y + 102 && mouse_y < (int)menu_y + 144)
                open_app(APP_TERMINAL);
            else if (mouse_y >= (int)menu_y + 154 && mouse_y < (int)menu_y + 196)
                open_app(APP_SETTINGS);
            else launcher_open = 0;
        } else {
            launcher_open = 0;
        }
        return;
    }

    if (active_app != APP_NONE) {
        u32 width = fb.width > 620 ? 500 : fb.width - 40;
        u32 height = fb.height > 390 ? 300 : fb.height - 100;
        u32 x = (fb.width - width) / 2;
        u32 y = (fb.height - height) / 2;
        if (mouse_x >= (int)(x + width - 34) && mouse_x < (int)(x + width - 10) &&
            mouse_y >= (int)(y + 7) && mouse_y < (int)(y + 31))
            active_app = APP_NONE;
        return;
    }

    /* Desktop shortcuts. */
    if (mouse_x >= 30 && mouse_x <= 110 && mouse_y >= 76 && mouse_y <= 146)
        open_app(APP_FILES);
    else if (mouse_x >= 30 && mouse_x <= 110 && mouse_y >= 146 && mouse_y <= 216)
        open_app(APP_SETTINGS);
}


/* Polled PS/2 mouse driver: no IRQ/IDT dependency in this early kernel. */
static inline void outb(u16 port, u8 value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}
static inline u8 inb(u16 port) {
    u8 value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}
static void io_wait(void) {
    __asm__ volatile ("outb %%al, $0x80" : : "a"(0));
}
static int mouse_wait_input(void) {
    for (u32 i = 0; i < 100000; i++) {
        if (!(inb(0x64) & 2)) return 1;
    }
    return 0;
}
static int mouse_wait_output(void) {
    for (u32 i = 0; i < 100000; i++) {
        if (inb(0x64) & 1) return 1;
    }
    return 0;
}
static int mouse_write(u8 value) {
    if (!mouse_wait_input()) return 0;
    outb(0x64, 0xD4);
    if (!mouse_wait_input()) return 0;
    outb(0x60, value);
    return 1;
}
static int mouse_read_ack(void) {
    if (!mouse_wait_output()) return 0;
    return inb(0x60) == 0xFA;
}
static int mouse_initialize(void) {
    if (!mouse_wait_input()) return 0;
    outb(0x64, 0xA8);       /* Enable auxiliary PS/2 device port. */
    io_wait();
    if (!mouse_write(0xF4)) return 0; /* Enable mouse data reporting. */
    return mouse_read_ack();
}

#define CURSOR_W 12
#define CURSOR_H 16
static u32 cursor_saved[CURSOR_W * CURSOR_H];
static int cursor_visible = 0;
static int cursor_x = 80;
static int cursor_y = 80;
static u8 mouse_buttons = 0;

static const u16 cursor_shape[CURSOR_H] = {
    0x001, 0x003, 0x007, 0x00F, 0x01F, 0x03F, 0x07F, 0x0FF,
    0x1FF, 0x03F, 0x033, 0x063, 0x0C3, 0x183, 0x103, 0x000
};

static void cursor_restore(void) {
    if (!cursor_visible) return;
    for (u32 y = 0; y < CURSOR_H; y++)
        for (u32 x = 0; x < CURSOR_W; x++)
            pixel((u32)cursor_x + x, (u32)cursor_y + y,
                  cursor_saved[y * CURSOR_W + x]);
    cursor_visible = 0;
}
static void cursor_draw(void) {
    if (cursor_x < 0 || cursor_y < 0) return;
    for (u32 y = 0; y < CURSOR_H; y++) {
        for (u32 x = 0; x < CURSOR_W; x++) {
            u32 px = (u32)cursor_x + x;
            u32 py = (u32)cursor_y + y;
            if (px >= fb.width || py >= fb.height) {
                cursor_saved[y * CURSOR_W + x] = 0;
                continue;
            }
            volatile u32* row = (volatile u32*)(fb.address + py * fb.pitch);
            cursor_saved[y * CURSOR_W + x] = row[px];
            if (cursor_shape[y] & (1u << x))
                pixel(px, py, (x == 0 || y == 0) ? rgb(5, 10, 22) : rgb(245, 249, 255));
        }
    }
    cursor_visible = 1;
}
static void mouse_handle_packet(const u8 packet[3]) {
    /* Ignore overflow packets; PS/2 reports signed 8-bit movement. */
    if (packet[0] & 0xC0) return;
    int dx = (int)packet[1] - ((packet[0] & 0x10) ? 256 : 0);
    int dy = (int)packet[2] - ((packet[0] & 0x20) ? 256 : 0);
    cursor_restore();
    cursor_x += dx;
    cursor_y -= dy;
    if (cursor_x < 0) cursor_x = 0;
    if (cursor_y < 0) cursor_y = 0;
    if ((u32)cursor_x >= fb.width) cursor_x = (int)fb.width - 1;
    if ((u32)cursor_y >= fb.height) cursor_y = (int)fb.height - 1;

    u8 new_buttons = packet[0] & 0x07;
    u8 pressed = new_buttons & (u8)~mouse_buttons;
    mouse_buttons = new_buttons;
    if (pressed & 0x01) {
        desktop_click(cursor_x, cursor_y);
        render_desktop();
    }
    cursor_draw();
}
static void mouse_poll(void) {
    static u8 packet[3];
    static u8 packet_index = 0;
    u8 status = inb(0x64);
    if (!(status & 1) || !(status & 0x20)) return;
    u8 data = inb(0x60);
    if (packet_index == 0 && !(data & 0x08)) return; /* Packet sync bit. */
    packet[packet_index++] = data;
    if (packet_index == 3) {
        packet_index = 0;
        mouse_handle_packet(packet);
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
        render_desktop();
        cursor_draw();
        mouse_initialize();
        for (;;) {
            mouse_poll();
            __asm__ volatile ("pause");
        }
    } else {
        draw_text_console();
        for (;;) __asm__ volatile ("hlt");
    }
}
