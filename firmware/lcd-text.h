/* Shared private fbdev command. Two native-endian RGB565 values. */
#ifndef F429_LCD_TEXT_H
#define F429_LCD_TEXT_H
#include <linux/types.h>
#include <linux/ioctl.h>
struct lcd_colors { __u16 fg, bg; };
#define LCD_CLEAR _IOW('L', 0x51, struct lcd_colors)
#define LCD_COLS 40
#define LCD_ROWS 40
/* Single-byte CP437 line drawing, in addition to printable ASCII. */
enum lcd_box {
    LCD_VERT=0xb3, LCD_TR=0xbf, LCD_BL=0xc0, LCD_HORIZ=0xc4, LCD_BR=0xd9, LCD_TL=0xda
};
#endif
