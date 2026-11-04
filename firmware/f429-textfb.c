// SPDX-License-Identifier: GPL-2.0
/* Fixed MB1075 B-01 SPI5 text framebuffer, TC-15. */
#include <linux/completion.h>
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

/* Compare submission strategies without changing the bus clock or pixel format. */
static bool batch_fill = true;
module_param(batch_fill, bool, 0644);
MODULE_PARM_DESC(batch_fill, "Submit fill transfers in groups of eight");

static unsigned int pipeline_rows;
module_param(pipeline_rows, uint, 0644);
MODULE_PARM_DESC(pipeline_rows, "Text DMA buffers: 4 or 8 scanlines each, otherwise synchronous");

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

static int fill_pixels(struct panel *p)
{
	enum { batch = 8, blocks = HEIGHT / FILL_ROWS };
	struct spi_transfer *xfers = NULL;
	unsigned i;
	int ret = 0;

	if (READ_ONCE(batch_fill))
		xfers = kcalloc(batch, sizeof(*xfers), GFP_KERNEL);
	if (!xfers) {
		for (i = 0; !ret && i < blocks; i++)
			ret = spi_write(p->spi, p->scan, FILL_BYTES);
		return ret;
	}
	for (i = 0; i < batch; i++) {
		xfers[i].tx_buf = p->scan;
		xfers[i].len = FILL_BYTES;
	}
	for (i = 0; !ret && i < blocks; i += batch)
		ret = spi_sync_transfer(p->spi, xfers, min_t(unsigned, batch, blocks - i));
	kfree(xfers);
	return ret;
}

static int clear_panel(struct panel *p, struct lcd_colors colors)
{
	int ret = window(p, 0, 0, WIDTH, HEIGHT);
	unsigned x;
	for (x = 0; x < WIDTH * FILL_ROWS; x++) pixel(p, x, colors.bg);
	if (!ret) ret = fill_pixels(p);
	if (!ret) {
		p->colors = colors;
		memset(p->cells, ' ', sizeof(p->cells));
	}
	return ret;
}

static void render(u8 *dst, const struct lcd_colors *colors, const u8 *line,
		   unsigned width, unsigned first, unsigned rows)
{
	unsigned y, i, x;
	for (y = first; y < first + rows; y++) {
		for (i = 0; i < width; i++) {
			const u8 *g = glyph[line[i] - ' '];
			for (x = 0; x < 6; x++) {
				u16 rgb = x < 5 && (g[x] & BIT(y)) ? colors->fg : colors->bg;
				*dst++ = rgb >> 8;
				*dst++ = rgb;
			}
		}
	}
}

struct slot {
	struct spi_message msg;
	struct spi_transfer xfer;
	struct completion done;
	bool pending;
};

static void sent(void *ctx)
{
	complete(ctx);
}

static int join(struct slot *slot)
{
	if (!slot->pending) return 0;
	wait_for_completion(&slot->done);
	slot->pending = false;
	return slot->msg.status;
}

/* One rectangular window stays open while the SPI queue consumes the blocks. */
static int draw_blocks(struct panel *p, const u8 *text, unsigned width,
		       unsigned lines, unsigned rows)
{
	struct slot *slots = NULL;
	u8 *buf[2] = { p->scan, NULL };
	unsigned block, total;
	int ret = 0;
	if (rows == 4 || rows == 8) {
		slots = kcalloc(2, sizeof(*slots), GFP_KERNEL);
		if (!slots) return -ENOMEM;
		buf[1] = rows == 4 ? p->scan + FILL_BYTES / 2 : kmalloc(FILL_BYTES, GFP_KERNEL);
		if (!buf[1]) { kfree(slots); return -ENOMEM; }
		for (block = 0; block < 2; block++) {
			struct slot *s = &slots[block];
			init_completion(&s->done);
			spi_message_init(&s->msg);
			s->msg.complete = sent;
			s->msg.context = &s->done;
			s->xfer.tx_buf = buf[block];
			s->xfer.len = width * 12 * rows;
			spi_message_add_tail(&s->xfer, &s->msg);
		}
	} else {
		rows = 8;
	}
	total = lines * 8 / rows;
	for (block = 0; !ret && block < total; block++) {
		unsigned idx = slots ? block % 2 : 0;
		unsigned y = block * rows;
		if (slots && (ret = join(&slots[idx]))) break;
		render(buf[idx], &p->colors, text + (y / 8) * width, width, y % 8, rows);
		if (slots) {
			ret = spi_async(p->spi, &slots[idx].msg);
			slots[idx].pending = !ret;
		} else {
			ret = spi_write(p->spi, buf[0], width * 12 * rows);
		}
	}
	if (slots) {
		for (block = 0; block < 2; block++) {
			int status = join(&slots[block]);
			if (!ret) ret = status;
		}
		if (rows == 8) kfree(buf[1]);
		kfree(slots);
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
		unsigned col = (unsigned)*pos % LCD_COLS;
		unsigned width = min_t(size_t, len - done, LCD_COLS - col);
		unsigned lines = width == LCD_COLS ? (len - done) / LCD_COLS : 1;
		unsigned n = width * lines, i;
		u8 *chars = line;
		if (lines > 1) {
			chars = kmalloc(n, GFP_KERNEL);
			if (!chars) { ret = -ENOMEM; break; }
		}
		if (copy_from_user(chars, buf + done, n)) {
			ret = -EFAULT;
		} else {
			for (i = 0; i < n; i++)
				if (chars[i] < 32 || chars[i] > 126) chars[i] = '?';
			ret = window(p, col * 6, ((unsigned)*pos / LCD_COLS) * 8, width * 6, lines * 8);
			if (!ret) ret = draw_blocks(p, chars, width, lines, READ_ONCE(pipeline_rows));
			if (!ret) {
				memcpy(p->cells + *pos, chars, n);
				done += n;
				*pos += n;
			}
		}
		if (chars != line) kfree(chars);
		if (ret) break;
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
