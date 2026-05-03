#



typedef unsigned int u32;

static inline void outl(unsigned short port,u32 val){
    __asm__ volatile("outl %0,%1"::"a"(val),"Nd"(port));
}

static inline u32 inl(unsigned short port){
    u32 ret;
    __asm__ volatile("inl %1,%0":"=a"(ret):"Nd"(port));
    return ret;
}

typedef unsigned long long u64;
typedef unsigned char u8;
typedef unsigned short u16;

static u32 *fb;

static char cpu_brand[64];
static char gpu_info[96];
static u64 W, H, P;

static int cx = 16, cy = 16;
static char input[128];
static int input_len = 0;

static char user_name[32];
static char user_pass[32];
static int setup_done = 0;
static int auth_stage = 0;
static int logged_in = 0;
static int is_root = 0;
static int sudo_wait = 0;

static const char *cwd = "/home";

static void kfield_init(void);
static void kfield_ls(void);
static void kfield_cat(const char* name);
static void ata_read_sector(u32 lba, void* buf);
static void fat_info(void);
static void fat_ls(void);
static char up(char c);
static void fat_cat(const char* name);
static void usb_info_cmd(void);
static void xhci_init_cmd(void);
static void xhci_ports_cmd(void);
static void fat_write_cmd(const char* args);
static void fat_rm_cmd(const char* name);
static void ata_write_sector(u32 lba, void* buf);

static void outb(u16 port, u8 val);

static u32 rgb(int r,int g,int b){return ((r&255)<<16)|((g&255)<<8)|(b&255);}

static void px(int x,int y,u32 c){
    if(x<0||y<0||x>=(int)W||y>=(int)H)return;
    fb[y*(P/4)+x]=c;
}

static void rect(int x,int y,int w,int h,u32 c){
    for(int yy=0;yy<h;yy++)for(int xx=0;xx<w;xx++)px(x+xx,y+yy,c);
}

static void clear_screen(){
    rect(0,0,W,H,rgb(5,8,12));
    cx=16; cy=16;
}

static int streq_ci(const char*a,const char*b);

static int streq(const char*a,const char*b){
    int i=0;
    while(a[i]&&b[i]){ if(a[i]!=b[i])return 0; i++; }
    return a[i]==0&&b[i]==0;
}

static int starts(const char*a,const char*b){
    int i=0;
    while(b[i]){ if(a[i]!=b[i])return 0; i++; }
    return 1;
}

static void copy(char*dst,const char*src,int max){
    int i=0;
    while(src[i]&&i<max-1){dst[i]=src[i];i++;}
    dst[i]=0;
}

static const u8 font[39][7]={
{0,0,0,0,0,0,0},
{14,17,17,31,17,17,17},{30,17,17,30,17,17,30},{14,17,16,16,16,17,14},
{30,17,17,17,17,17,30},{31,16,16,30,16,16,31},{31,16,16,30,16,16,16},
{14,17,16,23,17,17,15},{17,17,17,31,17,17,17},{14,4,4,4,4,4,14},
{1,1,1,1,17,17,14},{17,18,20,24,20,18,17},{16,16,16,16,16,16,31},
{17,27,21,21,17,17,17},{17,25,21,19,17,17,17},{14,17,17,17,17,17,14},
{30,17,17,30,16,16,16},{14,17,17,17,21,18,13},{30,17,17,30,20,18,17},
{15,16,16,14,1,1,30},{31,4,4,4,4,4,4},{17,17,17,17,17,17,14},
{17,17,17,17,17,10,4},{17,17,17,21,21,21,10},{17,17,10,4,10,17,17},
{17,17,10,4,4,4,4},{31,1,2,4,8,16,31},
{14,17,19,21,25,17,14},{4,12,4,4,4,4,14},{14,17,1,2,4,8,31},
{30,1,1,14,1,1,30},{2,6,10,18,31,2,2},{31,16,30,1,1,17,14},
{6,8,16,30,17,17,14},{31,1,2,4,8,8,8},{14,17,17,14,17,17,14},
{14,17,17,15,1,2,12},{0,0,0,31,0,0,0},{0,0,0,0,0,12,12}
};

static int glyph_id(char c){
    if(c>='a'&&c<='z')c-=32;
    if(c>='A'&&c<='Z')return c-'A'+1;
    if(c>='0'&&c<='9')return c-'0'+27;
    if(c=='-')return 37;
    if(c=='.')return 38;
    return 0;
}

static void putc(char c,u32 color){
    if(c=='\n'){cx=16;cy+=18;return;}
    if(cx>(int)W-20){cx=16;cy+=18;}
    if(cy>(int)H-30){clear_screen();}
    int id=glyph_id(c);
    for(int y=0;y<7;y++)for(int x=0;x<5;x++)
        if(font[id][y]&(1<<(4-x)))rect(cx+x*2,cy+y*2,2,2,color);
    cx+=12;
}

static void print(const char*s,u32 color){ while(*s)putc(*s++,color); }

static void prompt(){
    print("\n",rgb(180,180,180));
    if(is_root){
        print("root@kernelx:",rgb(255,80,80));
        print(cwd,rgb(120,190,255));
        print("# ",rgb(255,255,255));
    }else{
        print(user_name,rgb(0,255,100));
        print("@kernelx:",rgb(0,255,100));
        print(cwd,rgb(120,190,255));
        print("$ ",rgb(255,255,255));
    }
}


static void cpuid(u32 leaf,u32 sub,u32*a,u32*b,u32*c,u32*d){
    __asm__ volatile("cpuid":"=a"(*a),"=b"(*b),"=c"(*c),"=d"(*d):"a"(leaf),"c"(sub));
}

static void detect_cpu(){
    u32 a,b,c,d;
    u32 max;
    cpuid(0x80000000,0,&max,&b,&c,&d);

    if(max >= 0x80000004){
        u32* out=(u32*)cpu_brand;
        cpuid(0x80000002,0,&out[0],&out[1],&out[2],&out[3]);
        cpuid(0x80000003,0,&out[4],&out[5],&out[6],&out[7]);
        cpuid(0x80000004,0,&out[8],&out[9],&out[10],&out[11]);
        cpu_brand[48]=0;
    }else{
        copy(cpu_brand,"Unknown x86_64 CPU",64);
    }
}

static u32 pci_read(u8 bus,u8 slot,u8 func,u8 off){
    u32 addr=(1u<<31)|((u32)bus<<16)|((u32)slot<<11)|((u32)func<<8)|(off&0xfc);
    outb(0xCF8,addr&0xff);
    outb(0xCF9,(addr>>8)&0xff);
    outb(0xCFA,(addr>>16)&0xff);
    outb(0xCFB,(addr>>24)&0xff);

    u32 ret;
    __asm__ volatile("inl %1,%0":"=a"(ret):"Nd"((u16)0xCFC));
    return ret;
}

static void hex16(char*buf,u16 v){
    const char*h="0123456789ABCDEF";
    buf[0]='0';buf[1]='x';
    buf[2]=h[(v>>12)&15];buf[3]=h[(v>>8)&15];buf[4]=h[(v>>4)&15];buf[5]=h[v&15];
    buf[6]=0;
}

static void append(char*dst,const char*src){
    int i=0,j=0;
    while(dst[i])i++;
    while(src[j])dst[i++]=src[j++];
    dst[i]=0;
}

static void detect_gpu(){
    copy(gpu_info,"No PCI GPU detected",96);

    for(int bus=0;bus<256;bus++){
        for(int slot=0;slot<32;slot++){
            for(int func=0;func<8;func++){
                u32 id=pci_read(bus,slot,func,0);
                if(id==0xffffffff)continue;

                u16 vendor=id&0xffff;
                u16 device=(id>>16)&0xffff;

                u32 classreg=pci_read(bus,slot,func,8);
                u8 class=(classreg>>24)&0xff;
                u8 subclass=(classreg>>16)&0xff;

                if(class==0x03){
                    char dev[8];
                    hex16(dev,device);

                    if(vendor==0x10DE) copy(gpu_info,"NVIDIA GPU device ",96);
                    else if(vendor==0x1002) copy(gpu_info,"AMD Radeon GPU device ",96);
                    else if(vendor==0x8086) copy(gpu_info,"Intel Graphics device ",96);
                    else copy(gpu_info,"Unknown VGA GPU device ",96);

                    append(gpu_info,dev);

                    if(subclass==0x00) append(gpu_info," VGA");
                    if(subclass==0x02) append(gpu_info," 3D");
                    return;
                }
            }
        }
    }
}

static void run_cmd(char*cmd){
    print("\n",rgb(255,255,255));

    if(sudo_wait){
        sudo_wait=0;
        if(streq(cmd,user_pass)){
            is_root=1;
            print("root access granted\n",rgb(255,80,80));
        }else{
            print("sudo: wrong password\n",rgb(255,80,80));
        }
        prompt();
        return;
    }

    if(streq(cmd,"xhci-ports")){
        xhci_ports_cmd();
    }else if(streq(cmd,"xhci-init")){
        xhci_init_cmd();
    }else if(streq(cmd,"usb-info")){
        usb_info_cmd();
    }else if(starts(cmd,"fat-write ")){
        fat_write_cmd(cmd+10);
    }else if(starts(cmd,"fat-rm ")){
        fat_rm_cmd(cmd+7);
    }else if(streq(cmd,"fat-info")){
        fat_info();
    }else if(streq(cmd,"fat-ls")){
        fat_ls();
    }else if(starts(cmd,"fat-cat ")){
        fat_cat(cmd+8);
    }else if(streq(cmd,"kfield-ls")){
        kfield_ls();
    }else if(starts(cmd,"txt-view ")){
        kfield_cat(cmd+9);
    }else if(starts(cmd,"cat ")){
        kfield_cat(cmd+4);
    }else if(streq(cmd,"help-pls")){
        print("KernelX is a small 64-bit kernel booted by GRUB UEFI.\n",rgb(0,255,120));
        print("It uses framebuffer for text output and KField as virtual FS.\n",rgb(0,255,120));
        print("To build your own OS: edit kernel/main.c, build kernelx.bin, boot with GRUB.\n",rgb(0,255,120));
        print("Main parts: bootloader, kernel, framebuffer, keyboard, filesystem, shell.\n",rgb(0,255,120));
    }else if(streq(cmd,"txt-view README.TXT") || streq(cmd,"txt-view readme.txt") || streq(cmd,"README.TXT") || streq(cmd,"readme.txt")){
        print("README.TXT:\n",rgb(120,190,255));
        print("Welcome to KernelX 1.5.\n",rgb(0,255,120));
        print("This is a kernel-only system without DE or GUI.\n",rgb(0,255,120));
        print("KField is the virtual filesystem layer.\n",rgb(0,255,120));
        print("Use help, help-pls, fastfetch, kfield-ls, txt-view README.TXT.\n",rgb(0,255,120));
    }else if(streq(cmd,"txt-view KFIELD.SYS") || streq(cmd,"txt-view kfield.sys")){
        print("KFIELD.SYS:\n",rgb(120,190,255));
        print("KField status: mounted, memory/disk backed account storage enabled.\n",rgb(0,255,120));
    }else if(starts(cmd,"txt-view ")){
        print("txt-view: file not found or not a txt file\n",rgb(255,80,80));
    }else if(streq(cmd,"hello")){
        print("Hello from KernelX\n",rgb(0,255,120));

    }else if(streq(cmd,"system")){
        print("Use: fastfetch meminfo cpuinfo gpuinfo uname uptime hostname whoami date\n",rgb(0,255,120));

    }else if(streq(cmd,"help")){
        print("AVAILABLE: help clear uname uname -r uptime hostname whoami date\n",rgb(0,255,120));
        print("FILES: kfield-ls txt-view cat fat-info fat-ls fat-cat fat-write fat-rm usb-info xhci-init xhci-ports\n",rgb(0,255,120));
        print("SYSTEM: fastfetch cpuinfo sudo su exit halt\n",rgb(0,255,120));
    }else if(streq(cmd,"sudo su")){
        print("password: ",rgb(255,220,80));
        sudo_wait=1;
        return;
    }else if(streq(cmd,"exit")){
        if(is_root){is_root=0; print("left root shell\n",rgb(255,220,80));}
        else print("already user shell\n",rgb(255,220,80));
    }else if(streq(cmd,"whoami")){
        if(is_root)print("root\n",rgb(255,80,80));
        else {print(user_name,rgb(0,255,120)); print("\n",rgb(0,255,120));}
    }else if(streq(cmd,"clear")){
        clear_screen();
    }else if(streq(cmd,"uname")){
        print("KernelX 1.5 x86_64 GRUB KField\n",rgb(0,255,120));
    }else if(streq(cmd,"uname -r")){
        print("1.5.0-kernelx64\n",rgb(0,255,120));
    }else if(streq(cmd,"uptime")){
        print("uptime: not implemented yet\n",rgb(255,220,80));
    }else if(streq(cmd,"hostname")){
        print("kernelx\n",rgb(0,255,120));
    }else if(streq(cmd,"date")){
        print("date: RTC driver not loaded\n",rgb(255,220,80));
    }else if(streq(cmd,"pwd")){
        print(cwd,rgb(0,255,120)); print("\n",rgb(0,255,120));
    }else if(streq(cmd,"ls")||streq(cmd,"ls -al")){
        kfield_ls();
    }else if(streq(cmd,"cat README.TXT")||streq(cmd,"cat readme.txt")){
        print("Welcome to KernelX shell. KField virtual FS mounted.\n",rgb(0,255,120));
    }else if(starts(cmd,"cd ")){
        cwd=cmd+3; print("changed directory\n",rgb(0,255,120));
    }else if(starts(cmd,"mkdir ")){
        print("mkdir: write support not implemented yet\n",rgb(255,220,80));
    }else if(starts(cmd,"touch ")){
        print("touch: write support not implemented yet\n",rgb(255,220,80));
    }else if(starts(cmd,"rm ")){
        print("rm: delete support not implemented yet\n",rgb(255,220,80));
    }else if(streq(cmd,"drives")){
        print("drive0: ATA/IDE KernelX.img\n",rgb(0,255,120));
        print("usb: not loaded yet, needs USB mass storage driver\n",rgb(255,220,80));
    }else if(streq(cmd,"mount 0")){
        print("mounted drive0 as FAT/KField disk\n",rgb(0,255,120));
    }else if(streq(cmd,"txt-view /README.TXT")){
        print("Reading real FAT32 files is next: need FAT parser\n",rgb(255,220,80));
        print("Current txt-view reads built-in virtual files only.\n",rgb(255,220,80));
    }else if(streq(cmd,"drives")){
        print("drive0: ATA/IDE KernelX.img\n",rgb(0,255,120));
        print("usb: not loaded yet, needs USB mass storage driver\n",rgb(255,220,80));
    }else if(streq(cmd,"mount 0")){
        print("mounted drive0 as FAT/KField disk\n",rgb(0,255,120));
    }else if(streq(cmd,"txt-view /README.TXT")){
        print("Reading real FAT32 files is next: need FAT parser\n",rgb(255,220,80));
        print("Current txt-view reads built-in virtual files only.\n",rgb(255,220,80));
    }else if(streq(cmd,"kfield")){
        print("KField FS: mounted at /, memory-only, status OK\n",rgb(0,255,120));
    }else if(streq(cmd,"fastfetch")){
        print("KX\n",rgb(0,200,255));
        print("OS: KernelX 1.5\n",rgb(0,255,120));
        print("Kernel: 1.5.0-kernelx64\n",rgb(0,255,120));
        print("Bootloader: GRUB UEFI\n",rgb(0,255,120));
        print("CPU: ",rgb(0,255,120)); print(cpu_brand,rgb(0,255,120)); print("\n",rgb(0,255,120));
        print("GPU: ",rgb(0,255,120)); print(gpu_info,rgb(0,255,120)); print("\n",rgb(0,255,120));
        print("Mode: kernel-only\n",rgb(0,255,120));
        print("FS: KField\n",rgb(0,255,120));
        print("User: ",rgb(0,255,120)); print(user_name,rgb(0,255,120)); print("\n",rgb(0,255,120));
    }else if(streq(cmd,"gpuinfo")){
        print("GPU: ",rgb(0,255,120));
        print(gpu_info,rgb(0,255,120));
        print("\n",rgb(0,255,120));
    }else if(streq(cmd,"meminfo")){
        print("meminfo: memory map parser not implemented yet\n",rgb(255,220,80));
    }else if(streq(cmd,"cpuinfo")){
        print("CPU: ",rgb(0,255,120)); print(cpu_brand,rgb(0,255,120)); print("\n",rgb(0,255,120));
    }else if(streq(cmd,"halt")){
        print("system halted\n",rgb(255,80,80));
        for(;;)__asm__ volatile("hlt");
    }else if(cmd[0]==0){
    }else{
        print("kxsh: command not found: ",rgb(255,80,80));
        print(cmd,rgb(255,80,80));
        print("\n",rgb(255,80,80));
    }

    prompt();
}

static u8 inb(u16 port){
    u8 ret;
    __asm__ volatile("inb %1,%0":"=a"(ret):"Nd"(port));
    return ret;
}

static void outb(u16 port, u8 val){
    __asm__ volatile("outb %0,%1"::"a"(val),"Nd"(port));
}

#define ACCOUNT_LBA 120000
#define ACCOUNT_MAGIC 0x4B584143

struct account_sector {
    u32 magic;
    char user[32];
    char pass[32];
    char pad[512 - 4 - 32 - 32];
};

static void ata_wait(){
    while(inb(0x1F7) & 0x80){}
}

static void ata_read_sector(u32 lba, void* buf){
    ata_wait();
    outb(0x1F6, 0xE0 | ((lba >> 24) & 0x0F));
    outb(0x1F2, 1);
    outb(0x1F3, lba & 0xFF);
    outb(0x1F4, (lba >> 8) & 0xFF);
    outb(0x1F5, (lba >> 16) & 0xFF);
    outb(0x1F7, 0x20);
    ata_wait();

    u16* p=(u16*)buf;
    for(int i=0;i<256;i++)
        __asm__ volatile("inw %1,%0":"=a"(p[i]):"Nd"((u16)0x1F0));
}


static u16 rd16(u8* p){
    return (u16)p[0] | ((u16)p[1] << 8);
}

static u32 rd32(u8* p){
    return (u32)p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24);
}

static void print_dec(u32 v){
    char buf[16];
    int i=0;
    if(v==0){ putc('0',rgb(0,255,120)); return; }
    while(v>0 && i<15){ buf[i++]='0'+(v%10); v/=10; }
    while(i>0) putc(buf[--i],rgb(0,255,120));
}

static void fat_name_from_entry(u8* e,char* out){
    int k=0;
    for(int i=0;i<8;i++){
        if(e[i]==' ')break;
        out[k++]=e[i];
    }
    if(e[8]!=' '){
        out[k++]='.';
        for(int i=8;i<11;i++){
            if(e[i]==' ')break;
            out[k++]=e[i];
        }
    }
    out[k]=0;
}

static int fat_match_name(u8* e,const char* name){
    char n[16];
    fat_name_from_entry(e,n);
    return streq_ci(n,name);
}


struct fat_info_data {
    u16 bps;
    u8 spc;
    u16 reserved;
    u8 fats;
    u32 spf;
    u32 root_lba;
    u32 root_sectors;
    u32 data_lba;
    u16 root_entries;
    u16 type;
    u32 root_cluster;
};

static int fat_load_boot_data(struct fat_info_data* f){
    u8 b[512];
    ata_read_sector(0,b);

    f->bps=rd16(b+11);
    f->spc=b[13];
    f->reserved=rd16(b+14);
    f->fats=b[16];
    f->root_entries=rd16(b+17);
    u16 spf16=rd16(b+22);
    u32 spf32=rd32(b+36);
    f->spf=spf16?spf16:spf32;
    f->root_cluster=rd32(b+44);

    if(f->bps!=512)return 0;

    f->root_sectors=((f->root_entries*32)+511)/512;
    f->root_lba=f->reserved + f->fats*f->spf;
    f->data_lba=f->root_lba + f->root_sectors;
    f->type=(f->root_entries==0)?32:16;

    if(f->type==32){
        f->root_lba=0;
        f->root_sectors=0;
    }

    return 1;
}

static int fat_load_boot(u32* root_lba,u32* root_sectors,u32* data_lba,u16* bytes_per_sector,u8* sectors_per_cluster,u16* root_entries,u16* fat_type){
    struct fat_info_data f;
    if(!fat_load_boot_data(&f))return 0;
    *root_lba=f.root_lba;
    *root_sectors=f.root_sectors;
    *data_lba=f.data_lba;
    *bytes_per_sector=f.bps;
    *sectors_per_cluster=f.spc;
    *root_entries=f.root_entries;
    *fat_type=f.type;
    return 1;
}

static u32 fat_cluster_lba(struct fat_info_data* f,u32 cl){
    return f->data_lba + (cl - 2) * f->spc;
}

static u32 fat_next_cluster(struct fat_info_data* f,u32 cl){
    u8 sec[512];
    u32 fat_offset=cl*4;
    u32 lba=f->reserved + fat_offset/512;
    u32 off=fat_offset%512;
    ata_read_sector(lba,sec);
    return rd32(sec+off)&0x0fffffff;
}

static int fat_cluster_end(u32 cl){
    return cl>=0x0ffffff8 || cl==0;
}

static void fat_print_entry(u8* e){
    char name[16];
    fat_name_from_entry(e,name);
    if(name[0]){
        print(name,rgb(0,255,120));
        if(e[11]&0x10)print("  DIR\n",rgb(255,220,80));
        else{
            print("  ",rgb(180,180,180));
            print_dec(rd32(e+28));
            print(" bytes\n",rgb(180,180,180));
        }
    }
}

static void fat_info(){
    struct fat_info_data f;
    if(!fat_load_boot_data(&f)){
        print("FAT: unsupported boot sector\n",rgb(255,80,80));
        return;
    }

    print("FAT volume detected\n",rgb(0,255,120));
    print("Type: FAT",rgb(0,255,120)); print_dec(f.type); print("\n",rgb(0,255,120));
    print("Bytes per sector: ",rgb(0,255,120)); print_dec(f.bps); print("\n",rgb(0,255,120));
    print("Sectors per cluster: ",rgb(0,255,120)); print_dec(f.spc); print("\n",rgb(0,255,120));

    if(f.type==32){
        print("Root cluster: ",rgb(0,255,120)); print_dec(f.root_cluster); print("\n",rgb(0,255,120));
    }else{
        print("Root LBA: ",rgb(0,255,120)); print_dec(f.root_lba); print("\n",rgb(0,255,120));
    }
}

static void fat_ls(){
    struct fat_info_data f;
    u8 sec[512];

    if(!fat_load_boot_data(&f)){
        print("FAT: unsupported boot sector\n",rgb(255,80,80));
        return;
    }

    if(f.type==16){
        for(u32 s=0;s<f.root_sectors;s++){
            ata_read_sector(f.root_lba+s,sec);
            for(int off=0;off<512;off+=32){
                u8* e=sec+off;
                if(e[0]==0x00)return;
                if(e[0]==0xE5)continue;
                if(e[11]==0x0F)continue;
                if(e[11]&0x08)continue;
                fat_print_entry(e);
            }
        }
        return;
    }

    u32 cl=f.root_cluster;

    while(!fat_cluster_end(cl)){
        u32 lba=fat_cluster_lba(&f,cl);

        for(u32 s=0;s<f.spc;s++){
            ata_read_sector(lba+s,sec);

            for(int off=0;off<512;off+=32){
                u8* e=sec+off;
                if(e[0]==0x00)return;
                if(e[0]==0xE5)continue;
                if(e[11]==0x0F)continue;
                if(e[11]&0x08)continue;
                fat_print_entry(e);
            }
        }
    }
}

static int fat_find_file(struct fat_info_data* f,const char* name,u8* out){
    u8 sec[512];

    if(f->type==16){
        for(u32 s=0;s<f->root_sectors;s++){
            ata_read_sector(f->root_lba+s,sec);
            for(int off=0;off<512;off+=32){
                u8* e=sec+off;
                if(e[0]==0x00)return 0;
                if(e[0]==0xE5)continue;
                if(e[11]==0x0F)continue;
                if(e[11]&0x10)continue;
                if(fat_match_name(e,name)){
                    for(int i=0;i<32;i++)out[i]=e[i];
                    return 1;
                }
            }
        }
        return 0;
    }

    u32 cl=f->root_cluster;

    while(!fat_cluster_end(cl)){
        u32 lba=fat_cluster_lba(f,cl);

        for(u32 s=0;s<f->spc;s++){
            ata_read_sector(lba+s,sec);

            for(int off=0;off<512;off+=32){
                u8* e=sec+off;
                if(e[0]==0x00)return 0;
                if(e[0]==0xE5)continue;
                if(e[11]==0x0F)continue;
                if(e[11]&0x10)continue;
                if(fat_match_name(e,name)){
                    for(int i=0;i<32;i++)out[i]=e[i];
                    return 1;
                }
            }
        }

        cl=fat_next_cluster(f,cl);
    }

    return 0;
}

static void fat_cat(const char* name){
    struct fat_info_data f;
    u8 e[32];
    u8 sec[512];
    char fname[16];

    while(*name==' ')name++;

    if(!fat_load_boot_data(&f)){
        print("FAT: unsupported boot sector\n",rgb(255,80,80));
        return;
    }

    if(!fat_find_file(&f,name,e)){
        print("fat-cat: file not found: ",rgb(255,80,80));
        print(name,rgb(255,80,80));
        print("\n",rgb(255,80,80));
        return;
    }

    fat_name_from_entry(e,fname);
    print(fname,rgb(120,190,255));
    print(":\n",rgb(120,190,255));

    u32 size=rd32(e+28);
    u32 cl;

    if(f.type==32)cl=((u32)rd16(e+20)<<16)|rd16(e+26);
    else cl=rd16(e+26);

    u32 left=size;

    while(!fat_cluster_end(cl) && left>0){
        u32 lba=fat_cluster_lba(&f,cl);

        for(u32 s=0;s<f.spc && left>0;s++){
            ata_read_sector(lba+s,sec);

            for(int i=0;i<512 && left>0;i++,left--){
                char ch=sec[i];
                if(ch==13)continue;
                if(ch==10 || (ch>=32 && ch<=126))putc(ch,rgb(0,255,120));
                else putc('.',rgb(255,220,80));
            }
        }

        if(f.type==32)cl=fat_next_cluster(&f,cl);
        else break;
    }

    print("\n",rgb(0,255,120));
}



static void fat_83(const char* in,char* out){
    for(int i=0;i<11;i++)out[i]=' ';
    int i=0,j=0;
    while(in[i]&&in[i]!=' '&&in[i]!='.'&&j<8)out[j++]=up(in[i++]);
    if(in[i]=='.'){
        i++;
        j=8;
        int k=0;
        while(in[i]&&in[i]!=' '&&k<3){out[j++]=up(in[i++]);k++;}
    }
}

static void fat_set32(struct fat_info_data* f,u32 cl,u32 val){
    u8 sec[512];
    u32 off=cl*4;
    for(int fat=0;fat<f->fats;fat++){
        u32 lba=f->reserved+fat*f->spf+off/512;
        u32 o=off%512;
        ata_read_sector(lba,sec);
        sec[o]=val&255;
        sec[o+1]=(val>>8)&255;
        sec[o+2]=(val>>16)&255;
        sec[o+3]=(val>>24)&255;
        ata_write_sector(lba,sec);
    }
}

static u32 fat_find_free_cluster(struct fat_info_data* f){
    for(u32 cl=3;cl<65536;cl++){
        if(fat_next_cluster(f,cl)==0)return cl;
    }
    return 0;
}

static int fat_find_slot(struct fat_info_data* f,const char* name,u32* out_lba,u32* out_off,u8* old,int existing){
    u8 sec[512];
    u32 cl=f->root_cluster;

    while(!fat_cluster_end(cl)){
        u32 lba=fat_cluster_lba(f,cl);
        for(u32 s=0;s<f->spc;s++){
            ata_read_sector(lba+s,sec);
            for(int off=0;off<512;off+=32){
                u8* e=sec+off;
                if(existing){
                    if(e[0]==0x00)return 0;
                    if(e[0]==0xE5)continue;
                    if(e[11]==0x0F)continue;
                    if(fat_match_name(e,name)){
                        for(int i=0;i<32;i++)old[i]=e[i];
                        *out_lba=lba+s;
                        *out_off=off;
                        return 1;
                    }
                }else{
                    if(e[0]==0x00||e[0]==0xE5){
                        *out_lba=lba+s;
                        *out_off=off;
                        return 1;
                    }
                }
            }
        }
        cl=fat_next_cluster(f,cl);
    }
    return 0;
}

static void fat_write_cmd(const char* args){
    struct fat_info_data f;
    u8 sec[512];
    u8 entry[32];
    u8 old[32];
    char name[32];
    char n83[11];

    while(*args==' ')args++;

    int i=0;
    while(args[i]&&args[i]!=' '&&i<31){name[i]=args[i];i++;}
    name[i]=0;
    while(args[i]==' ')i++;
    const char* data=args+i;

    if(name[0]==0||data[0]==0){
        print("fat-write: usage fat-write FILE.TXT TEXT\n",rgb(255,220,80));
        return;
    }

    if(!fat_load_boot_data(&f)||f.type!=32){
        print("fat-write: FAT32 only\n",rgb(255,80,80));
        return;
    }

    u32 cl=fat_find_free_cluster(&f);
    if(cl==0){
        print("fat-write: no free cluster\n",rgb(255,80,80));
        return;
    }

    u32 size=0;
    while(data[size]&&size<f.spc*512)size++;

    u32 lba=fat_cluster_lba(&f,cl);
    u32 written=0;

    for(u32 ss=0;ss<f.spc;ss++){
        for(int k=0;k<512;k++)sec[k]=0;
        for(int k=0;k<512&&written<size;k++)sec[k]=data[written++];
        ata_write_sector(lba+ss,sec);
    }

    fat_set32(&f,cl,0x0fffffff);

    u32 dir_lba=0,dir_off=0;

    if(fat_find_slot(&f,name,&dir_lba,&dir_off,old,1)){
        u32 oldcl=((u32)rd16(old+20)<<16)|rd16(old+26);
        if(oldcl>=2)fat_set32(&f,oldcl,0);
    }else{
        if(!fat_find_slot(&f,name,&dir_lba,&dir_off,old,0)){
            print("fat-write: no dir slot\n",rgb(255,80,80));
            return;
        }
    }

    ata_read_sector(dir_lba,sec);

    for(int k=0;k<32;k++)entry[k]=0;
    fat_83(name,n83);
    for(int k=0;k<11;k++)entry[k]=n83[k];

    entry[11]=0x20;
    entry[20]=(cl>>16)&255;
    entry[21]=(cl>>24)&255;
    entry[26]=cl&255;
    entry[27]=(cl>>8)&255;
    entry[28]=size&255;
    entry[29]=(size>>8)&255;
    entry[30]=(size>>16)&255;
    entry[31]=(size>>24)&255;

    for(int k=0;k<32;k++)sec[dir_off+k]=entry[k];
    ata_write_sector(dir_lba,sec);

    print("fat-write: saved ",rgb(0,255,120));
    print(name,rgb(0,255,120));
    print("\n",rgb(0,255,120));
}

static void fat_rm_cmd(const char* name){
    struct fat_info_data f;
    u8 sec[512];
    u8 old[32];
    u32 dir_lba=0,dir_off=0;

    while(*name==' ')name++;

    if(!fat_load_boot_data(&f)||f.type!=32){
        print("fat-rm: FAT32 only\n",rgb(255,80,80));
        return;
    }

    if(!fat_find_slot(&f,name,&dir_lba,&dir_off,old,1)){
        print("fat-rm: file not found\n",rgb(255,80,80));
        return;
    }

    u32 cl=((u32)rd16(old+20)<<16)|rd16(old+26);
    if(cl>=2)fat_set32(&f,cl,0);

    ata_read_sector(dir_lba,sec);
    sec[dir_off]=0xE5;
    ata_write_sector(dir_lba,sec);

    print("fat-rm: removed ",rgb(255,120,120));
    print(name,rgb(255,120,120));
    print("\n",rgb(255,120,120));
}




static u32 pci_read32(u8 bus,u8 dev,u8 func,u8 off){
    u32 addr = 0x80000000
        | ((u32)bus << 16)
        | ((u32)dev << 11)
        | ((u32)func << 8)
        | (off & 0xFC);

    outl(0xCF8, addr);
    return inl(0xCFC);
}

static u16 pci_read16(u8 bus,u8 dev,u8 func,u8 off){
    u32 v = pci_read32(bus,dev,func,off);
    return (v >> ((off & 2) * 8)) & 0xFFFF;
}

static u8 pci_read8(u8 bus,u8 dev,u8 func,u8 off){
    u32 v = pci_read32(bus,dev,func,off);
    return (v >> ((off & 3) * 8)) & 0xFF;
}

static void print_hex4(u32 v){
    char h[]="0123456789ABCDEF";
    for(int i=28;i>=0;i-=4){
        char c[2];
        c[0]=h[(v>>i)&15];
        c[1]=0;
        print(c,rgb(120,190,255));
    }
}



static const char* usb_type_name(u8 prog){
    if(prog==0x00)return "UHCI";
    if(prog==0x10)return "OHCI";
    if(prog==0x20)return "EHCI";
    if(prog==0x30)return "XHCI";
    if(prog==0x80)return "USB OTHER";
    if(prog==0xFE)return "USB DEVICE";
    return "UNKNOWN USB";
}

static void usb_info_cmd(void){
    int found=0;

    print("USB PCI scan:\n",rgb(255,255,120));

    for(u32 bus=0;bus<256;bus++){
        for(u32 dev=0;dev<32;dev++){
            for(u32 func=0;func<8;func++){
                u16 vendor=pci_read16(bus,dev,func,0x00);
                if(vendor==0xFFFF)continue;

                u8 class_code=pci_read8(bus,dev,func,0x0B);
                u8 subclass=pci_read8(bus,dev,func,0x0A);
                u8 prog_if=pci_read8(bus,dev,func,0x09);

                if(class_code==0x0C && subclass==0x03){
                    found=1;

                    print("USB controller: ",rgb(0,255,120));
                    print(usb_type_name(prog_if),rgb(0,255,120));

                    print("  bus ",rgb(220,220,220));
                    print_dec(bus);
                    print(" dev ",rgb(220,220,220));
                    print_dec(dev);
                    print(" func ",rgb(220,220,220));
                    print_dec(func);

                    print("  vendor/device 0x",rgb(220,220,220));
                    u32 id=pci_read32(bus,dev,func,0x00);
                    print_hex4(id);

                    print("  BAR0 0x",rgb(220,220,220));
                    u32 bar0=pci_read32(bus,dev,func,0x10);
                    print_hex4(bar0);

                    print("\n",rgb(220,220,220));
                }
            }
        }
    }

    if(!found){
        print("No USB controller found.\n",rgb(255,80,80));
    }
}



static void pci_write32(u8 bus,u8 dev,u8 func,u8 off,u32 val){
    u32 addr = 0x80000000
        | ((u32)bus << 16)
        | ((u32)dev << 11)
        | ((u32)func << 8)
        | (off & 0xFC);

    outl(0xCF8, addr);
    outl(0xCFC, val);
}

static u32 mmio_read32(u64 addr){
    volatile u32* p=(volatile u32*)addr;
    return *p;
}

static u8 mmio_read8(u64 addr){
    volatile u8* p=(volatile u8*)addr;
    return *p;
}

static int xhci_find(u8* obus,u8* odev,u8* ofunc,u64* obase){
    for(u32 bus=0;bus<256;bus++){
        for(u32 dev=0;dev<32;dev++){
            for(u32 func=0;func<8;func++){
                u16 vendor=pci_read16(bus,dev,func,0x00);
                if(vendor==0xFFFF)continue;

                u8 class_code=pci_read8(bus,dev,func,0x0B);
                u8 subclass=pci_read8(bus,dev,func,0x0A);
                u8 prog_if=pci_read8(bus,dev,func,0x09);

                if(class_code==0x0C && subclass==0x03 && prog_if==0x30){
                    u32 bar0=pci_read32(bus,dev,func,0x10);
                    u32 bar1=pci_read32(bus,dev,func,0x14);

                    u64 base=0;

                    if((bar0 & 1)==0){
                        if((bar0 & 0x6)==0x4){
                            base=((u64)bar1<<32)|(bar0 & 0xFFFFFFF0);
                        }else{
                            base=bar0 & 0xFFFFFFF0;
                        }
                    }

                    *obus=bus;
                    *odev=dev;
                    *ofunc=func;
                    *obase=base;
                    return 1;
                }
            }
        }
    }

    return 0;
}

static void xhci_init_cmd(void){
    u8 bus=0,dev=0,func=0;
    u64 base=0;

    print("XHCI init safe check:\n",rgb(255,255,120));

    if(!xhci_find(&bus,&dev,&func,&base)){
        print("No XHCI controller found.\n",rgb(255,80,80));
        return;
    }

    u32 bar0=pci_read32(bus,dev,func,0x10);
    u32 bar1=pci_read32(bus,dev,func,0x14);

    print("XHCI found. BAR0 0x",rgb(0,255,120));
    print_hex4(bar0);
    print(" BAR1 0x",rgb(0,255,120));
    print_hex4(bar1);
    print("\n",rgb(220,220,220));

    if(bar0 & 1){
        print("BAR0 is IO space, not MMIO. Do not read it.\n",rgb(255,80,80));
        return;
    }

    if(base==0 || base<0x100000){
        print("Bad/low MMIO base. Skipping MMIO read to avoid reboot.\n",rgb(255,80,80));
        return;
    }

    print("MMIO base seems valid, but safe mode skipped register read.\n",rgb(255,255,120));
}



static void xhci_ports_cmd(void){
    u8 bus=0,dev=0,func=0;
    u64 base=0;

    print("XHCI ports:\n",rgb(255,255,120));

    if(!xhci_find(&bus,&dev,&func,&base)){
        print("No XHCI controller found.\n",rgb(255,80,80));
        return;
    }

    u32 bar0=pci_read32(bus,dev,func,0x10);
    u32 bar1=pci_read32(bus,dev,func,0x14);

    print("BAR0 0x",rgb(120,190,255));
    print_hex4(bar0);
    print(" BAR1 0x",rgb(120,190,255));
    print_hex4(bar1);
    print("\n",rgb(220,220,220));

    if(bar0 & 1){
        print("BAR0 is IO space. XHCI needs MMIO.\n",rgb(255,80,80));
        return;
    }

    base = bar0 & 0xFFFFFFF0;

    if(base==0 || base<0x100000){
        print("Bad/unassigned XHCI MMIO BAR. Ports skipped.\n",rgb(255,80,80));
        print("Try QEMU q35/ahci launch later.\n",rgb(255,220,80));
        return;
    }

    u8 caplen=mmio_read8(base+0x00);
    u32 hcs1=mmio_read32(base+0x04);

    u32 max_ports=(hcs1>>24)&0xFF;
    u64 opbase=base+caplen;

    print("CAPLEN ",rgb(0,255,120));
    print_dec(caplen);
    print(" PORTS ",rgb(0,255,120));
    print_dec(max_ports);
    print("\n",rgb(220,220,220));

    for(u32 i=0;i<max_ports;i++){
        u64 portsc=opbase+0x400+(i*0x10);
        u32 v=mmio_read32(portsc);

        print("Port ",rgb(120,190,255));
        print_dec(i+1);
        print(": PORTSC 0x",rgb(120,190,255));
        print_hex4(v);

        if(v&1) print(" connected",rgb(0,255,120));
        else print(" empty",rgb(180,180,180));

        print("\n",rgb(220,220,220));
    }
}


static void ata_write_sector(u32 lba, void* buf){
    ata_wait();
    outb(0x1F6, 0xE0 | ((lba >> 24) & 0x0F));
    outb(0x1F2, 1);
    outb(0x1F3, lba & 0xFF);
    outb(0x1F4, (lba >> 8) & 0xFF);
    outb(0x1F5, (lba >> 16) & 0xFF);
    outb(0x1F7, 0x30);
    ata_wait();

    u16* p=(u16*)buf;
    for(int i=0;i<256;i++)
        __asm__ volatile("outw %0,%1"::"a"(p[i]),"Nd"((u16)0x1F0));

    outb(0x1F7, 0xE7);
    ata_wait();
}

static void save_account(){
    struct account_sector acc;
    acc.magic=ACCOUNT_MAGIC;
    copy(acc.user,user_name,32);
    copy(acc.pass,user_pass,32);
    ata_write_sector(ACCOUNT_LBA,&acc);
}

static void load_account(){
    struct account_sector acc;
    ata_read_sector(ACCOUNT_LBA,&acc);

    if(acc.magic==ACCOUNT_MAGIC){
        copy(user_name,acc.user,32);
        copy(user_pass,acc.pass,32);
        setup_done=1;
        auth_stage=2;
    }
}

static char scancode_to_char(u8 s){
    static char map[128]={
        0,27,'1','2','3','4','5','6','7','8','9','0','-','=',8,9,
        'q','w','e','r','t','y','u','i','o','p','[',']','\n',0,
        'a','s','d','f','g','h','j','k','l',';',39,'`',0,'\\',
        'z','x','c','v','b','n','m',',','.','/',0,'*',0,' '
    };
    if(s<128)return map[s];
    return 0;
}

#define KFIELD_MAGIC 0x4B465331
#define KFIELD_TABLE_LBA 120100
#define KFIELD_DATA_LBA 120120
#define KFIELD_MAX_FILES 7
#define KFIELD_FILE_SECTORS 4

struct kfield_entry {
    char name[32];
    u32 start_lba;
    u32 sectors;
    u32 size;
    u32 used;
    char pad[16];
};

struct kfield_table {
    u32 magic;
    struct kfield_entry files[KFIELD_MAX_FILES];
    char pad[512 - 4 - (64 * KFIELD_MAX_FILES)];
};

static void memzero(void* ptr, int n){
    u8* p=(u8*)ptr;
    for(int i=0;i<n;i++)p[i]=0;
}

static int strlen2(const char* s){
    int i=0;
    while(s[i])i++;
    return i;
}

static char up(char c){
    if(c>='a'&&c<='z')return c-32;
    return c;
}

static int streq_ci(const char*a,const char*b){
    int i=0;
    while(a[i]&&b[i]){
        if(up(a[i])!=up(b[i]))return 0;
        i++;
    }
    return a[i]==0&&b[i]==0;
}

static void kfield_write_table(struct kfield_table* t){
    ata_write_sector(KFIELD_TABLE_LBA,t);
}

static void kfield_read_table(struct kfield_table* t){
    ata_read_sector(KFIELD_TABLE_LBA,t);
}

static void kfield_write_file(u32 lba,const char* data,u32 size){
    char sector[512];
    u32 written=0;

    for(u32 s=0;s<KFIELD_FILE_SECTORS;s++){
        memzero(sector,512);
        for(int i=0;i<512 && written<size;i++){
            sector[i]=data[written++];
        }
        ata_write_sector(lba+s,sector);
    }
}

static void kfield_init(){
    struct kfield_table t;
    kfield_read_table(&t);

    if(t.magic==KFIELD_MAGIC)return;

    memzero(&t,512);
    t.magic=KFIELD_MAGIC;

    const char* readme =
        "KernelX 1.5 README.TXT\n"
        "This file is stored on the real KernelX.img disk using KField FS.\n"
        "KernelX kernel-only mode has no GUI or desktop environment.\n"
        "Commands: help, help-pls, fastfetch, ls, kfield-ls, txt-view README.TXT.\n"
        "KField is a tiny experimental filesystem made for KernelX.\n";

    copy(t.files[0].name,"README.TXT",32);
    t.files[0].start_lba=KFIELD_DATA_LBA;
    t.files[0].sectors=KFIELD_FILE_SECTORS;
    t.files[0].size=strlen2(readme);
    t.files[0].used=1;

    kfield_write_table(&t);
    kfield_write_file(KFIELD_DATA_LBA,readme,t.files[0].size);
}

static void kfield_ls(){
    struct kfield_table t;
    kfield_read_table(&t);

    if(t.magic!=KFIELD_MAGIC){
        print("KField: not initialized\n",rgb(255,80,80));
        return;
    }

    for(int i=0;i<KFIELD_MAX_FILES;i++){
        if(t.files[i].used){
            print(t.files[i].name,rgb(0,255,120));
            print("  TXT  on-disk\n",rgb(180,180,180));
        }
    }
}

static void kfield_cat(const char* name){
    struct kfield_table t;
    kfield_read_table(&t);

    if(t.magic!=KFIELD_MAGIC){
        print("KField: not initialized\n",rgb(255,80,80));
        return;
    }

    while(*name==' ')name++;

    for(int i=0;i<KFIELD_MAX_FILES;i++){
        if(t.files[i].used && streq_ci(name,t.files[i].name)){
            char sector[512];

            print(t.files[i].name,rgb(120,190,255));
            print(":\n",rgb(120,190,255));

            u32 left=t.files[i].size;
            for(u32 s=0;s<t.files[i].sectors && left>0;s++){
                ata_read_sector(t.files[i].start_lba+s,sector);
                for(int j=0;j<512 && left>0;j++,left--){
                    putc(sector[j],rgb(0,255,120));
                }
            }
            print("\n",rgb(0,255,120));
            return;
        }
    }

    print("txt-view: file not found: ",rgb(255,80,80));
    print(name,rgb(255,80,80));
    print("\n",rgb(255,80,80));
}



static void boot_message(){
    clear_screen();
    print("KernelX 1.5 64-bit Kernel\n",rgb(0,255,120));
    print("KField virtual filesystem loaded\n",rgb(0,255,120));

    if(setup_done){
        print("Local account loaded from disk\n\n",rgb(0,255,120));
        print("login: ",rgb(0,255,120));
    }else{
        print("First boot setup\n\n",rgb(180,180,180));
        print("create user: ",rgb(0,255,120));
    }
}


#define MULTIBOOT_BOOTLOADER_MAGIC 0x2BADB002

struct multiboot_info {
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
    u64 framebuffer_addr;
    u32 framebuffer_pitch;
    u32 framebuffer_width;
    u32 framebuffer_height;
    u8 framebuffer_bpp;
    u8 framebuffer_type;
    u16 reserved;
} __attribute__((packed));


void kernel_main(u32 magic, u32 mbi_addr){
    if(magic != MULTIBOOT_BOOTLOADER_MAGIC){        for(;;)__asm__ volatile("hlt");
    }

    struct multiboot_info* mbi = (struct multiboot_info*)(u64)mbi_addr;
    if(!(mbi->flags & (1 << 12))){
        for(;;)__asm__ volatile("hlt");
    }

    fb=(u32*)(u64)mbi->framebuffer_addr;
    W=mbi->framebuffer_width;
    H=mbi->framebuffer_height;
    P=mbi->framebuffer_pitch;

    for(u32 y=0;y<H;y++){
        for(u32 x=0;x<W;x++){
            fb[y*(P/4)+x]=0x000000; // чёрный экран
        }
    }

    fb[10*(P/4)+10]=0x00FF0000; // красная точка

    detect_cpu();
    copy(gpu_info,"GPU scan disabled",96);

    load_account();
    kfield_init();
    boot_message();

    while(1){
        if(inb(0x64)&1){
            u8 sc=inb(0x60);
            if(sc&0x80)continue;
            char c=scancode_to_char(sc);

            if(c=='\n'){
                input[input_len]=0;

                if(!setup_done){
                    if(auth_stage==0){
                        copy(user_name,input,32);
                        input_len=0;
                        auth_stage=1;
                        print("\ncreate password: ",rgb(0,255,120));
                        continue;
                    }else{
                        copy(user_pass,input,32);
                        input_len=0;
                        setup_done=1;
                        auth_stage=2;
                        save_account();
                        print("\nAccount created. Login.\nlogin: ",rgb(0,255,120));
                        continue;
                    }
                }

                if(!logged_in){
                    if(auth_stage==2){
                        if(streq(input,user_name)){
                            input_len=0;
                            auth_stage=3;
                            print("\npassword: ",rgb(0,255,120));
                        }else{
                            input_len=0;
                            print("\nLogin incorrect\nlogin: ",rgb(255,80,80));
                        }
                        continue;
                    }else if(auth_stage==3){
                        if(streq(input,user_pass)){
                            logged_in=1;
                            auth_stage=4;
                            input_len=0;
                            print("\nWelcome to KernelX kernel mode. Type help.\n",rgb(0,255,120));
                            cwd="/home";
                            prompt();
                        }else{
                            input_len=0;
                            auth_stage=2;
                            print("\nWrong password\nlogin: ",rgb(255,80,80));
                        }
                        continue;
                    }
                }

                run_cmd(input);
                input_len=0;
            }else if(c==8){
                if(input_len>0){
                    input_len--;
                    cx-=12;
                    rect(cx,cy,12,16,rgb(5,8,12));
                }
            }else if(c&&input_len<120){
                input[input_len++]=c;
                if((!setup_done && auth_stage==1) || (!logged_in && auth_stage==3) || sudo_wait)
                    putc('*',rgb(255,255,255));
                else
                    putc(c,rgb(255,255,255));
            }
        }
    }
}
