/* Render standard input in the STM32 text fbdev, TC-15. */
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>
#include "lcd-text.h"

static int initialize(void)
{
    for (int attempt = 0; attempt < 100; attempt++) {
        if (chown("/dev/fb0", 0, 1000) == 0)
            return chmod("/dev/fb0", 0660);
        if (errno != ENOENT) break;
        usleep(50000);
    }
    return -1;
}

static uint16_t color(const char *name)
{
    static const struct { const char *name; uint16_t rgb; } colors[] = {
        {"black",0},{"white",0xffff},{"red",0xf800},{"green",0x07e0},{"blue",0x001f},
    };
    for (unsigned i = 0; i < sizeof(colors)/sizeof(colors[0]); i++)
        if (!strcmp(name,colors[i].name)) return colors[i].rgb;
    fprintf(stderr,"unknown color: %s\n",name);
    exit(2);
}

static int64_t micros(void)
{
    struct { int64_t sec, nsec; } t;
    if (syscall(403 /* ARM EABI clock_gettime64 */, 1, &t)) { perror("clock_gettime64"); exit(1); }
    return t.sec*1000000 + t.nsec/1000;
}

static int put_cells(int fd, int pos, const char *cells, int len)
{
    return lseek(fd,pos,SEEK_SET) >= 0 && write(fd,cells,len) == len ? 0 : -1;
}

/* One outer column of margin, two additional cells of padding inside the frame. */
static int uptime_panel(int fd)
{
    enum { left=1, top=(LCD_ROWS-5)/2, width=LCD_COLS-2, field=20 };
    char row[width], prev[field+1] = "";
    for (int y=0; y<5; y++) {
        memset(row, y==0 || y==4 ? LCD_HORIZ : ' ', width);
        row[0] = y==0 ? LCD_TL : y==4 ? LCD_BL : LCD_VERT;
        row[width-1] = y==0 ? LCD_TR : y==4 ? LCD_BR : LCD_VERT;
        if (y==2) memcpy(row+3,"UPTIME",6);
        if (put_cells(fd,(top+y)*LCD_COLS+left,row,width)) return -1;
    }
    for (;;) {
        struct { int64_t sec,nsec; } t;
        if (syscall(403,7 /* CLOCK_BOOTTIME */, &t)) return -1;
        char value[field+1], stamp[field+1];
        unsigned long days = t.sec/86400;
        unsigned h=t.sec/3600%24, m=t.sec/60%60, sec=t.sec%60;
        int n=snprintf(stamp,sizeof(stamp),"%lud %02u:%02u:%02u",days,h,m,sec);
        if (n < 0 || n > field) return -1;
        memset(value,' ',field-n);
        memcpy(value+field-n,stamp,n+1);
        int first=0,last=field;
        while (first<last && value[first]==prev[first]) first++;
        while (last>first && value[last-1]==prev[last-1]) last--;
        if (last>first && put_cells(fd,(top+2)*LCD_COLS+left+width-3-field+first,
                                   value+first,last-first)) return -1;
        memcpy(prev,value,sizeof(prev));
        if (usleep(1000000-t.nsec/1000) && errno!=EINTR) return -1;
    }
}

int main(int argc, char **argv)
{
    struct lcd_colors c = {0xffff,0};
    int clear = 0, colors_set = 0, uptime = 0;
    if (argc == 2 && !strcmp(argv[1],"--init")) {
        if (initialize() == 0) return 0;
        perror("initialize /dev/fb0"); return 1;
    }
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i],"--clear")) clear = 1;
        else if (!strcmp(argv[i],"--uptime")) { uptime = 1; clear = 1; }
        else if (!strcmp(argv[i],"--bg") && i+1 < argc) { c.bg=color(argv[++i]); colors_set=1; }
        else if (!strcmp(argv[i],"--fg") && i+1 < argc) { c.fg=color(argv[++i]); colors_set=1; }
        else { fprintf(stderr,"usage: lcd-text [--clear | --uptime] [--bg color] [--fg color]\n"); return 2; }
    }
    if (colors_set && !clear) { fprintf(stderr,"colors require --clear\n"); return 2; }
    int fd = open("/dev/fb0", O_WRONLY);
    if (fd < 0) { perror("/dev/fb0"); return 1; }
    if (clear) {
        int64_t start=micros();
        if (ioctl(fd,LCD_CLEAR,&c)) { perror("LCD_CLEAR"); return 1; }
        fprintf(stderr,"clear: %ld us\n",(long)(micros()-start));
    }
    if (uptime) {
        if (uptime_panel(fd)) { perror("uptime panel"); return 1; }
        return 0;
    }
    char line[LCD_COLS];
    int ch, n=0, row=0, wrapped=0;
    while ((ch=getchar()) != EOF) {
        if (ch=='\r') continue;
        if (ch=='\n' && wrapped) { wrapped=0; continue; }
        wrapped=0;
        if (ch!='\n') line[n++]=ch;
        if (ch!='\n' && n<LCD_COLS) continue;
        if (row>=LCD_ROWS || lseek(fd,row*LCD_COLS,SEEK_SET)<0 || write(fd,line,n)!=n) goto error;
        wrapped=n==LCD_COLS; n=0; row++;
    }
    if (ferror(stdin) || (n && (row>=LCD_ROWS || lseek(fd,row*LCD_COLS,SEEK_SET)<0 || write(fd,line,n)!=n))) goto error;
    return close(fd)!=0;
error:
    fprintf(stderr,"text exceeds screen or framebuffer I/O failed\n"); return 1;
}
