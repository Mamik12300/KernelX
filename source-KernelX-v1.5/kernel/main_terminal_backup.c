#define VGA_WIDTH 80
#define VGA_HEIGHT 25

volatile char* video = (volatile char*)0xB8000;

int row = 0;
int col = 0;

char input[128];
int input_len = 0;

const char* current_path = "@user/mamikk";
void clear_screen();

unsigned char inb(unsigned short port) {
    unsigned char ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

void putchar(char c, char color) {
    if (c == '\n') {
        row++;
        col = 0;
        return;
    }

    if (row >= VGA_HEIGHT) {
        clear_screen();
    }

    int index = (row * VGA_WIDTH + col) * 2;
    video[index] = c;
    video[index + 1] = color;

    col++;
    if (col >= VGA_WIDTH) {
        col = 0;
        row++;
    }
}

void print(const char* text, char color) {
    for (int i = 0; text[i] != '\0'; i++) {
        putchar(text[i], color);
    }
}

void clear_screen() {
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        video[i * 2] = ' ';
        video[i * 2 + 1] = 0x0F;
    }
    row = 0;
    col = 0;
}

int strcmp(const char* a, const char* b) {
    int i = 0;
    while (a[i] && b[i]) {
        if (a[i] != b[i]) return 1;
        i++;
    }
    return a[i] != b[i];
}

void prompt() {
    print("\nmamikk", 0x0A);
    print(" ∈ ", 0x08);
    print(current_path, 0x0B);
    print(" » ", 0x0F);
}

void fastfetch() {
    print("\n", 0x0F);

    print("   K   K   X   X\n", 0x0B);
    print("   K  K     X X \n", 0x0B);
    print("   KKK       X  \n", 0x0B);
    print("   K  K     X X \n", 0x0B);
    print("   K   K   X   X\n\n", 0x0B);

    print("System:     KernelX OS v0.4\n", 0x0F);
    print("Kernel:     KernelX Core\n", 0x0F);
    print("Arch:       x86 32-bit\n", 0x0F);
    print("CPU:        Generic x86 CPU\n", 0x0F);
    print("GPU:        VGA Text Mode\n", 0x0F);
    print("Display:    80x25 VGA\n", 0x0F);
    print("DE/WM:      None / Bare Metal\n", 0x0F);
    print("Shell:      kxsh\n", 0x0F);
    print("Terminal:   KernelX TTY\n", 0x0F);
    print("FS:         KField\n", 0x0F);
    print("Path:       ", 0x0F);
    print(current_path, 0x0B);
    print("\n", 0x0F);
}

void help() {
    print("\nCommands:\n", 0x0B);
    print("  help       show commands\n", 0x0F);
    print("  clear      clear screen\n", 0x0F);
    print("  fastfetch  show system info\n", 0x0F);
    print("  where      show current KField sector\n", 0x0F);
    print("  list       list virtual files\n", 0x0F);
    print("  goto core  go to @core\n", 0x0F);
    print("  goto apps  go to @apps\n", 0x0F);
    print("  goto user  go to @user/mamikk\n", 0x0F);
}

void list() {
    print("\n", 0x0F);

    if (strcmp(current_path, "@user/mamikk") == 0) {
        print("notes.kx\nprojects/\nprofile.kx\n", 0x0F);
    } else if (strcmp(current_path, "@core") == 0) {
        print("kernel.core\nvga.driver\nkeyboard.driver\n", 0x0F);
    } else if (strcmp(current_path, "@apps") == 0) {
        print("fastfetch.app\nshell.app\neditor.app\n", 0x0F);
    } else {
        print("empty sector\n", 0x08);
    }
}

void run_command() {
    input[input_len] = '\0';

    if (strcmp(input, "clear") == 0) {
        clear_screen();
    } else if (strcmp(input, "help") == 0) {
        help();
    } else if (strcmp(input, "fastfetch") == 0) {
        fastfetch();
    } else if (strcmp(input, "where") == 0) {
        print("\nCurrent sector: ", 0x0F);
        print(current_path, 0x0B);
        print("\n", 0x0F);
    } else if (strcmp(input, "list") == 0) {
        list();
    } else if (strcmp(input, "goto core") == 0) {
        current_path = "@core";
        print("\nMoved to @core\n", 0x0A);
    } else if (strcmp(input, "goto apps") == 0) {
        current_path = "@apps";
        print("\nMoved to @apps\n", 0x0A);
    } else if (strcmp(input, "goto user") == 0) {
        current_path = "@user/mamikk";
        print("\nMoved to @user/mamikk\n", 0x0A);
    } else if (input_len > 0) {
        print("\nUnknown command: ", 0x0C);
        print(input, 0x0C);
        print("\nType 'help'\n", 0x08);
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
                    if (col > 0) col--;
                    int index = (row * VGA_WIDTH + col) * 2;
                    video[index] = ' ';
                    video[index + 1] = 0x0F;
                }
            } else if (c && input_len < 127) {
                input[input_len++] = c;
                putchar(c, 0x0F);
            }
        }
    }
}

void kernel_main(void) {
    clear_screen();

    print("KernelX Terminal v0.4\n", 0x0B);
    print("KField filesystem online\n", 0x0A);
    print("Type 'help' or 'fastfetch'\n", 0x08);

    prompt();
    keyboard_loop();
}
