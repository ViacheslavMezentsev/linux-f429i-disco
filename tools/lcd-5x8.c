/* TC-14: one line in the ILI9341's GRAM, MB1075 B-01. */
#include <fcntl.h>
#include <linux/gpio.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

static int serial, select_fd, dc_fd;

static void fail(const char *what)
{
    perror(what);
    exit(1);
}

static int lines(const char *bank, unsigned first, int second, int initial)
{
    for (unsigned index = 0; index < 16; index++) {
        char path[32];
        snprintf(path, sizeof(path), "/dev/gpiochip%u", index);
        int fd = open(path, O_RDONLY);
        if (fd < 0) continue;
        struct gpiochip_info info = {0};
        if (ioctl(fd, GPIO_GET_CHIPINFO_IOCTL, &info) == 0 && !strcmp(info.label, bank)) {
            struct gpiohandle_request req = {0};
            strcpy(req.consumer_label, "lcd-5x8");
            req.flags = GPIOHANDLE_REQUEST_OUTPUT;
            req.lines = second < 0 ? 1 : 2;
            req.lineoffsets[0] = first;
            req.default_values[0] = initial;
            if (second >= 0) req.lineoffsets[1] = second;
            if (ioctl(fd, GPIO_GET_LINEHANDLE_IOCTL, &req) < 0) fail(bank);
            close(fd);
            return req.fd;
        }
        close(fd);
    }
    fprintf(stderr, "GPIO bank %s unavailable\n", bank);
    exit(1);
}

static void set(int fd, int first, int second)
{
    struct gpiohandle_data v = {0};
    v.values[0] = first;
    v.values[1] = second;
    if (ioctl(fd, GPIOHANDLE_SET_LINE_VALUES_IOCTL, &v) < 0) fail("set GPIO");
}

static void byte_out(unsigned byte)
{
    for (int bit = 7; bit >= 0; bit--) {
        unsigned level = (byte >> bit) & 1;
        set(serial, 0, level);
        set(serial, 1, level);
    }
    set(serial, 0, 0);
}

static void command(unsigned cmd, const uint8_t *args, unsigned count)
{
    set(select_fd, 0, 0);
    set(dc_fd, 0, 0);
    byte_out(cmd);
    set(dc_fd, 1, 0);
    for (unsigned i = 0; i < count; i++) byte_out(args[i]);
    set(select_fd, 1, 0);
}

/* Five columns per glyph, bit 0 at the top. Row 8 and the sixth column are spacing. */
static const struct { char c; uint8_t column[5]; } glyphs[] = {
    {'L',{0x7f,0x40,0x40,0x40,0x40}}, {'I',{0,0x41,0x7f,0x41,0}},
    {'N',{0x7f,0x02,0x0c,0x10,0x7f}}, {'U',{0x3f,0x40,0x40,0x40,0x3f}},
    {'X',{0x63,0x14,0x08,0x14,0x63}}, {' ',{0,0,0,0,0}},
    {'5',{0x27,0x45,0x45,0x45,0x39}}, {'8',{0x36,0x49,0x49,0x49,0x36}}
};

static int pixel(char ch, unsigned x, unsigned y)
{
    if (x == 5) return 0;
    for (unsigned i = 0; i < sizeof(glyphs)/sizeof(glyphs[0]); i++)
        if (glyphs[i].c == ch) return (glyphs[i].column[x] >> y) & 1;
    return 0;
}

int main(void)
{
    /* Bootloader has released gyro INT1. Keep its CS high on the shared SPI5 bus. */
    int gyro_cs = lines("GPIOC", 1, -1, 1);
    select_fd = lines("GPIOC", 2, -1, 1);
    dc_fd = lines("GPIOD", 13, -1, 1);
    serial = lines("GPIOF", 7, 9, 0);
    command(0x01, NULL, 0);
    usleep(150000);
    command(0x11, NULL, 0);
    usleep(150000);
    command(0xf6, (uint8_t[]){1,0,0}, 3); /* Internal display clock, MCU memory writes */
    command(0x36, (uint8_t[]){0x48}, 1);
    command(0x3a, (uint8_t[]){0x55}, 1); /* RGB565 */
    command(0x29, NULL, 0);
    static const char message[] = "LINUX 5X8";
    enum { width = 6*(sizeof(message)-1)+4, height = 12,
           left = (240-width)/2, top = (320-height)/2 };
    command(0x2a, (uint8_t[]){0,left,0,left+width-1}, 4);
    command(0x2b, (uint8_t[]){0,top,0,top+height-1}, 4);
    command(0x2c, NULL, 0);
    set(select_fd, 0, 0);
    for (unsigned y = 0; y < height; y++) {
        for (unsigned x = 0; x < width; x++) {
            int on = x >= 2 && x < width-2 && y >= 2 && y < 10
                     && pixel(message[(x-2)/6], (x-2)%6, y-2);
            byte_out(on ? 0xff : 0);
            byte_out(on ? 0xff : 0);
        }
    }
    set(select_fd, 1, 0);
    close(serial); close(dc_fd); close(select_fd); close(gyro_cs);
    puts("ILI9341 wrote LINUX 5X8");
    return 0;
}
