#include "lcd-text.h"
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
static int64_t us(void) {
    struct {
        int64_t sec, nsec;
    } t;
    if (syscall(403, 1, &t))
        exit(2);
    return t.sec * 1000000 + t.nsec / 1000;
}
static unsigned irq(void) {
    char line[160];
    unsigned n;
    FILE *f = fopen("/proc/interrupts", "r");
    if (!f)
        exit(3);
    while (fgets(line, sizeof line, f))
        if (strstr(line, "dma1chan4") && sscanf(line, "%*d: %u", &n) == 1) {
            fclose(f);
            return n;
        }
    exit(4);
}
int main(void) {
    char buf[1600], readback[1600];
    struct lcd_colors c = {0xffff, 0};
    int fd = open("/dev/fb0", O_RDWR);
    if (fd < 0)
        return 5;
    for (int trial = 0; trial < 5; trial++) {
        for (int mode = 0; mode < 4; mode++) {
            for (int i = 0; i < 1600; i++)
                buf[i] = 33 + (i * 7 + i / 40 + trial + mode) % 92;
            if (lseek(fd, 0, SEEK_SET) < 0)
                return 6;
            unsigned begin = irq();
            int64_t start = us();
            int len = mode == 1 ? 40 : 1600;
            if (mode == 0) {
                c.bg = trial % 2 ? 0x001f : 0;
                if (ioctl(fd, LCD_CLEAR, &c))
                    return 7;
            } else {
                int step = mode == 2 ? 40 : len;
                for (int off = 0; off < len; off += step)
                    if (write(fd, buf + off, step) != step)
                        return 8;
            }
            long elapsed = us() - start;
            unsigned count = irq() - begin;
            if (mode == 0)
                memset(buf, ' ', len);
            if (lseek(fd, 0, SEEK_SET) < 0 || read(fd, readback, len) != len ||
                memcmp(buf, readback, len))
                return 9;
            printf("%d %d %ld %u\n", trial, mode, elapsed, count);
            fflush(stdout);
        }
    }
    return 0;
}
