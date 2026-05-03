typedef unsigned int u32;
typedef unsigned long long u64;
typedef unsigned char u8;
typedef unsigned short u16;

struct multiboot_info {
    u32 flags, mem_lower, mem_upper, boot_device, cmdline;
    u32 mods_count, mods_addr, syms[4];
    u32 mmap_length, mmap_addr, drives_length, drives_addr;
    u32 config_table, boot_loader_name, apm_table;
    u32 vbe_control_info, vbe_mode_info;
    u16 vbe_mode, vbe_interface_seg, vbe_interface_off, vbe_interface_len;
    u64 framebuffer_addr;
    u32 framebuffer_pitch, framebuffer_width, framebuffer_height;
    u8 framebuffer_bpp, framebuffer_type;
};

u32* fb;
u32 width, height, pitch;

int terminal_open = 0;
int explorer_open = 0;
int settings_open = 0;
int start_open = 0;
int powered_off = 0;

int active_window = 0;     /* 0 desktop, 1 terminal, 2 files, 3 settings */
int super_down = 0;
int extended_scancode = 0;
int start_selected = 0;
int file_selected = 0;
int settings_selected = 0;
int theme = 0;             /* 0 light, 1 dark */
int brightness = 100;

int cursor_x = 70;
int cursor_y = 185;

char input[128];
int input_len = 0;
const char* current_path = "@user/mamikk";

unsigned char inb(unsigned short port) {
    unsigned char ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

void halt_forever() {
    while (1) {
        __asm__ volatile ("hlt");
    }
}

u32 bg() { return theme ? 0x101018 : 0xE8E8E8; }
u32 panel() { return theme ? 0x202030 : 0xD0D0D0; }
u32 win_bg() { return theme ? 0x15151F : 0xF6F6F6; }
u32 term_bg() { return theme ? 0x050507 : 0xFFFFFF; }
u32 txt() { return theme ? 0xFFFFFF : 0x000000; }
u32 accent() { return 0x00AEEF; }

void pixel(int x, int y, u32 color) {
    if (x < 0 || y < 0 || x >= (int)width || y >= (int)height) return;
    u32* row = (u32*)((u8*)fb + y * pitch);
    row[x] = color;
}

void rect(int x, int y, int w, int h, u32 color) {
    for (int yy = y; yy < y + h; yy++)
        for (int xx = x; xx < x + w; xx++)
            pixel(xx, yy, color);
}

void border(int x, int y, int w, int h, u32 color) {
    rect(x, y, w, 2, color);
    rect(x, y + h - 2, w, 2, color);
    rect(x, y, 2, h, color);
    rect(x + w - 2, y, 2, h, color);
}

int strcmp(const char* a, const char* b) {
    int i = 0;
    while (a[i] && b[i]) {
        if (a[i] != b[i]) return 1;
        i++;
    }
    return a[i] != b[i];
}

const char* glyph(char c) {
    if (c >= 'a' && c <= 'z') c -= 32;
    switch (c) {
        case 'A': return "01110""10001""10001""11111""10001""10001""10001";
        case 'B': return "11110""10001""10001""11110""10001""10001""11110";
        case 'C': return "01111""10000""10000""10000""10000""10000""01111";
        case 'D': return "11110""10001""10001""10001""10001""10001""11110";
        case 'E': return "11111""10000""10000""11110""10000""10000""11111";
        case 'F': return "11111""10000""10000""11110""10000""10000""10000";
        case 'G': return "01111""10000""10000""10111""10001""10001""01111";
        case 'H': return "10001""10001""10001""11111""10001""10001""10001";
        case 'I': return "11111""00100""00100""00100""00100""00100""11111";
        case 'J': return "00111""00010""00010""00010""10010""10010""01100";
        case 'K': return "10001""10010""10100""11000""10100""10010""10001";
        case 'L': return "10000""10000""10000""10000""10000""10000""11111";
        case 'M': return "10001""11011""10101""10101""10001""10001""10001";
        case 'N': return "10001""11001""10101""10011""10001""10001""10001";
        case 'O': return "01110""10001""10001""10001""10001""10001""01110";
        case 'P': return "11110""10001""10001""11110""10000""10000""10000";
        case 'Q': return "01110""10001""10001""10001""10101""10010""01101";
        case 'R': return "11110""10001""10001""11110""10100""10010""10001";
        case 'S': return "01111""10000""10000""01110""00001""00001""11110";
        case 'T': return "11111""00100""00100""00100""00100""00100""00100";
        case 'U': return "10001""10001""10001""10001""10001""10001""01110";
        case 'V': return "10001""10001""10001""10001""01010""01010""00100";
        case 'W': return "10001""10001""10001""10101""10101""11011""10001";
        case 'X': return "10001""01010""00100""00100""00100""01010""10001";
        case 'Y': return "10001""01010""00100""00100""00100""00100""00100";
        case 'Z': return "11111""00001""00010""00100""01000""10000""11111";
        case '0': return "01110""10001""10011""10101""11001""10001""01110";
        case '1': return "00100""01100""00100""00100""00100""00100""01110";
        case '2': return "01110""10001""00001""00010""00100""01000""11111";
        case '3': return "11110""00001""00001""01110""00001""00001""11110";
        case '4': return "10010""10010""10010""11111""00010""00010""00010";
        case '5': return "11111""10000""10000""11110""00001""00001""11110";
        case '6': return "01110""10000""10000""11110""10001""10001""01110";
        case '7': return "11111""00001""00010""00100""01000""01000""01000";
        case '8': return "01110""10001""10001""01110""10001""10001""01110";
        case '9': return "01110""10001""10001""01111""00001""00001""01110";
        case ':': return "00000""00100""00100""00000""00100""00100""00000";
        case '.': return "00000""00000""00000""00000""00000""01100""01100";
        case '-': return "00000""00000""00000""11111""00000""00000""00000";
        case '/': return "00001""00010""00010""00100""01000""01000""10000";
        case '@': return "01110""10001""10111""10101""10111""10000""01110";
        case '>': return "10000""01000""00100""00010""00100""01000""10000";
        case '_': return "00000""00000""00000""00000""00000""00000""11111";
        case '+': return "00000""00100""00100""11111""00100""00100""00000";
        case ' ': return "00000""00000""00000""00000""00000""00000""00000";
    }
    return "11111""10001""00110""00100""00100""00000""00100";
}

void draw_char(int x, int y, char c, u32 color) {
    const char* g = glyph(c);
    for (int yy = 0; yy < 7; yy++)
        for (int xx = 0; xx < 5; xx++)
            if (g[yy * 5 + xx] == '1')
                rect(x + xx * 2, y + yy * 2, 2, 2, color);
}

void text(int x, int y, const char* s, u32 color) {
    for (int i = 0; s[i]; i++) {
        draw_char(x, y, s[i], color);
        x += 12;
    }
}

void delay() {
    for (volatile int i = 0; i < 900000; i++) {}
}

void draw_taskbar() {
    rect(0, height - 48, width, 48, panel());
    rect(18, height - 38, 96, 28, accent());
    text(36, height - 30, "START", 0xFFFFFF);

    rect(130, height - 38, 132, 28, active_window == 1 ? accent() : 0xB0B0B0);
    text(145, height - 30, "TERMINAL", active_window == 1 ? 0xFFFFFF : 0x000000);

    rect(278, height - 38, 92, 28, active_window == 2 ? accent() : 0xB0B0B0);
    text(292, height - 30, "FILES", active_window == 2 ? 0xFFFFFF : 0x000000);

    rect(386, height - 38, 112, 28, active_window == 3 ? accent() : 0xB0B0B0);
    text(400, height - 30, "SETTINGS", active_window == 3 ? 0xFFFFFF : 0x000000);
}

void draw_titlebar(int x, int y, int w, const char* title) {
    rect(x, y, w, 32, accent());
    text(x + 14, y + 10, title, 0xFFFFFF);
}

void draw_terminal_window_at(int x, int y) {
    rect(x, y, 760, 460, win_bg());
    border(x, y, 760, 460, accent());
    draw_titlebar(x, y, 760, "KERNELX TERMINAL");
    rect(x + 12, y + 48, 736, 390, term_bg());
}

void draw_explorer_window_at(int x, int y) {
    rect(x, y, 420, 420, win_bg());
    border(x, y, 420, 420, accent());
    draw_titlebar(x, y, 420, "FILE EXPLORER");

    text(x + 20, y + 55, "KFIELD", accent());
    const char* items[5] = {"@USER/MAMIKK", "@CORE", "@APPS", "@VAULT", "PROJECTS"};
    for (int i = 0; i < 5; i++) {
        int yy = y + 90 + i * 34;
        if (file_selected == i && active_window == 2) rect(x + 15, yy - 8, 360, 26, accent());
        text(x + 25, yy, items[i], file_selected == i && active_window == 2 ? 0xFFFFFF : txt());
    }
}

void draw_settings_window_at(int x, int y) {
    rect(x, y, 460, 370, win_bg());
    border(x, y, 460, 370, accent());
    draw_titlebar(x, y, 460, "SETTINGS");

    const char* items[4] = {"THEME", "SYSTEM INFO", "BRIGHTNESS", "ABOUT KERNELX"};
    for (int i = 0; i < 4; i++) {
        int yy = y + 70 + i * 48;
        if (settings_selected == i && active_window == 3) rect(x + 18, yy - 8, 390, 28, accent());
        text(x + 30, yy, items[i], settings_selected == i && active_window == 3 ? 0xFFFFFF : txt());
    }

    text(x + 30, y + 285, theme ? "THEME: DARK" : "THEME: LIGHT", txt());
    text(x + 30, y + 315, brightness == 100 ? "BRIGHTNESS: 100" : "BRIGHTNESS: 70", txt());
}

void menu_item(int x, int y, const char* label, int selected, u32 color) {
    if (selected) rect(x - 10, y - 8, 220, 26, accent());
    text(x, y, label, selected ? 0xFFFFFF : color);
}

void draw_start_menu() {
    if (!start_open) return;

    rect(20, height - 320, 280, 270, win_bg());
    border(20, height - 320, 280, 270, accent());
    text(45, height - 295, "KERNELX MENU", accent());

    menu_item(45, height - 250, "TERMINAL", start_selected == 0, txt());
    menu_item(45, height - 220, "FILE EXPLORER", start_selected == 1, txt());
    menu_item(45, height - 190, "SETTINGS", start_selected == 2, txt());
    menu_item(45, height - 160, "FASTFETCH", start_selected == 3, txt());
    menu_item(45, height - 130, "POWER OFF", start_selected == 4, 0xFF3333);
}

void draw_desktop() {
    rect(0, 0, width, height, bg());
    rect(0, 0, width, 38, panel());
    text(20, 12, "KERNELX DESKTOP", accent());

    text(70, 70, "KX", accent());
    text(45, 95, "TERMINAL", txt());
    text(190, 70, "KF", accent());
    text(175, 95, "FILES", txt());
    text(310, 70, "ST", accent());
    text(285, 95, "SETTINGS", txt());

    if (terminal_open && active_window != 1) draw_terminal_window_at(420, 150);
    if (explorer_open && active_window != 2) draw_explorer_window_at(560, 120);
    if (settings_open && active_window != 3) draw_settings_window_at(530, 150);

    if (terminal_open && active_window == 1) draw_terminal_window_at(120, 140);
    if (explorer_open && active_window == 2) draw_explorer_window_at(500, 120);
    if (settings_open && active_window == 3) draw_settings_window_at(460, 150);

    draw_taskbar();
    draw_start_menu();
}

void clear_terminal_area() {
    rect(132, 188, 736, 390, term_bg());
    cursor_x = 145;
    cursor_y = 205;
}

void newline() {
    cursor_x = 145;
    cursor_y += 18;
    if (cursor_y > 550) clear_terminal_area();
}

void tprint(const char* s, u32 color) {
    for (int i = 0; s[i]; i++) {
        if (s[i] == '\n') newline();
        else {
            draw_char(cursor_x, cursor_y, s[i], color);
            cursor_x += 12;
        }
    }
}

void prompt() {
    tprint("\nMAMIKK ", 0x00AA44);
    tprint(current_path, accent());
    tprint(" > ", txt());
}

void redraw() {
    draw_desktop();
    if (terminal_open) {
        clear_terminal_area();
        tprint("KernelX Terminal ready\n", accent());
        prompt();
    }
}

void animate_open(int app) {
    int sx = width + 30;
    int y = app == 1 ? 140 : 120;
    for (int x = sx; x > (app == 1 ? 120 : 500); x -= 90) {
        draw_desktop();
        if (app == 1) draw_terminal_window_at(x, y);
        if (app == 2) draw_explorer_window_at(x, y);
        if (app == 3) draw_settings_window_at(x, 150);
        delay();
    }
}

void open_terminal() {
    terminal_open = 1;
    active_window = 1;
    start_open = 0;
    animate_open(1);
    redraw();
}

void open_files() {
    explorer_open = 1;
    active_window = 2;
    start_open = 0;
    animate_open(2);
    redraw();
}

void open_settings() {
    settings_open = 1;
    active_window = 3;
    start_open = 0;
    animate_open(3);
    redraw();
}

void power_off_screen() {
    powered_off = 1;
    rect(0, 0, width, height, 0x000000);
    text(360, 300, "KERNELX POWERED OFF", 0xFFFFFF);
    text(330, 340, "YOU CAN CLOSE QEMU NOW", 0x888888);
    halt_forever();
}

void fastfetch() {
    tprint("\nKX  SYSTEM     KERNELX OS V0.7", txt());
    tprint("\nKX  KERNEL     KERNELX CORE", txt());
    tprint("\nKX  ARCH       X86 32-BIT", txt());
    tprint("\nKX  CPU        GENERIC X86 CPU", txt());
    tprint("\nKX  GPU        VGA FRAMEBUFFER", txt());
    tprint("\nKX  DE         KERNELX DESKTOP", txt());
    tprint("\nKX  THEME      ", txt());
    tprint(theme ? "DARK" : "LIGHT", accent());
    tprint("\nKX  FS         KFIELD", txt());
    tprint("\n", txt());
}

void run_start_selected() {
    if (start_selected == 0) open_terminal();
    else if (start_selected == 1) open_files();
    else if (start_selected == 2) open_settings();
    else if (start_selected == 3) {
        open_terminal();
        fastfetch();
    } else if (start_selected == 4) power_off_screen();
}

void run_settings_selected() {
    if (settings_selected == 0) {
        theme = !theme;
        redraw();
    } else if (settings_selected == 1) {
        open_terminal();
        fastfetch();
    } else if (settings_selected == 2) {
        brightness = brightness == 100 ? 70 : 100;
        redraw();
    } else if (settings_selected == 3) {
        open_terminal();
        tprint("\nKernelX OS v0.7\nBuilt from the core\n", accent());
    }
}

void run_command() {
    input[input_len] = '\0';

    if (strcmp(input, "clear") == 0) clear_terminal_area();
    else if (strcmp(input, "fastfetch") == 0) fastfetch();
    else if (strcmp(input, "start") == 0) { start_open = 1; redraw(); }
    else if (strcmp(input, "files") == 0) open_files();
    else if (strcmp(input, "settings") == 0) open_settings();
    else if (strcmp(input, "terminal") == 0) open_terminal();
    else if (strcmp(input, "poweroff") == 0) power_off_screen();
    else if (input_len > 0) {
        tprint("\nUnknown command: ", 0xFF3333);
        tprint(input, 0xFF3333);
        tprint("\n", txt());
    }

    input_len = 0;
    prompt();
}

char scancode_to_char(unsigned char sc) {
    char map[58] = {
        0, 27, '1','2','3','4','5','6','7','8','9','0','-','=', '\b',
        '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
        0,'a','s','d','f','g','h','j','k','l',';','\'','`',
        0,'\\','z','x','c','v','b','n','m',',','.','/',0,
        '*',0,' '
    };
    if (sc < 58) return map[sc];
    return 0;
}

void keyboard_loop() {
    while (1) {
        if (!(inb(0x64) & 1)) continue;

        unsigned char sc = inb(0x60);

        if (sc == 0xE0) {
            extended_scancode = 1;
            continue;
        }

        if (extended_scancode && (sc == 0x5B || sc == 0x5C)) {
            super_down = 1;
            start_open = 1;
            redraw();
            extended_scancode = 0;
            continue;
        }

        if (extended_scancode && (sc == 0xDB || sc == 0xDC)) {
            super_down = 0;
            extended_scancode = 0;
            continue;
        }

        if (sc & 0x80) {
            extended_scancode = 0;
            continue;
        }

        if (super_down && sc == 0x02) { open_terminal(); continue; }
        if (super_down && sc == 0x03) { open_files(); continue; }
        if (super_down && sc == 0x04) { open_settings(); continue; }

        if (start_open) {
            if (extended_scancode && sc == 0x48) {
                if (start_selected > 0) start_selected--;
                redraw();
            } else if (extended_scancode && sc == 0x50) {
                if (start_selected < 4) start_selected++;
                redraw();
            } else if (sc == 0x1C) {
                run_start_selected();
            } else if (sc == 0x01) {
                start_open = 0;
                redraw();
            }
            extended_scancode = 0;
            continue;
        }

        if (active_window == 2) {
            if (extended_scancode && sc == 0x48) {
                if (file_selected > 0) file_selected--;
                redraw();
            } else if (extended_scancode && sc == 0x50) {
                if (file_selected < 4) file_selected++;
                redraw();
            } else if (sc == 0x01) {
                explorer_open = 0;
                active_window = terminal_open ? 1 : 0;
                redraw();
            }
            extended_scancode = 0;
            continue;
        }

        if (active_window == 3) {
            if (extended_scancode && sc == 0x48) {
                if (settings_selected > 0) settings_selected--;
                redraw();
            } else if (extended_scancode && sc == 0x50) {
                if (settings_selected < 3) settings_selected++;
                redraw();
            } else if (sc == 0x1C) {
                run_settings_selected();
            } else if (sc == 0x01) {
                settings_open = 0;
                active_window = terminal_open ? 1 : 0;
                redraw();
            }
            extended_scancode = 0;
            continue;
        }

        if (active_window != 1 || !terminal_open) {
            extended_scancode = 0;
            continue;
        }

        char c = scancode_to_char(sc);

        if (c == '\n') run_command();
        else if (c == '\b') {
            if (input_len > 0) {
                input_len--;
                cursor_x -= 12;
                rect(cursor_x, cursor_y, 12, 16, term_bg());
            }
        } else if (c && input_len < 127) {
            input[input_len++] = c;
            draw_char(cursor_x, cursor_y, c, txt());
            cursor_x += 12;
        }

        extended_scancode = 0;
    }
}

void kernel_main(u32 magic, struct multiboot_info* mbi) {
    fb = (u32*)(u32)mbi->framebuffer_addr;
    width = mbi->framebuffer_width;
    height = mbi->framebuffer_height;
    pitch = mbi->framebuffer_pitch;

    draw_desktop();
    keyboard_loop();
}
