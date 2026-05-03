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

int term_x = 40;
int term_y = 100;
int cursor_x = 52;
int cursor_y = 155;

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
void draw_text(int x, int y, const char* text, u32 color) {
    int start = x;
    for (int i = 0; text[i]; i++) {
        if (text[i] == '\n') {
            y += 14;
            x = start;
        } else {
            draw_char(x, y, text[i], color);
            x += 8;
        }
    }
}

void terminal_newline() {
    cursor_x = term_x + 12;
    cursor_y += 16;

    if (cursor_y > 690) {
        rect(term_x + 10, term_y + 45, 840, 570, 0x050507);
        cursor_y = term_y + 55;
    }
}

void tprint(const char* text, u32 color) {
    for (int i = 0; text[i]; i++) {
        if (text[i] == '\n') {
            terminal_newline();
        } else {
            draw_char(cursor_x, cursor_y, text[i], color);
            cursor_x += 8;
        }
    }
}

void draw_gui() {
    rect(0, 0, width, height, 0x101018);

    rect(0, 0, width, 36, 0x161625);
    draw_text(20, 10, "KernelX Desktop", 0x00AAFF);

    rect(0, height - 46, width, 46, 0x161625);
    rect(20, height - 36, 120, 28, 0x00AAFF);
    draw_text(42, height - 28, "Terminal", 0xFFFFFF);

    rect(term_x, term_y, 860, 620, 0x202030);
    border(term_x, term_y, 860, 620, 0x00AAFF);
    rect(term_x, term_y, 860, 34, 0x00AAFF);
    draw_text(term_x + 12, term_y + 10, "KernelX Terminal", 0xFFFFFF);

    rect(term_x + 10, term_y + 45, 840, 570, 0x050507);

    cursor_x = term_x + 12;
    cursor_y = term_y + 55;
}

void prompt() {
    tprint("\nmamikk ", 0x00FF66);
    tprint(current_path, 0x00AAFF);
    tprint(" > ", 0xFFFFFF);
}

void fastfetch() {
    tprint("\nKX  System: KernelX OS v0.5", 0xFFFFFF);
    tprint("\nKX  Kernel: KernelX Core", 0xFFFFFF);
    tprint("\nKX  Arch: x86 32-bit", 0xFFFFFF);
    tprint("\nKX  CPU: Generic x86 CPU", 0xFFFFFF);
    tprint("\nKX  GPU: VGA Framebuffer", 0xFFFFFF);
    tprint("\nKX  Display: Graphical framebuffer", 0xFFFFFF);
    tprint("\nKX  DE: KernelX Desktop", 0xFFFFFF);
    tprint("\nKX  Terminal: KernelX Terminal App", 0xFFFFFF);
    tprint("\nKX  Shell: kxsh", 0xFFFFFF);
    tprint("\nKX  FS: KField", 0xFFFFFF);
    tprint("\nKX  Path: ", 0xFFFFFF);
    tprint(current_path, 0x00AAFF);
    tprint("\n", 0xFFFFFF);
}

void help() {
    tprint("\nCommands:", 0x00AAFF);
    tprint("\nhelp       show commands", 0xFFFFFF);
    tprint("\nclear      clear terminal", 0xFFFFFF);
    tprint("\nfastfetch  show system info", 0xFFFFFF);
    tprint("\nwhere      show KField sector", 0xFFFFFF);
    tprint("\nlist       list virtual files", 0xFFFFFF);
    tprint("\ngoto core  go to @core", 0xFFFFFF);
    tprint("\ngoto apps  go to @apps", 0xFFFFFF);
    tprint("\ngoto user  go to @user/mamikk", 0xFFFFFF);
    tprint("\n", 0xFFFFFF);
}

void list() {
    tprint("\n", 0xFFFFFF);

    if (strcmp(current_path, "@user/mamikk") == 0) {
        tprint("notes.kx\nprojects\nprofile.kx", 0xFFFFFF);
    } else if (strcmp(current_path, "@core") == 0) {
        tprint("kernel.core\nframebuffer.driver\nkeyboard.driver", 0xFFFFFF);
    } else if (strcmp(current_path, "@apps") == 0) {
        tprint("terminal.app\nfastfetch.app\nfiles.app", 0xFFFFFF);
    } else {
        tprint("empty sector", 0x888888);
    }

    tprint("\n", 0xFFFFFF);
}

void clear_terminal() {
    rect(term_x + 10, term_y + 45, 840, 570, 0x050507);
    cursor_x = term_x + 12;
    cursor_y = term_y + 55;
}

void run_command() {
    input[input_len] = '\0';

    if (strcmp(input, "clear") == 0) {
        clear_terminal();
    } else if (strcmp(input, "help") == 0) {
        help();
    } else if (strcmp(input, "fastfetch") == 0) {
        fastfetch();
    } else if (strcmp(input, "where") == 0) {
        tprint("\nCurrent sector: ", 0xFFFFFF);
        tprint(current_path, 0x00AAFF);
        tprint("\n", 0xFFFFFF);
    } else if (strcmp(input, "list") == 0) {
        list();
    } else if (strcmp(input, "goto core") == 0) {
        current_path = "@core";
        tprint("\nMoved to @core\n", 0x00FF66);
    } else if (strcmp(input, "goto apps") == 0) {
        current_path = "@apps";
        tprint("\nMoved to @apps\n", 0x00FF66);
    } else if (strcmp(input, "goto user") == 0) {
        current_path = "@user/mamikk";
        tprint("\nMoved to @user/mamikk\n", 0x00FF66);
    } else if (input_len > 0) {
        tprint("\nUnknown command: ", 0xFF4444);
        tprint(input, 0xFF4444);
        tprint("\n", 0xFFFFFF);
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
        if (inb(0x64) & 1) {
            unsigned char sc = inb(0x60);
            if (sc & 0x80) continue;

            char c = scancode_to_char(sc);

            if (c == '\n') {
                run_command();
            } else if (c == '\b') {
                if (input_len > 0) {
                    input_len--;
                    cursor_x -= 8;
                    rect(cursor_x, cursor_y, 8, 12, 0x050507);
                }
            } else if (c && input_len < 127) {
                input[input_len++] = c;
                draw_char(cursor_x, cursor_y, c, 0xFFFFFF);
                cursor_x += 8;
            }
        }
    }
}

void kernel_main(u32 magic, struct multiboot_info* mbi) {
    fb = (u32*)(u32)mbi->framebuffer_addr;
    width = mbi->framebuffer_width;
    height = mbi->framebuffer_height;
    pitch = mbi->framebuffer_pitch;

    draw_gui();

    tprint("KernelX OS v0.5\n", 0x00AAFF);
    tprint("KernelX Desktop loaded\n", 0x00FF66);
    tprint("Type 'help' or 'fastfetch'\n", 0x888888);

    prompt();
    keyboard_loop();
}
