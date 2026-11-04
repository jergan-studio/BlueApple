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
static char current_username[16] = "GUEST";
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
        } else if (starts_with(&line[p], "username")) {
            while (line[p] && line[p] != '=') p++;
            if (line[p] == '=') {
                p++;
                while (line[p] == ' ' || line[p] == '\t') p++;
                u32 n = 0;
                while (line[p] && line[p] != '\r' && n < sizeof(current_username) - 1) {
                    char ch = line[p++];
                    if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
                        (ch >= '0' && ch <= '9') || ch == '_' || ch == '-')
                        current_username[n++] = ch;
                }
                current_username[n] = 0;
                if (!n) {
                    current_username[0] = 'G'; current_username[1] = 'U';
                    current_username[2] = 'E'; current_username[3] = 'S';
                    current_username[4] = 'T'; current_username[5] = 0;
                }
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

static void fill_circle(u32 cx, u32 cy, u32 radius, u32 color) {
    int rr = (int)(radius * radius);
    for (int dy = -(int)radius; dy <= (int)radius; dy++)
        for (int dx = -(int)radius; dx <= (int)radius; dx++)
            if (dx * dx + dy * dy <= rr)
                pixel((u32)((int)cx + dx), (u32)((int)cy + dy), color);
}

/* Rounded rectangle primitive used by Orbit's panels, cards and windows. */
static void fill_round_rect(u32 x, u32 y, u32 width, u32 height,
                            u32 radius, u32 color) {
    if (!width || !height || x >= fb.width || y >= fb.height) return;
    if (radius > width / 2) radius = width / 2;
    if (radius > height / 2) radius = height / 2;
    if (!radius) {
        fill_rect(x, y, width, height, color);
        return;
    }
    fill_rect(x + radius, y, width - radius * 2, height, color);
    fill_rect(x, y + radius, width, height - radius * 2, color);
    fill_circle(x + radius, y + radius, radius, color);
    fill_circle(x + width - radius - 1, y + radius, radius, color);
    fill_circle(x + radius, y + height - radius - 1, radius, color);
    fill_circle(x + width - radius - 1, y + height - radius - 1, radius, color);
}

/* BlueApple's own mark: a blue apple silhouette with a green leaf. */
static void draw_blueapple_logo(u32 x, u32 y, u32 size, u32 background) {
    u32 blue = rgb(75, 165, 255);
    u32 leaf = rgb(105, 220, 155);
    fill_circle(x + size * 36 / 100, y + size * 59 / 100, size * 25 / 100, blue);
    fill_circle(x + size * 64 / 100, y + size * 59 / 100, size * 25 / 100, blue);
    fill_rect(x + size * 17 / 100, y + size * 52 / 100,
              size * 66 / 100, size * 25 / 100, blue);
    fill_circle(x + size * 50 / 100, y + size * 70 / 100, size * 29 / 100, blue);
    /* Leaf and small stem. */
    fill_rect(x + size * 48 / 100, y + size * 10 / 100,
              size * 5 / 100 + 1, size * 18 / 100, rgb(180, 205, 225));
    fill_circle(x + size * 65 / 100, y + size * 16 / 100, size * 13 / 100, leaf);
    /* A small bite gives the mark a recognizable apple profile. */
    fill_circle(x + size * 86 / 100, y + size * 43 / 100, size * 12 / 100, background);
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


static void draw_app_icon(int app, u32 x, u32 y, u32 size);

static void draw_orbit_desktop(void) {
    /* Deep-blue Orbit wallpaper with a soft diagonal color shift. */
    for (u32 y = 0; y < fb.height; y++) {
        for (u32 x = 0; x < fb.width; x++) {
            u32 glow = (x * 18 / (fb.width ? fb.width : 1)) +
                       (y * 22 / (fb.height ? fb.height : 1));
            pixel(x, y, rgb((u8)(13 + glow / 2), (u8)(31 + glow / 2),
                            (u8)(72 + glow)));
        }
    }

    /* Orbit Bar */
    fill_rect(0, 0, fb.width, 44, rgb(34, 39, 48));
    fill_rect(0, 43, fb.width, 1, rgb(65, 76, 92));
    draw_blueapple_logo(16, 7, 28, rgb(10, 17, 36));
    draw_string("BLUEAPPLE", 52, 16, 2, rgb(225, 238, 255));
    draw_string(current_username, 180, 16, 1, rgb(190, 215, 245));
    /* Small Wi-Fi status glyph: icon is present, radio support is a future driver. */
    fill_rect(340, 22, 4, 4, rgb(130, 175, 225));
    fill_rect(334, 18, 16, 3, rgb(130, 175, 225));
    fill_rect(330, 14, 24, 3, rgb(130, 175, 225));
    fill_rect(340, 29, 4, 4, rgb(130, 175, 225));
    draw_string("WIFI", 360, 18, 1, rgb(145, 180, 230));

    /* Desktop shortcut tiles */
    fill_round_rect(30, 78, 72, 76, 10, rgb(25, 53, 91));
    fill_round_rect(30, 158, 86, 76, 10, rgb(25, 53, 91));
    draw_app_icon(1, 50, 88, 32);
    draw_app_icon(2, 50, 168, 32);
    draw_string("FILES", 42, 136, 1, rgb(245, 248, 255));
    draw_string("SETTINGS", 35, 218, 1, rgb(245, 248, 255));

    /* BlueDock */
    u32 dock_width = fb.width < 620 ? fb.width - 24 : 560;
    u32 dock_x = (fb.width - dock_width) / 2;
    u32 dock_y = fb.height > 100 ? fb.height - 76 : 0;
    fill_round_rect(dock_x + 3, dock_y + 3, dock_width, 54, 13, rgb(5, 12, 27));
    fill_round_rect(dock_x, dock_y, dock_width, 54, 13, rgb(34, 42, 57));
    static const int dock_apps[6] = { 0, 1, 3, 4, 2, 9 };
    for (u32 i = 0; i < 6; i++) {
        u32 icon_x = dock_x + 18 + i * 66;
        if (dock_apps[i] == 0)
            draw_blueapple_logo(icon_x + 2, dock_y + 10, 28, rgb(14, 24, 48));
        else
            draw_app_icon(dock_apps[i], icon_x, dock_y + 8, 32);
    }
    draw_string("MENU", dock_x + 17, dock_y + 43, 1, rgb(205, 220, 245));
    draw_string("FILES", dock_x + 82, dock_y + 43, 1, rgb(205, 220, 245));
    draw_string("TERM", dock_x + 148, dock_y + 43, 1, rgb(205, 220, 245));
    draw_string("WEB", dock_x + 214, dock_y + 43, 1, rgb(205, 220, 245));
    draw_string("SET", dock_x + 280, dock_y + 43, 1, rgb(205, 220, 245));
    draw_string("MSG", dock_x + 346, dock_y + 43, 1, rgb(205, 220, 245));
}


/* Simple interactive app/window state for the first Orbit desktop prototype. */
enum {
    APP_NONE = 0,
    APP_FILES = 1,
    APP_SETTINGS = 2,
    APP_TERMINAL = 3,
    APP_WEB = 4,
    APP_PAINT = 5,
    APP_ABOUT = 6,
    APP_WIFI = 7,
    APP_USERS = 8,
    APP_MESSAGES = 9
};

typedef struct {
    int id;
    const char* name;
    const char* subtitle;
    u32 accent;
} orbit_app_t;

/* Central app registry: add future built-in apps here. */
static const orbit_app_t orbit_apps[] = {
    { APP_FILES, "BLUEFILES", "FILE MANAGER", 0x65B7FF },
    { APP_TERMINAL, "BLUETERM", "TERMINAL", 0x69E1AF },
    { APP_SETTINGS, "SETTINGS", "CONTROL CENTER", 0xB28AF5 },
    { APP_WEB, "ORBIT WEB", "BROWSER", 0x8CCBFF },
    { APP_PAINT, "ORBIT PAINT", "DRAWING APP", 0xF0B85A },
    { APP_ABOUT, "ABOUT", "BLUEAPPLE SYSTEM", 0x75D6C0 },
    { APP_WIFI, "WIFI", "WIRELESS NETWORKS", 0x70BFFF },
    { APP_USERS, "USERS", "LOCAL USER PROFILES", 0xD5A5FF },
    { APP_MESSAGES, "MESSAGES", "LOCAL MESSAGE CENTER", 0x75D6C0 }
};
#define ORBIT_APP_COUNT (sizeof(orbit_apps) / sizeof(orbit_apps[0]))

static const orbit_app_t* find_app(int id) {
    for (u32 i = 0; i < ORBIT_APP_COUNT; i++)
        if (orbit_apps[i].id == id) return &orbit_apps[i];
    return 0;
}

static void icon_rect(u32 x, u32 y, u32 size, u32 rx, u32 ry,
                      u32 rw, u32 rh, u32 color) {
    u32 xx = x + rx * size / 32, yy = y + ry * size / 32;
    u32 ww = rw * size / 32, hh = rh * size / 32;
    fill_rect(xx, yy, ww ? ww : 1, hh ? hh : 1, color);
}
static void icon_disc(u32 x, u32 y, u32 size, u32 cx, u32 cy,
                      u32 radius, u32 color) {
    u32 scale = size / 32;
    if (!scale) scale = 1;
    fill_circle(x + cx * size / 32, y + cy * size / 32,
                radius * scale, color);
}

/* Pixel-art style icons, drawn entirely by the kernel. */
static void draw_app_icon(int app, u32 x, u32 y, u32 size) {
    u32 bg = rgb(28, 48, 78);
    u32 white = rgb(238, 245, 255);
    u32 blue = rgb(100, 180, 255);
    u32 accent = blue;

    if (app == 1) accent = rgb(245, 190, 85);       /* Files */
    else if (app == 2) accent = rgb(105, 225, 175);  /* Settings */
    else if (app == 3) accent = rgb(100, 220, 180); /* Terminal */
    else if (app == 4) accent = rgb(115, 190, 255); /* Web */
    else if (app == 5) accent = rgb(245, 175, 85);  /* Paint */
    else if (app == 6) accent = rgb(110, 220, 195); /* About */
    else if (app == 7) accent = rgb(100, 190, 255); /* Wi-Fi */
    else if (app == 8) accent = rgb(205, 155, 255); /* Users */
    else if (app == 9) accent = rgb(100, 220, 195); /* Messages */

    fill_rect(x, y, size, size, bg);
    if (app == 1) {
        icon_rect(x,y,size,4,10,24,17,accent);
        icon_rect(x,y,size,5,6,11,7,accent);
        icon_rect(x,y,size,7,14,18,2,rgb(255,225,155));
    } else if (app == 2) {
        icon_rect(x,y,size,13,3,6,26,accent);
        icon_rect(x,y,size,3,13,26,6,accent);
        icon_disc(x,y,size,16,16,7,bg);
        icon_disc(x,y,size,16,16,3,white);
    } else if (app == 3) {
        icon_rect(x,y,size,4,6,24,20,rgb(9,18,30));
        icon_rect(x,y,size,8,11,3,3,accent);
        icon_rect(x,y,size,11,14,3,3,accent);
        icon_rect(x,y,size,14,17,3,3,accent);
        icon_rect(x,y,size,19,21,7,2,white);
    } else if (app == 4) {
        icon_disc(x,y,size,16,16,13,accent);
        icon_disc(x,y,size,16,16,9,bg);
        icon_rect(x,y,size,15,3,2,26,accent);
        icon_rect(x,y,size,4,15,24,2,accent);
        icon_rect(x,y,size,7,9,18,2,accent);
        icon_rect(x,y,size,7,21,18,2,accent);
    } else if (app == 5) {
        icon_disc(x,y,size,13,17,10,accent);
        icon_disc(x,y,size,11,13,2,white);
        icon_disc(x,y,size,17,11,2,white);
        icon_disc(x,y,size,21,17,2,white);
        icon_rect(x,y,size,20,3,3,13,rgb(210,225,245));
    } else if (app == 6) {
        icon_disc(x,y,size,16,16,12,accent);
        icon_rect(x,y,size,14,13,4,11,bg);
        icon_rect(x,y,size,14,8,4,3,bg);
    } else if (app == 7) {
        icon_rect(x,y,size,14,22,4,4,accent);
        icon_rect(x,y,size,10,17,12,3,accent);
        icon_rect(x,y,size,6,12,20,3,accent);
        icon_rect(x,y,size,3,7,26,3,accent);
    } else if (app == 8) {
        icon_disc(x,y,size,11,12,6,accent);
        icon_disc(x,y,size,23,13,5,accent);
        icon_rect(x,y,size,4,20,15,8,accent);
        icon_rect(x,y,size,18,21,11,7,accent);
    } else if (app == 9) {
        icon_rect(x,y,size,4,6,24,18,accent);
        icon_rect(x,y,size,8,24,6,4,accent);
        icon_rect(x,y,size,9,12,14,2,white);
        icon_rect(x,y,size,9,17,10,2,white);
    } else {
        icon_rect(x,y,size,5,5,22,22,blue);
        icon_rect(x,y,size,13,10,6,12,white);
    }
}

static int active_app = APP_NONE;
static int launcher_open = 0;
static int window_x = -1;
static int window_y = -1;
static int window_dragging = 0;
static int drag_offset_x = 0;
static int drag_offset_y = 0;
static int paint_points_x[96];
static int paint_points_y[96];
static u32 paint_point_count = 0;
static char terminal_line[48];
static u32 terminal_line_length = 0;
static char terminal_output[48] = "TYPE HELP AND PRESS ENTER";
static char message_draft[48];
static u32 message_draft_length = 0;
static char last_message[48] = "WELCOME TO BLUEAPPLE";

static void window_bounds(u32* x, u32* y, u32* width, u32* height) {
    *width = fb.width > 620 ? 500 : fb.width - 40;
    *height = fb.height > 390 ? 300 : fb.height - 100;
    *x = window_x >= 0 ? (u32)window_x : (fb.width - *width) / 2;
    *y = window_y >= 0 ? (u32)window_y : (fb.height - *height) / 2;
}

static void open_app(int app) {
    active_app = app;
    launcher_open = 0;
    window_x = -1;
    window_y = -1;
    window_dragging = 0;
}

static void draw_active_window(void) {
    if (active_app == APP_NONE) return;
    const orbit_app_t* app = find_app(active_app);
    if (!app) return;

    u32 width, height, x, y;
    window_bounds(&x, &y, &width, &height);
    if (width < 220 || height < 130) return;

    fill_round_rect(x + 5, y + 6, width, height, 13, rgb(5, 10, 22));
    fill_round_rect(x, y, width, height, 13, rgb(20, 31, 55));
    fill_round_rect(x, y, width, 42, 13, rgb(48, 63, 82));
    fill_rect(x, y + 22, width, 20, rgb(48, 63, 82));
    fill_round_rect(x + width - 36, y + 8, 26, 26, 8, rgb(194, 77, 87));
    draw_string("X", x + width - 27, y + 15, 2, rgb(255, 255, 255));
    draw_app_icon(active_app, x + 10, y + 4, 30);
    draw_string(app->name, x + 48, y + 14, 2, rgb(240, 246, 255));
    fill_round_rect(x + 18, y + 58, width - 36, 30, 8, rgb(31, 45, 65));
    draw_string(app->subtitle, x + 28, y + 69, 1, app->accent);

    if (active_app == APP_FILES) {
        fill_round_rect(x + 18, y + 98, width - 36, height - 116, 9, rgb(222, 230, 240));
        draw_string("BLUEAPPLE SYSTEM", x + 30, y + 112, 1, rgb(40, 58, 82));
        draw_string("LOAD.OL", x + 34, y + 142, 1, rgb(35, 105, 190));
        draw_string("BOOT CONFIGURATION", x + 132, y + 142, 1, rgb(65, 78, 98));
        draw_string("BLUEAPPLE.ELF", x + 34, y + 170, 1, rgb(35, 105, 190));
        draw_string("KERNEL IMAGE", x + 132, y + 170, 1, rgb(65, 78, 98));
        draw_string("VIRTUAL VIEW - DISK DRIVER NOT READY", x + 30, y + 220, 1, rgb(65, 78, 98));
    } else if (active_app == APP_SETTINGS) {
        draw_string("DISPLAY PROFILE", x + 24, y + 108, 1, rgb(225, 235, 250));
        draw_string("ORBIT BLUE THEME", x + 34, y + 138, 1, rgb(178, 138, 245));
        draw_string("FRAME TARGET", x + 24, y + 174, 1, rgb(225, 235, 250));
        draw_string(target_fps == 50 ? "50 FPS" : (target_fps == 150 ? "150 FPS" : "100 FPS"), x + 34, y + 198, 1, rgb(105, 225, 175));
        draw_string("BOOT MODE", x + 24, y + 232, 1, rgb(225, 235, 250));
        draw_string(boot_mode == 'L' ? "LOCAL" : (boot_mode == '1' ? "USB LIVE" : "MACHINE"), x + 110, y + 232, 1, rgb(225, 235, 250));
    } else if (active_app == APP_TERMINAL) {
        draw_string("BLUEAPPLE TERMINAL", x + 24, y + 108, 1, rgb(105, 225, 175));
        draw_string("TYPE HELP AND PRESS ENTER", x + 24, y + 138, 1, rgb(175, 195, 225));
        draw_string(">", x + 24, y + 178, 1, rgb(105, 225, 175));
        draw_string(terminal_line, x + 38, y + 178, 1, rgb(240, 246, 255));
        draw_string(terminal_output, x + 24, y + 214, 1, rgb(105, 225, 175));
    } else if (active_app == APP_WEB) {
        draw_string("ORBIT WEB // OFFLINE START PAGE", x + 24, y + 108, 1, rgb(225, 235, 250));
        draw_string("NETWORK DRIVER NOT INSTALLED", x + 24, y + 142, 1, rgb(175, 195, 225));
        draw_string("NO INTERNET CONNECTION", x + 24, y + 172, 1, rgb(175, 195, 225));
    } else if (active_app == APP_PAINT) {
        fill_round_rect(x + 24, y + 108, width - 48, height - 132, 9, rgb(235, 239, 247));
        draw_string("CLICK CANVAS TO PAINT", x + 34, y + 90, 1, rgb(240, 246, 255));
        for (u32 i = 0; i < paint_point_count; i++)
            fill_rect(x + 24 + (u32)paint_points_x[i], y + 108 + (u32)paint_points_y[i], 5, 5, rgb(35, 105, 220));
    } else if (active_app == APP_ABOUT) {
        draw_blueapple_logo(x + 24, y + 104, 52, rgb(20, 31, 55));
        draw_string("A SMALL OS WITH A BIG FUTURE", x + 88, y + 112, 1, rgb(225, 235, 250));
        draw_string("ORBIT UI", x + 24, y + 142, 1, rgb(225, 235, 250));
        draw_string("BLUEAPPLE V1 IS IN DEVELOPMENT", x + 24, y + 172, 1, rgb(225, 235, 250));
    } else if (active_app == APP_WIFI) {
        draw_string("WIRELESS STATUS", x + 24, y + 108, 1, rgb(225, 235, 250));
        draw_string("WIFI RADIO: NOT CONNECTED", x + 24, y + 140, 1, rgb(240, 185, 110));
        draw_string("NO WIFI DEVICE DRIVER YET", x + 24, y + 172, 1, rgb(175, 195, 225));
        draw_string("NETWORK SCANNING IS NOT ACTIVE", x + 24, y + 204, 1, rgb(175, 195, 225));
        draw_string("ADAPTER SUPPORT IS NEXT", x + 24, y + 236, 1, rgb(105, 185, 255));
    } else if (active_app == APP_USERS) {
        draw_string("CURRENT PROFILE", x + 24, y + 108, 1, rgb(225, 235, 250));
        draw_string(current_username, x + 24, y + 140, 2, rgb(105, 185, 255));
        draw_string("LOCAL GUEST PROFILE", x + 24, y + 180, 1, rgb(175, 195, 225));
        draw_string("EDIT load.ol TO CHANGE USERNAME", x + 24, y + 220, 1, rgb(175, 195, 225));
    } else if (active_app == APP_MESSAGES) {
        draw_string("INBOX", x + 24, y + 108, 1, rgb(225, 235, 250));
        draw_string("SYSTEM", x + 24, y + 140, 1, rgb(105, 185, 255));
        draw_string("WELCOME TO BLUEAPPLE", x + 100, y + 140, 1, rgb(225, 235, 250));
        draw_string(current_username, x + 24, y + 174, 1, rgb(105, 225, 175));
        draw_string(last_message, x + 100, y + 174, 1, rgb(225, 235, 250));
        draw_string("NEW MESSAGE:", x + 24, y + 218, 1, rgb(175, 195, 225));
        draw_string(message_draft, x + 24, y + 244, 1, rgb(240, 246, 255));
        draw_string("ENTER TO SEND LOCALLY", x + 24, y + 270, 1, rgb(105, 225, 175));
    }
}
static void draw_launcher(void) {
    if (!launcher_open) return;
    u32 width = fb.width > 360 ? 300 : fb.width - 24;
    u32 row_height = 38;
    u32 height = 42 + (u32)ORBIT_APP_COUNT * 44;
    u32 x = 18;
    u32 y = fb.height > height + 70 ? fb.height - height - 70 : 48;
    fill_round_rect(x + 4, y + 5, width, height, 14, rgb(5, 10, 22));
    fill_round_rect(x, y, width, height, 14, rgb(40, 49, 64));
    fill_round_rect(x, y, width, 40, 14, rgb(57, 77, 103));
    fill_rect(x, y + 22, width, 18, rgb(57, 77, 103));
    draw_string("ORBIT MENU", x + 14, y + 13, 2, rgb(240, 246, 255));

    for (u32 i = 0; i < ORBIT_APP_COUNT; i++) {
        u32 row_y = y + 44 + i * 44;
        fill_round_rect(x + 10, row_y, width - 20, row_height, 8, rgb(54, 68, 87));
        draw_app_icon(orbit_apps[i].id, x + 17, row_y + 6, 26);
        draw_string(orbit_apps[i].name, x + 54, row_y + 15, 1, rgb(240, 246, 255));
    }
}
static void render_desktop(void) {
    draw_orbit_desktop();
    draw_active_window();
    draw_launcher();
}

static void desktop_click(int mouse_x, int mouse_y) {
    /* Clicking the Wi-Fi glyph in the Orbit Bar opens wireless status. */
    if (mouse_y >= 6 && mouse_y < 40 && mouse_x >= 326 && mouse_x < 400) {
        open_app(APP_WIFI);
        return;
    }
    u32 dock_width = fb.width < 620 ? fb.width - 24 : 560;
    u32 dock_x = (fb.width - dock_width) / 2;
    u32 dock_y = fb.height > 100 ? fb.height - 76 : 0;

    if ((u32)mouse_y >= dock_y && (u32)mouse_y < dock_y + 54) {
        int item = (mouse_x - (int)dock_x - 8) / 66;
        if (mouse_x >= (int)dock_x + 8 && item >= 0 && item < 6) {
            if (item == 0) {
                active_app = APP_NONE;
                launcher_open = !launcher_open;
            } else if (item == 1) open_app(APP_FILES);
            else if (item == 2) open_app(APP_TERMINAL);
            else if (item == 3) open_app(APP_WEB);
            else if (item == 4) open_app(APP_SETTINGS);
            else if (item == 5) open_app(APP_MESSAGES);
            return;
        }
    }

    if (launcher_open) {
        u32 menu_width = fb.width > 360 ? 300 : fb.width - 24;
        u32 menu_x = 18;
        u32 menu_height = 42 + (u32)ORBIT_APP_COUNT * 44;
        u32 menu_y = fb.height > menu_height + 70 ? fb.height - menu_height - 70 : 48;
        if (mouse_x >= (int)menu_x + 10 && mouse_x < (int)(menu_x + menu_width - 10)) {
            for (u32 i = 0; i < ORBIT_APP_COUNT; i++) {
                u32 row_y = menu_y + 44 + i * 44;
                if (mouse_y >= (int)row_y && mouse_y < (int)(row_y + 38)) {
                    open_app(orbit_apps[i].id);
                    return;
                }
            }
        }
        launcher_open = 0;
        return;
    }

    if (active_app != APP_NONE) {
        u32 width, height, x, y;
        window_bounds(&x, &y, &width, &height);
        if (mouse_x >= (int)(x + width - 34) && mouse_x < (int)(x + width - 10) &&
            mouse_y >= (int)(y + 7) && mouse_y < (int)(y + 31)) {
            active_app = APP_NONE;
            window_dragging = 0;
            return;
        }
        if (mouse_x >= (int)x && mouse_x < (int)(x + width) &&
            mouse_y >= (int)y && mouse_y < (int)(y + 38)) {
            window_dragging = 1;
            drag_offset_x = mouse_x - (int)x;
            drag_offset_y = mouse_y - (int)y;
            return;
        }
        if (active_app == APP_PAINT &&
            mouse_x >= (int)(x + 24) && mouse_x < (int)(x + width - 24) &&
            mouse_y >= (int)(y + 108) && mouse_y < (int)(y + height - 24) &&
            paint_point_count < 96) {
            paint_points_x[paint_point_count] = mouse_x - (int)x - 24;
            paint_points_y[paint_point_count] = mouse_y - (int)y - 108;
            paint_point_count++;
        }
        return;
    }

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
static int mouse_wait_aux_output(void) {
    for (u32 i = 0; i < 100000; i++) {
        u8 status = inb(0x64);
        if ((status & 0x21) == 0x21) return 1;
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
    if (!mouse_wait_aux_output()) return 0;
    return inb(0x60) == 0xFA;
}
static int mouse_initialize(void) {
    /* Enable the auxiliary port while keeping interrupts disabled: this
       kernel polls the controller and does not install an IRQ/IDT yet. */
    if (!mouse_wait_input()) return 0;
    outb(0x64, 0xA8);
    io_wait();

    if (!mouse_wait_input()) return 0;
    outb(0x64, 0x20);              /* Read controller command byte. */
    if (!mouse_wait_output()) return 0;
    u8 command = inb(0x60);
    command = (u8)(command & (u8)~0x22); /* Aux clock on, IRQ12 off: polling only. */
    if (!mouse_wait_input()) return 0;
    outb(0x64, 0x60);              /* Write controller command byte. */
    if (!mouse_wait_input()) return 0;
    outb(0x60, command);

    /* Drain stale controller bytes before starting a fresh mouse session. */
    for (u32 i = 0; i < 32 && (inb(0x64) & 1); i++) (void)inb(0x60);

    /* Reset, consume the reset self-test and device ID, then enable reporting. */
    if (!mouse_write(0xFF) || !mouse_read_ack()) return 0;
    if (!mouse_wait_aux_output()) return 0;
    (void)inb(0x60);               /* 0xAA self-test result */
    if (!mouse_wait_aux_output()) return 0;
    (void)inb(0x60);               /* Mouse device ID */
    if (!mouse_write(0xF6) || !mouse_read_ack()) return 0; /* Defaults */
    if (!mouse_write(0xF4) || !mouse_read_ack()) return 0; /* Data reporting */
    return 1;
}

#define CURSOR_W 12
#define CURSOR_H 16
static u32 cursor_saved[CURSOR_W * CURSOR_H];
static int cursor_visible = 0;
static int cursor_x = 80;
static int cursor_y = 80;
static u8 mouse_buttons = 0;

static void draw_unsigned(u32 value, u32 x, u32 y, u32 color) {
    char digits[10];
    u32 count = 0;
    if (value == 0) digits[count++] = '0';
    while (value && count < sizeof(digits)) {
        digits[count++] = (char)('0' + (value % 10));
        value /= 10;
    }
    while (count) {
        draw_char(digits[--count], x, y, 1, color);
        x += 7;
    }
}

/* Guest pointer coordinates are estimated by accumulating PS/2 relative deltas. */
static void draw_mouse_coordinates(void) {
    /* Keep the coordinate HUD compact at the far right of the Orbit Bar. */
    u32 left = fb.width > 260 ? fb.width - 126 : 4;
    fill_round_rect(left, 7, fb.width > 260 ? 122 : fb.width - 8, 27, 8, rgb(48, 57, 71));
    draw_string("X", left + 4, 16, 1, rgb(145, 180, 230));
    draw_unsigned((u32)cursor_x, left + 17, 16, rgb(235, 245, 255));
    draw_string("Y", left + 66, 16, 1, rgb(145, 180, 230));
    draw_unsigned((u32)cursor_y, left + 79, 16, rgb(235, 245, 255));
}

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
    /* Bit 3 must be set in the first byte; overflow deltas are discarded. */
    if (!(packet[0] & 0x08) || (packet[0] & 0xC0)) return;
    int dx = (int)packet[1] - ((packet[0] & 0x10) ? 256 : 0);
    int dy = (int)packet[2] - ((packet[0] & 0x20) ? 256 : 0);
    u8 new_buttons = packet[0] & 0x07;
    u8 pressed = new_buttons & (u8)~mouse_buttons;

    cursor_restore();
    cursor_x += dx;
    cursor_y -= dy;
    if (cursor_x < 0) cursor_x = 0;
    if (cursor_y < 0) cursor_y = 0;
    if (fb.width && (u32)cursor_x >= fb.width) cursor_x = (int)fb.width - 1;
    if (fb.height && (u32)cursor_y >= fb.height) cursor_y = (int)fb.height - 1;

    if (window_dragging && (new_buttons & 0x01) && active_app != APP_NONE) {
        u32 width, height, old_x, old_y;
        window_bounds(&old_x, &old_y, &width, &height);
        window_x = cursor_x - drag_offset_x;
        window_y = cursor_y - drag_offset_y;
        if (window_x < 0) window_x = 0;
        if (window_y < 42) window_y = 42;
        if ((u32)window_x + width > fb.width) window_x = (int)(fb.width - width);
        if ((u32)window_y + height > fb.height - 54) window_y = (int)(fb.height - height - 54);
        render_desktop();
    }
    if (window_dragging && !(new_buttons & 0x01)) window_dragging = 0;

    mouse_buttons = new_buttons;
    if (pressed & 0x01) {
        desktop_click(cursor_x, cursor_y);
        render_desktop();
    }
    draw_mouse_coordinates();
    cursor_draw();
}
static void mouse_poll(void) {
    static u8 packet[3];
    static u8 packet_index = 0;
    u8 status = inb(0x64);
    /* Leave keyboard bytes in the controller for keyboard_poll next cycle. */
    if (!(status & 1) || !(status & 0x20)) return;
    u8 data = inb(0x60);
    if (packet_index == 0 && !(data & 0x08)) return;
    packet[packet_index++] = data;
    if (packet_index == 3) {
        packet_index = 0;
        mouse_handle_packet(packet);
    }
}

static int text_equals(const char* a, const char* b) {
    while (*a && *b) {
        if (*a != *b) return 0;
        a++;
        b++;
    }
    return *a == 0 && *b == 0;
}

static void terminal_execute(void) {
    terminal_line[terminal_line_length] = 0;
    if (text_equals(terminal_line, "HELP")) {
        const char* s = "HELP ABOUT MODE FPS CLEAR";
        u32 i = 0;
        while (s[i] && i < sizeof(terminal_output) - 1) {
            terminal_output[i] = s[i];
            i++;
        }
        terminal_output[i] = 0;
    } else if (text_equals(terminal_line, "ABOUT")) {
        const char* s = "BLUEAPPLE ORBIT KERNEL";
        u32 i = 0;
        while (s[i] && i < sizeof(terminal_output) - 1) {
            terminal_output[i] = s[i];
            i++;
        }
        terminal_output[i] = 0;
    } else if (text_equals(terminal_line, "MODE")) {
        const char* s = boot_mode == 'L' ? "MODE LO LOCAL" :
                        (boot_mode == '1' ? "MODE M1 USB LIVE" : "MODE M2 MACHINE");
        u32 i = 0;
        while (s[i] && i < sizeof(terminal_output) - 1) {
            terminal_output[i] = s[i];
            i++;
        }
        terminal_output[i] = 0;
    } else if (text_equals(terminal_line, "FPS")) {
        const char* s = target_fps == 50 ? "TARGET 50 FPS" :
                        (target_fps == 150 ? "TARGET 150 FPS" : "TARGET 100 FPS");
        u32 i = 0;
        while (s[i] && i < sizeof(terminal_output) - 1) {
            terminal_output[i] = s[i];
            i++;
        }
        terminal_output[i] = 0;
    } else if (text_equals(terminal_line, "CLEAR")) {
        terminal_output[0] = 0;
    } else {
        const char* s = "UNKNOWN COMMAND";
        u32 i = 0;
        while (s[i] && i < sizeof(terminal_output) - 1) {
            terminal_output[i] = s[i];
            i++;
        }
        terminal_output[i] = 0;
    }
    terminal_line_length = 0;
    terminal_line[0] = 0;
}

static char keyboard_character(u8 code) {
    static const char keys[128] = {
        [0x02]='1',[0x03]='2',[0x04]='3',[0x05]='4',[0x06]='5',
        [0x07]='6',[0x08]='7',[0x09]='8',[0x0A]='9',[0x0B]='0',
        [0x10]='Q',[0x11]='W',[0x12]='E',[0x13]='R',[0x14]='T',
        [0x15]='Y',[0x16]='U',[0x17]='I',[0x18]='O',[0x19]='P',
        [0x1E]='A',[0x1F]='S',[0x20]='D',[0x21]='F',[0x22]='G',
        [0x23]='H',[0x24]='J',[0x25]='K',[0x26]='L',
        [0x2C]='Z',[0x2D]='X',[0x2E]='C',[0x2F]='V',[0x30]='B',
        [0x31]='N',[0x32]='M',[0x39]=' '
    };
    return code < 128 ? keys[code] : 0;
}

static void keyboard_poll(void) {
    u8 status = inb(0x64);
    if (!(status & 1) || (status & 0x20)) return;
    u8 code = inb(0x60);
    if ((code & 0x80) || (active_app != APP_TERMINAL && active_app != APP_MESSAGES)) return;
    if (active_app == APP_TERMINAL) {
        if (code == 0x1C) {
            terminal_execute();
        } else if (code == 0x0E) {
            if (terminal_line_length) terminal_line[--terminal_line_length] = 0;
        } else {
            char ch = keyboard_character(code);
            if (ch && terminal_line_length < sizeof(terminal_line) - 1) {
                terminal_line[terminal_line_length++] = ch;
                terminal_line[terminal_line_length] = 0;
            }
        }
    } else {
        if (code == 0x1C) {
            message_draft[message_draft_length] = 0;
            if (message_draft_length) {
                u32 n = 0;
                while (message_draft[n] && n < sizeof(last_message) - 1) {
                    last_message[n] = message_draft[n];
                    n++;
                }
                last_message[n] = 0;
            }
            message_draft_length = 0;
            message_draft[0] = 0;
        } else if (code == 0x0E) {
            if (message_draft_length) message_draft[--message_draft_length] = 0;
        } else {
            char ch = keyboard_character(code);
            if (ch && message_draft_length < sizeof(message_draft) - 1) {
                message_draft[message_draft_length++] = ch;
                message_draft[message_draft_length] = 0;
            }
        }
    }
    cursor_restore();
    render_desktop();
    draw_mouse_coordinates();
    cursor_draw();
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
        draw_mouse_coordinates();
        cursor_draw();
        mouse_initialize();
        for (;;) {
            keyboard_poll();
            mouse_poll();
            __asm__ volatile ("pause");
        }
    } else {
        draw_text_console();
        for (;;) __asm__ volatile ("hlt");
    }
}
