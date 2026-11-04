// SPDX-License-Identifier: GPL-2.0
/* Fixed MB1075 B-01 SPI5 text framebuffer, TC-15. */
#include <linux/delay.h>
#include <linux/fb.h>
#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <linux/spi/spi.h>
#include <linux/uaccess.h>
#include "lcd-text.h"
#include "lcd-font.h"

#define WIDTH (LCD_COLS * 6)
#define HEIGHT (LCD_ROWS * 8)
#define FILL_ROWS 8
#define FILL_BYTES (WIDTH * FILL_ROWS * 2)
static_assert(HEIGHT % FILL_ROWS == 0);

struct panel {
	struct spi_device *spi;
	struct gpio_desc *dc;
	struct mutex lock;
	struct lcd_colors colors;
	u8 cells[LCD_ROWS * LCD_COLS];
	u8 *scan; /* DMA-safe scratch shared by fills and glyph rows */
};

static int command(struct panel *p, u8 op, const u8 *args, size_t n)
{
	int ret;
	gpiod_set_value_cansleep(p->dc, 0);
	ret = spi_write_then_read(p->spi, &op, 1, NULL, 0);
	gpiod_set_value_cansleep(p->dc, 1);
	if (!ret && n)
		ret = spi_write_then_read(p->spi, args, n, NULL, 0);
	return ret;
}

static int window(struct panel *p, unsigned x, unsigned y, unsigned w, unsigned h)
{
	u16 right = x + w - 1, bottom = y + h - 1;
	u8 col[] = {x >> 8, x, right >> 8, right};
	u8 row[] = {y >> 8, y, bottom >> 8, bottom};
	int ret = command(p, 0x2a, col, sizeof(col));
	if (!ret) ret = command(p, 0x2b, row, sizeof(row));
	if (!ret) ret = command(p, 0x2c, NULL, 0);
	return ret;
}

static void pixel(struct panel *p, unsigned x, u16 color)
{
	p->scan[2*x] = color >> 8;
	p->scan[2*x+1] = color;
}

static int clear_panel(struct panel *p, struct lcd_colors colors)
{
	int ret = window(p, 0, 0, WIDTH, HEIGHT);
	unsigned x, y;
	for (x = 0; x < WIDTH * FILL_ROWS; x++) pixel(p, x, colors.bg);
	for (y = 0; !ret && y < HEIGHT; y += FILL_ROWS)
		ret = spi_write(p->spi, p->scan, FILL_BYTES);
	if (!ret) {
		p->colors = colors;
		memset(p->cells, ' ', sizeof(p->cells));
	}
	return ret;
}

static int draw(struct panel *p, unsigned pos, const u8 *text, unsigned n)
{
	unsigned y, i, x;
	int ret = window(p, (pos % LCD_COLS)*6, (pos / LCD_COLS)*8, n*6, 8);
	for (y = 0; !ret && y < 8; y++) {
		for (i = 0; i < n; i++) {
			const u8 *g = glyph[text[i]-' '];
			for (x = 0; x < 6; x++)
				pixel(p, i*6+x, x < 5 && (g[x] & BIT(y)) ?
				      p->colors.fg : p->colors.bg);
		}
		ret = spi_write(p->spi, p->scan, n*12);
	}
	return ret;
}

static ssize_t text_read(struct fb_info *info, char __user *buf, size_t len, loff_t *pos)
{
	struct panel *p = info->par;
	ssize_t ret;
	mutex_lock(&p->lock);
	ret = simple_read_from_buffer(buf, len, pos, p->cells, sizeof(p->cells));
	mutex_unlock(&p->lock);
	return ret;
}

static ssize_t text_write(struct fb_info *info, const char __user *buf, size_t len, loff_t *pos)
{
	struct panel *p = info->par;
	size_t done = 0;
	int ret = 0;
	u8 line[LCD_COLS];
	if (*pos < 0) return -EINVAL;
	if (!len) return 0;
	if (*pos >= sizeof(p->cells)) return -ENOSPC;
	len = min_t(size_t, len, sizeof(p->cells) - *pos);
	mutex_lock(&p->lock);
	while (done < len) {
		size_t n = min_t(size_t, len - done, LCD_COLS - ((unsigned)*pos % LCD_COLS));
		size_t i;
		if (copy_from_user(line, buf + done, n)) { ret = -EFAULT; break; }
		for (i = 0; i < n; i++)
			if (line[i] < 32 || line[i] > 126) line[i] = '?';
		ret = draw(p, *pos, line, n);
		if (ret) break;
		memcpy(p->cells + *pos, line, n);
		done += n;
		*pos += n;
	}
	mutex_unlock(&p->lock);
	return done ? (ssize_t)done : ret;
}

static int text_ioctl(struct fb_info *info, unsigned int cmd, unsigned long arg)
{
	struct panel *p = info->par;
	struct lcd_colors c;
	int ret;
	if (cmd != LCD_CLEAR) return -ENOTTY;
	if (copy_from_user(&c, (void __user *)arg, sizeof(c))) return -EFAULT;
	mutex_lock(&p->lock);
	ret = clear_panel(p, c);
	mutex_unlock(&p->lock);
	return ret;
}

static int text_mmap(struct fb_info *info, struct vm_area_struct *vma)
{
	return -ENODEV;
}

static const struct fb_ops text_ops = {
	.owner = THIS_MODULE, .fb_read = text_read, .fb_write = text_write,
	.fb_ioctl = text_ioctl, .fb_mmap = text_mmap,
};

static int text_probe(struct spi_device *spi)
{
	struct fb_info *info;
	struct panel *p;
	int ret;
	info = framebuffer_alloc(sizeof(*p), &spi->dev);
	if (!info) return -ENOMEM;
	p = info->par;
	p->spi = spi;
	p->scan = devm_kmalloc(&spi->dev, FILL_BYTES, GFP_KERNEL);
	if (!p->scan) { ret = -ENOMEM; goto fail; }
	p->dc = devm_gpiod_get(&spi->dev, "dc", GPIOD_OUT_HIGH);
	if (IS_ERR(p->dc)) { ret = PTR_ERR(p->dc); goto fail; }
	mutex_init(&p->lock);
	ret = command(p, 1, NULL, 0);
	if (ret) goto fail;
	msleep(150);
	ret = command(p, 0x11, NULL, 0);
	if (ret) goto fail;
	msleep(150);
	ret = command(p, 0xf6, (const u8[]){1,0,0}, 3);
	if (!ret) ret = command(p, 0x36, (const u8[]){0x48}, 1);
	if (!ret) ret = command(p, 0x3a, (const u8[]){0x55}, 1);
	if (!ret) ret = clear_panel(p, (struct lcd_colors){0xffff, 0});
	if (!ret) ret = command(p, 0x29, NULL, 0);
	if (ret) goto fail;
	info->fbops = &text_ops;
	info->fix = (struct fb_fix_screeninfo){
		.id = "f429-text", .type = FB_TYPE_TEXT, .type_aux = FB_AUX_TEXT_MDA,
		.visual = FB_VISUAL_MONO01, .line_length = LCD_COLS,
		.smem_len = sizeof(p->cells),
	};
	info->var.xres = info->var.xres_virtual = LCD_COLS;
	info->var.yres = info->var.yres_virtual = LCD_ROWS;
	info->var.bits_per_pixel = 8;
	info->screen_buffer = p->cells;
	info->screen_size = sizeof(p->cells);
	ret = register_framebuffer(info);
	if (!ret) return 0;
fail:
	framebuffer_release(info);
	return dev_err_probe(&spi->dev, ret, "text framebuffer initialization failed\n");
}

static const struct of_device_id match[] = {
	{ .compatible = "disco,ili9341-text" }, {}
};
static const struct spi_device_id ids[] = { { "ili9341-text" }, {} };
static struct spi_driver text_driver = {
	.driver = { .name = "f429-text", .of_match_table = match, .suppress_bind_attrs = true },
	.id_table = ids, .probe = text_probe,
};
module_spi_driver(text_driver);
MODULE_LICENSE("GPL");
