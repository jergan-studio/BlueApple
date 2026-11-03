/* BlueApple first boot test kernel */
typedef unsigned short u16;
typedef unsigned int u32;

#define VGA_MEMORY ((volatile u16*)0xB8000)
#define VGA_WIDTH 80
#define VGA_HEIGHT 25

static void clear_screen(void) {
    for (u32 i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        VGA_MEMORY[i] = (u16)0x0F00 | ' ';
    }
}

static void print_at(const char* text, u32 row, u32 column, u8 color) {
    volatile u16* cell = VGA_MEMORY + row * VGA_WIDTH + column;
    while (*text) {
        *cell++ = ((u16)color << 8) | (u8)*text++;
    }
}

void kernel_main(void) {
    clear_screen();
    print_at("BlueApple OS", 3, 4, 0x0B);
    print_at("The kernel has booted successfully.", 5, 4, 0x0F);
    print_at("Next: graphics, desktop, and USB live mode.", 7, 4, 0x0E);
    print_at("You can safely power off this test machine.", 10, 4, 0x07);

    for (;;) {
        __asm__ volatile ("hlt");
    }
}
