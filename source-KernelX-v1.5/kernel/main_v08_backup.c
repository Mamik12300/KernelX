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

int terminal_open = 0, explorer_open = 0, settings_open = 0, start_open = 0;
int active_window = 0;
int super_down = 0, extended_scancode = 0;
int start_selected = 0, file_selected = 0, settings_selected = 0;
int theme = 0, brightness = 100;

int term_x = 120, term_y = 140;
int file_x = 500, file_y = 120;
int set_x = 460, set_y = 150;

int cursor_x = 145, cursor_y = 205;
char input[128];
int input_len = 0;
const char* current_path = "@user/mamikk";

int mx = 400, my = 300;
int mouse_left = 0, mouse_right = 0;
int old_left = 0, old_right = 0;
int drag_window = 0;
int drag_dx = 0, drag_dy = 0;
int context_menu = 0, ctx_x = 0, ctx_y = 0;
unsigned char mouse_packet[3];
int mouse_cycle = 0;

unsigned char inb(unsigned short port) {
    unsigned char ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

void outb(unsigned short port, unsigned char val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

void io_wait() {
    __asm__ volatile ("outb %%al, $0x80" : : "a"(0));
}

void mouse_wait_input() {
    for (int i = 0; i < 100000; i++) {
        if ((inb(0x64) & 2) == 0) return;
    }
}

void mouse_wait_output() {
    for (int i = 0; i < 100000; i++) {
        if (inb(0x64) & 1) return;
    }
}

void mouse_write(unsigned char val) {
    mouse_wait_input();
    outb(0x64, 0xD4);
    mouse_wait_input();
    outb(0x60, val);
}

void mouse_init() {
    mouse_wait_input();
    outb(0x64, 0xA8);

    mouse_wait_input();
    outb(0x64, 0x20);
    mouse_wait_output();
    unsigned char status = inb(0x60) | 2;

    mouse_wait_input();
    outb(0x64, 0x60);
    mouse_wait_input();
    outb(0x60, status);

    mouse_write(0xF6);
    mouse_wait_output();
    inb(0x60);

    mouse_write(0xF4);
    mouse_wait_output();
    inb(0x60);
}

void halt_forever() {
    while (1) __asm__ volatile ("hlt");
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

int inside(int x, int y, int rx, int ry, int rw, int rh) {
    return x >= rx && y >= ry && x < rx + rw && y < ry + rh;
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
        case 'R': return "11110""10001""10001""11110""10100""10010""10001";
        case 'S': return "01111""10000""10000""01110""00001""00001""11110";
        case 'T': return "11111""00100""00100""00100""00100""00100""00100";
        case 'U': return "10001""10001""10001""10001""10001""10001""01110";
        case 'W': return "10001""10001""10001""10101""10101""11011""10001";
        case 'X': return "10001""01010""00100""00100""00100""01010""10001";
        case 'Y': return "10001""01010""00100""00100""00100""00100""00100";
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

void draw_mouse() {
    rect(mx, my, 2, 18, 0x000000);
    rect(mx + 2, my + 2, 2, 14, 0x000000);
    rect(mx + 4, my + 4, 2, 10, 0x000000);
    rect(mx + 6, my + 6, 2, 8, 0x000000);
    rect(mx + 8, my + 8, 2, 6, 0x000000);
    rect(mx + 2, my + 2, 2, 10, 0xFFFFFF);
    rect(mx + 4, my + 4, 2, 8, 0xFFFFFF);
    rect(mx + 6, my + 6, 2, 4, 0xFFFFFF);
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

void titlebar(int x, int y, int w, const char* t) {
    rect(x, y, w, 32, accent());
    text(x + 14, y + 10, t, 0xFFFFFF);
    rect(x + w - 34, y + 7, 20, 18, 0xFF4444);
    text(x + w - 29, y + 10, "X", 0xFFFFFF);
}

void draw_terminal_window() {
    rect(term_x, term_y, 760, 460, win_bg());
    border(term_x, term_y, 760, 460, accent());
    titlebar(term_x, term_y, 760, "KERNELX TERMINAL");
    rect(term_x + 12, term_y + 48, 736, 390, term_bg());
}

void draw_explorer_window() {
    rect(file_x, file_y, 420, 420, win_bg());
    border(file_x, file_y, 420, 420, accent());
    titlebar(file_x, file_y, 420, "FILE EXPLORER");
    text(file_x + 20, file_y + 55, "KFIELD", accent());
    const char* items[5] = {"@USER/MAMIKK", "@CORE", "@APPS", "@VAULT", "PROJECTS"};
    for (int i = 0; i < 5; i++) {
        int yy = file_y + 90 + i * 34;
        if (file_selected == i && active_window == 2) rect(file_x + 15, yy - 8, 360, 26, accent());
        text(file_x + 25, yy, items[i], file_selected == i && active_window == 2 ? 0xFFFFFF : txt());
    }
}

void draw_settings_window() {
    rect(set_x, set_y, 460, 370, win_bg());
    border(set_x, set_y, 460, 370, accent());
    titlebar(set_x, set_y, 460, "SETTINGS");
    const char* items[4] = {"THEME", "SYSTEM INFO", "BRIGHTNESS", "ABOUT KERNELX"};
    for (int i = 0; i < 4; i++) {
        int yy = set_y + 70 + i * 48;
        if (settings_selected == i && active_window == 3) rect(set_x + 18, yy - 8, 390, 28, accent());
        text(set_x + 30, yy, items[i], settings_selected == i && active_window == 3 ? 0xFFFFFF : txt());
    }
    text(set_x + 30, set_y + 285, theme ? "THEME: DARK" : "THEME: LIGHT", txt());
    text(set_x + 30, set_y + 315, brightness == 100 ? "BRIGHTNESS: 100" : "BRIGHTNESS: 70", txt());
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

void draw_context_menu() {
    if (!context_menu) return;
    rect(ctx_x, ctx_y, 260, 210, win_bg());
    border(ctx_x, ctx_y, 260, 210, accent());
    text(ctx_x + 20, ctx_y + 20, "OPEN", txt());
    text(ctx_x + 20, ctx_y + 55, "NEW", txt());
    text(ctx_x + 20, ctx_y + 90, "REFRESH", txt());
    text(ctx_x + 20, ctx_y + 125, "PROPERTIES", txt());
    text(ctx_x + 20, ctx_y + 160, "KFIELD INFO", accent());
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

    if (terminal_open && active_window != 1) draw_terminal_window();
    if (explorer_open && active_window != 2) draw_explorer_window();
    if (settings_open && active_window != 3) draw_settings_window();

    if (terminal_open && active_window == 1) draw_terminal_window();
    if (explorer_open && active_window == 2) draw_explorer_window();
    if (settings_open && active_window == 3) draw_settings_window();

    draw_taskbar();
    draw_start_menu();
    draw_context_menu();
    draw_mouse();
}

void clear_terminal_area() {
    rect(term_x + 12, term_y + 48, 736, 390, term_bg());
    cursor_x = term_x + 25;
    cursor_y = term_y + 65;
}

void newline() {
    cursor_x = term_x + 25;
    cursor_y += 18;
    if (cursor_y > term_y + 410) clear_terminal_area();
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
    if (terminal_open && active_window == 1) {
        clear_terminal_area();
        tprint("KernelX Terminal ready\n", accent());
        prompt();
    }
}

void open_terminal() {
    terminal_open = 1;
    active_window = 1;
    start_open = 0;
    context_menu = 0;
    redraw();
}

void open_files() {
    explorer_open = 1;
    active_window = 2;
    start_open = 0;
    context_menu = 0;
    redraw();
}

void open_settings() {
    settings_open = 1;
    active_window = 3;
    start_open = 0;
    context_menu = 0;
    redraw();
}

void power_off_screen() {
    rect(0, 0, width, height, 0x000000);
    text(360, 300, "KERNELX POWERED OFF", 0xFFFFFF);
    text(330, 340, "YOU CAN CLOSE QEMU NOW", 0x888888);
    halt_forever();
}

void fastfetch() {
    tprint("\nKX  SYSTEM     KERNELX OS V0.8", txt());
    tprint("\nKX  KERNEL     KERNELX CORE", txt());
    tprint("\nKX  MOUSE      PS2 MOUSE", txt());
    tprint("\nKX  DE         KERNELX DESKTOP", txt());
    tprint("\nKX  FS         KFIELD", txt());
    tprint("\n", txt());
}

void run_start_selected() {
    if (start_selected == 0) open_terminal();
    else if (start_selected == 1) open_files();
    else if (start_selected == 2) open_settings();
    else if (start_selected == 3) { open_terminal(); fastfetch(); }
    else if (start_selected == 4) power_off_screen();
}

void run_settings_selected() {
    if (settings_selected == 0) { theme = !theme; redraw(); }
    else if (settings_selected == 1) { open_terminal(); fastfetch(); }
    else if (settings_selected == 2) { brightness = brightness == 100 ? 70 : 100; redraw(); }
    else if (settings_selected == 3) { open_terminal(); tprint("\nKernelX OS v0.8\nMouse build\n", accent()); }
}

void open_file_item() {
    open_terminal();
    tprint("\nOpened KField item\n", accent());
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
    else if (input_len > 0) { tprint("\nUnknown command\n", 0xFF3333); }
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

void mouse_left_click() {
    context_menu = 0;

    if (inside(mx, my, 18, height - 38, 96, 28)) { start_open = 1; redraw(); return; }
    if (inside(mx, my, 130, height - 38, 132, 28)) { open_terminal(); return; }
    if (inside(mx, my, 278, height - 38, 92, 28)) { open_files(); return; }
    if (inside(mx, my, 386, height - 38, 112, 28)) { open_settings(); return; }

    if (start_open) {
        int base = height - 250;
        for (int i = 0; i < 5; i++) {
            if (inside(mx, my, 35, base + i * 30 - 8, 240, 26)) {
                start_selected = i;
                run_start_selected();
                return;
            }
        }
    }

    if (terminal_open && inside(mx, my, term_x, term_y, 760, 32)) {
        active_window = 1;
        drag_window = 1;
        drag_dx = mx - term_x;
        drag_dy = my - term_y;
        redraw();
        return;
    }

    if (explorer_open && inside(mx, my, file_x, file_y, 420, 32)) {
        active_window = 2;
        drag_window = 2;
        drag_dx = mx - file_x;
        drag_dy = my - file_y;
        redraw();
        return;
    }

    if (settings_open && inside(mx, my, set_x, set_y, 460, 32)) {
        active_window = 3;
        drag_window = 3;
        drag_dx = mx - set_x;
        drag_dy = my - set_y;
        redraw();
        return;
    }

    if (explorer_open && inside(mx, my, file_x, file_y, 420, 420)) {
        active_window = 2;
        for (int i = 0; i < 5; i++) {
            int yy = file_y + 90 + i * 34;
            if (inside(mx, my, file_x + 15, yy - 8, 360, 26)) {
                file_selected = i;
                open_file_item();
                return;
            }
        }
        redraw();
        return;
    }

    if (settings_open && inside(mx, my, set_x, set_y, 460, 370)) {
        active_window = 3;
        for (int i = 0; i < 4; i++) {
            int yy = set_y + 70 + i * 48;
            if (inside(mx, my, set_x + 18, yy - 8, 390, 28)) {
                settings_selected = i;
                run_settings_selected();
                return;
            }
        }
        redraw();
        return;
    }

    redraw();
}

void mouse_right_click() {
    if (explorer_open && inside(mx, my, file_x, file_y, 420, 420)) {
        context_menu = 1;
        ctx_x = mx;
        ctx_y = my;
        if (ctx_x + 260 > (int)width) ctx_x = width - 270;
        if (ctx_y + 210 > (int)height) ctx_y = height - 220;
        redraw();
    }
}

void process_mouse_packet() {
    int dx = (int)((signed char)mouse_packet[1]);
    int dy = (int)((signed char)mouse_packet[2]);

    mx += dx;
    my -= dy;

    if (mx < 0) mx = 0;
    if (my < 0) my = 0;
    if (mx > (int)width - 10) mx = width - 10;
    if (my > (int)height - 10) my = height - 10;

    old_left = mouse_left;
    old_right = mouse_right;

    mouse_left = mouse_packet[0] & 1;
    mouse_right = mouse_packet[0] & 2;

    if (mouse_left && !old_left) mouse_left_click();
    if (mouse_right && !old_right) mouse_right_click();

    if (!mouse_left) drag_window = 0;

    if (mouse_left && drag_window) {
        if (drag_window == 1) { term_x = mx - drag_dx; term_y = my - drag_dy; }
        if (drag_window == 2) { file_x = mx - drag_dx; file_y = my - drag_dy; }
        if (drag_window == 3) { set_x = mx - drag_dx; set_y = my - drag_dy; }

        if (term_x < 0) term_x = 0;
        if (term_y < 40) term_y = 40;
        if (file_x < 0) file_x = 0;
        if (file_y < 40) file_y = 40;
        if (set_x < 0) set_x = 0;
        if (set_y < 40) set_y = 40;

        redraw();
        return;
    }

    redraw();
}

void handle_keyboard(unsigned char sc) {
    if (sc == 0xE0) { extended_scancode = 1; return; }

    if (extended_scancode && (sc == 0x5B || sc == 0x5C)) {
        super_down = 1;
        start_open = 1;
        redraw();
        extended_scancode = 0;
        return;
    }

    if (extended_scancode && (sc == 0xDB || sc == 0xDC)) {
        super_down = 0;
        extended_scancode = 0;
        return;
    }

    if (sc & 0x80) { extended_scancode = 0; return; }

    if (super_down && sc == 0x02) { open_terminal(); return; }
    if (super_down && sc == 0x03) { open_files(); return; }
    if (super_down && sc == 0x04) { open_settings(); return; }

    if (start_open) {
        if (extended_scancode && sc == 0x48 && start_selected > 0) start_selected--;
        else if (extended_scancode && sc == 0x50 && start_selected < 4) start_selected++;
        else if (sc == 0x1C) run_start_selected();
        else if (sc == 0x01) start_open = 0;
        redraw();
        extended_scancode = 0;
        return;
    }

    if (active_window == 2) {
        if (extended_scancode && sc == 0x48 && file_selected > 0) file_selected--;
        else if (extended_scancode && sc == 0x50 && file_selected < 4) file_selected++;
        else if (sc == 0x1C) open_file_item();
        else if (sc == 0x01) { explorer_open = 0; active_window = terminal_open ? 1 : 0; }
        redraw();
        extended_scancode = 0;
        return;
    }

    if (active_window == 3) {
        if (extended_scancode && sc == 0x48 && settings_selected > 0) settings_selected--;
        else if (extended_scancode && sc == 0x50 && settings_selected < 3) settings_selected++;
        else if (sc == 0x1C) run_settings_selected();
        else if (sc == 0x01) { settings_open = 0; active_window = terminal_open ? 1 : 0; }
        redraw();
        extended_scancode = 0;
        return;
    }

    if (active_window != 1 || !terminal_open) return;

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

void main_loop() {
    while (1) {
        if (!(inb(0x64) & 1)) continue;

        unsigned char status = inb(0x64);
        unsigned char data = inb(0x60);

        if (status & 0x20) {
            if (mouse_cycle == 0 && !(data & 8)) continue;
            mouse_packet[mouse_cycle++] = data;
            if (mouse_cycle == 3) {
                mouse_cycle = 0;
                process_mouse_packet();
            }
        } else {
            handle_keyboard(data);
        }
    }
}

void kernel_main(u32 magic, struct multiboot_info* mbi) {
    fb = (u32*)(u32)mbi->framebuffer_addr;
    width = mbi->framebuffer_width;
    height = mbi->framebuffer_height;
    pitch = mbi->framebuffer_pitch;

    mouse_init();
    draw_desktop();
    main_loop();
}
