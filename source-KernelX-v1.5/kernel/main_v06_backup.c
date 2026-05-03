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
int start_open = 0;

int cursor_x = 70;
int cursor_y = 185;

int active_window = 0; // 0 desktop, 1 terminal, 2 files
int super_down = 0;
int start_selected = 0;
int extended_scancode = 0;

char input[128];
int input_len = 0;
const char* current_path = "@user/mamikk";

unsigned char inb(unsigned short port) {
    unsigned char ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

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

int starts_with(const char* a, const char* b) {
    int i = 0;
    while (b[i]) {
        if (a[i] != b[i]) return 0;
        i++;
    }
    return 1;
}

/* Clean fake font: still bitmap, but smoother/better readable */
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
        case ' ': return "00000""00000""00000""00000""00000""00000""00000";
    }

    return "11111""10001""00110""00100""00100""00000""00100";
}

void draw_char(int x, int y, char c, u32 color) {
    const char* g = glyph(c);
    for (int yy = 0; yy < 7; yy++) {
        for (int xx = 0; xx < 5; xx++) {
            if (g[yy * 5 + xx] == '1') {
                rect(x + xx * 2, y + yy * 2, 2, 2, color);
            }
        }
    }
}

void text(int x, int y, const char* s, u32 color) {
    for (int i = 0; s[i]; i++) {
        draw_char(x, y, s[i], color);
        x += 12;
    }
}

void clear_terminal_area() {
    rect(60, 160, 860, 500, 0x050507);
    cursor_x = 70;
    cursor_y = 175;
}

void newline() {
    cursor_x = 70;
    cursor_y += 18;
    if (cursor_y > 640) clear_terminal_area();
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

void draw_taskbar() {
    rect(0, height - 48, width, 48, 0x151522);
    rect(18, height - 38, 96, 28, 0x00AEEF);
    text(36, height - 30, "START", 0xFFFFFF);

    rect(130, height - 38, 132, 28, terminal_open ? 0x303048 : 0x222232);
    text(145, height - 30, "TERMINAL", 0xFFFFFF);

    rect(278, height - 38, 92, 28, explorer_open ? 0x303048 : 0x222232);
    text(292, height - 30, "FILES", 0xFFFFFF);
}

void menu_item(int x, int y, const char* label, int selected, u32 color) {
    if (selected) rect(x - 10, y - 8, 210, 26, 0x00AEEF);
    text(x, y, label, selected ? 0x000000 : color);
}

void draw_start_menu() {
    if (!start_open) return;

    rect(20, height - 300, 260, 250, 0x202034);
    border(20, height - 300, 260, 250, 0x00AEEF);
    text(45, height - 275, "KERNELX MENU", 0x00AEEF);

    menu_item(45, height - 235, "TERMINAL", start_selected == 0, 0xFFFFFF);
    menu_item(45, height - 205, "FILE EXPLORER", start_selected == 1, 0xFFFFFF);
    menu_item(45, height - 175, "FASTFETCH", start_selected == 2, 0xFFFFFF);
    menu_item(45, height - 145, "SETTINGS", start_selected == 3, 0x888888);
    menu_item(45, height - 115, "POWER", start_selected == 4, 0xFF5555);
}


void draw_terminal_window() {
    if (!terminal_open) return;

    rect(48, 108, 890, 590, 0x202030);
    border(48, 108, 890, 590, 0x00AEEF);
    rect(48, 108, 890, 34, 0x00AEEF);
    text(66, 118, "KERNELX TERMINAL", 0xFFFFFF);
    rect(60, 160, 860, 500, 0x050507);
}

void draw_explorer_window() {
    if (!explorer_open) return;

    rect(970, 108, 430, 420, 0x202030);
    border(970, 108, 430, 420, 0x00AEEF);
    rect(970, 108, 430, 34, 0x00AEEF);
    text(988, 118, "FILE EXPLORER", 0xFFFFFF);

    text(995, 160, "KFIELD SECTORS", 0x00AEEF);
    text(995, 200, "@USER/MAMIKK", 0xFFFFFF);
    text(995, 230, "@CORE", 0xFFFFFF);
    text(995, 260, "@APPS", 0xFFFFFF);
    text(995, 290, "@VAULT", 0xFFFFFF);

    text(995, 350, "FILES", 0x00AEEF);
    text(995, 385, "NOTES.KX", 0xFFFFFF);
    text(995, 415, "PROFILE.KX", 0xFFFFFF);
    text(995, 445, "PROJECTS", 0xFFFFFF);
}

void draw_desktop() {
    rect(0, 0, width, height, 0x0B1028);

    rect(0, 0, width, 38, 0x151522);
    text(20, 12, "KERNELX DESKTOP", 0x00AEEF);

    text(70, 70, "KX", 0x00AEEF);
    text(55, 95, "TERMINAL", 0xFFFFFF);

    text(180, 70, "KF", 0x00AEEF);
    text(165, 95, "FILES", 0xFFFFFF);

    if (terminal_open) draw_terminal_window();
    if (explorer_open) draw_explorer_window();

    draw_taskbar();

    if (start_open) draw_start_menu();
}
void prompt() {
    tprint("\nMAMIKK ", 0x00FF66);
    tprint(current_path, 0x00AEEF);
    tprint(" > ", 0xFFFFFF);
}

void fastfetch() {
    tprint("\nKX  SYSTEM     KERNELX OS V0.6", 0xFFFFFF);
    tprint("\nKX  KERNEL     KERNELX CORE", 0xFFFFFF);
    tprint("\nKX  ARCH       X86 32-BIT", 0xFFFFFF);
    tprint("\nKX  CPU        GENERIC X86 CPU", 0xFFFFFF);
    tprint("\nKX  GPU        VGA FRAMEBUFFER", 0xFFFFFF);
    tprint("\nKX  DE         KERNELX DESKTOP", 0xFFFFFF);
    tprint("\nKX  TERMINAL   KERNELX TERMINAL APP", 0xFFFFFF);
    tprint("\nKX  SHELL      KXSH", 0xFFFFFF);
    tprint("\nKX  FS         KFIELD", 0xFFFFFF);
    tprint("\nKX  PATH       ", 0xFFFFFF);
    tprint(current_path, 0x00AEEF);
    tprint("\n", 0xFFFFFF);
}

void help() {
    tprint("\nCOMMANDS", 0x00AEEF);
    tprint("\nHELP       SHOW COMMANDS", 0xFFFFFF);
    tprint("\nFASTFETCH  SYSTEM INFO", 0xFFFFFF);
    tprint("\nCLEAR      CLEAR TERMINAL", 0xFFFFFF);
    tprint("\nSTART      OPEN START MENU", 0xFFFFFF);
    tprint("\nFILES      OPEN FILE EXPLORER", 0xFFFFFF);
    tprint("\nTERMINAL   OPEN TERMINAL", 0xFFFFFF);
    tprint("\nWHERE      SHOW CURRENT SECTOR", 0xFFFFFF);
    tprint("\nLIST       LIST FILES", 0xFFFFFF);
    tprint("\nGOTO CORE  GO TO CORE", 0xFFFFFF);
    tprint("\nGOTO APPS  GO TO APPS", 0xFFFFFF);
    tprint("\nGOTO USER  GO TO USER", 0xFFFFFF);
    tprint("\n", 0xFFFFFF);
}

void list() {
    tprint("\n", 0xFFFFFF);
    if (strcmp(current_path, "@user/mamikk") == 0) {
        tprint("NOTES.KX\nPROFILE.KX\nPROJECTS", 0xFFFFFF);
    } else if (strcmp(current_path, "@core") == 0) {
        tprint("KERNEL.CORE\nFRAMEBUFFER.DRIVER\nKEYBOARD.DRIVER", 0xFFFFFF);
    } else if (strcmp(current_path, "@apps") == 0) {
        tprint("TERMINAL.APP\nFASTFETCH.APP\nFILES.APP", 0xFFFFFF);
    } else {
        tprint("EMPTY SECTOR", 0x888888);
    }
    tprint("\n", 0xFFFFFF);
}

void redraw_after_app_change() {
    draw_desktop();
}

void run_command() {
    input[input_len] = '\0';

    if (strcmp(input, "clear") == 0) {
        clear_terminal_area();
    } else if (strcmp(input, "help") == 0) {
        help();
    } else if (strcmp(input, "fastfetch") == 0) {
        fastfetch();
    } else if (strcmp(input, "start") == 0) {
        start_open = !start_open;
        redraw_after_app_change();
    } else if (strcmp(input, "files") == 0) {
        explorer_open = 1;
        redraw_after_app_change();
    } else if (strcmp(input, "terminal") == 0) {
        terminal_open = 1;
        redraw_after_app_change();
    } else if (strcmp(input, "where") == 0) {
        tprint("\nCURRENT SECTOR: ", 0xFFFFFF);
        tprint(current_path, 0x00AEEF);
        tprint("\n", 0xFFFFFF);
    } else if (strcmp(input, "list") == 0) {
        list();
    } else if (strcmp(input, "goto core") == 0) {
        current_path = "@core";
        tprint("\nMOVED TO @CORE\n", 0x00FF66);
    } else if (strcmp(input, "goto apps") == 0) {
        current_path = "@apps";
        tprint("\nMOVED TO @APPS\n", 0x00FF66);
    } else if (strcmp(input, "goto user") == 0) {
        current_path = "@user/mamikk";
        tprint("\nMOVED TO @USER/MAMIKK\n", 0x00FF66);
    } else if (input_len > 0) {
        tprint("\nUNKNOWN COMMAND: ", 0xFF5555);
        tprint(input, 0xFF5555);
        tprint("\n", 0xFFFFFF);
    }

    input_len = 0;
    prompt();
}

void open_selected_start_item() {
    if (start_selected == 0) {
        terminal_open = 1;
        active_window = 1;
    } else if (start_selected == 1) {
        explorer_open = 1;
        active_window = 2;
    } else if (start_selected == 2) {
        terminal_open = 1;
        active_window = 1;
        start_open = 0;
        redraw_after_app_change();
        fastfetch();
        return;
    }

    start_open = 0;
    redraw_after_app_change();
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
        if (inb(0x64) & 1) {
            unsigned char sc = inb(0x60);

            if (sc == 0xE0) {
                extended_scancode = 1;
                continue;
            }

            // Win/Super press
            if (extended_scancode && (sc == 0x5B || sc == 0x5C)) {
                super_down = 1;
                start_open = 1;
                redraw_after_app_change();
                extended_scancode = 0;
                continue;
            }

            // Win/Super release
            if (extended_scancode && (sc == 0xDB || sc == 0xDC)) {
                super_down = 0;
                extended_scancode = 0;
                continue;
            }

            // Win + 1 = Terminal
            if (super_down && sc == 0x02) {
                start_open = 0;
                terminal_open = 1;
                active_window = 1;
                redraw_after_app_change();
                clear_terminal_area();
                tprint("KernelX Terminal ready\n", 0x00AEEF);
                prompt();
                continue;
            }

            // Win + 2 = File Explorer
            if (super_down && sc == 0x03) {
                start_open = 0;
                explorer_open = 1;
                active_window = 2;
                redraw_after_app_change();
                continue;
            }

            if (sc & 0x80) {
                extended_scancode = 0;
                continue;
            }

            // If Start Menu is open, control ONLY menu
            if (start_open) {
                if (extended_scancode && sc == 0x48) { // Up
                    if (start_selected > 0) start_selected--;
                    redraw_after_app_change();
                    extended_scancode = 0;
                    continue;
                }

                if (extended_scancode && sc == 0x50) { // Down
                    if (start_selected < 4) start_selected++;
                    redraw_after_app_change();
                    extended_scancode = 0;
                    continue;
                }

                if (sc == 0x1C) { // Enter
                    open_selected_start_item();
                    extended_scancode = 0;
                    continue;
                }

                if (sc == 0x01) { // Esc
                    start_open = 0;
                    redraw_after_app_change();
                    extended_scancode = 0;
                    continue;
                }

                extended_scancode = 0;
                continue;
            }

            // No terminal = no text input
            if (!terminal_open || active_window != 1) {
                extended_scancode = 0;
                continue;
            }

            char c = scancode_to_char(sc);

            if (c == '\n') {
                run_command();
            } else if (c == '\b') {
                if (input_len > 0) {
                    input_len--;
                    cursor_x -= 12;
                    rect(cursor_x, cursor_y, 12, 16, 0x050507);
                }
            } else if (c && input_len < 127) {
                input[input_len++] = c;
                draw_char(cursor_x, cursor_y, c, 0xFFFFFF);
                cursor_x += 12;
            }

            extended_scancode = 0;
        }
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
