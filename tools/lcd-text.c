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

int main(int argc, char **argv)
{
    struct lcd_colors c = {0xffff,0};
    int clear = 0, colors_set = 0;
    if (argc == 2 && !strcmp(argv[1],"--init")) {
        if (initialize() == 0) return 0;
        perror("initialize /dev/fb0"); return 1;
    }
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i],"--clear")) clear = 1;
        else if (!strcmp(argv[i],"--bg") && i+1 < argc) { c.bg=color(argv[++i]); colors_set=1; }
        else if (!strcmp(argv[i],"--fg") && i+1 < argc) { c.fg=color(argv[++i]); colors_set=1; }
        else { fprintf(stderr,"usage: lcd-text [--clear [--bg color] [--fg color]]\n"); return 2; }
    }
    if (colors_set && !clear) { fprintf(stderr,"colors require --clear\n"); return 2; }
    int fd = open("/dev/fb0", O_WRONLY);
    if (fd < 0) { perror("/dev/fb0"); return 1; }
    if (clear) {
        int64_t start=micros();
        if (ioctl(fd,LCD_CLEAR,&c)) { perror("LCD_CLEAR"); return 1; }
        fprintf(stderr,"clear: %ld us\n",(long)(micros()-start));
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
