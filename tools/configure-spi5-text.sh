#!/bin/sh
# Add the fixed-board, built-in SPI framebuffer to a prepared Linux 6.14.5 tree.
set -eu
if [ "$#" -ne 1 ]; then echo "Usage: $0 configured-linux-6.14.5" >&2; exit 2; fi
fw=$(realpath "$(dirname "$0")/../firmware")
cd "$1"
test -f .config
test -f drivers/video/fbdev/Kconfig
cp "$fw"/f429-textfb.c "$fw"/lcd-text.h "$fw"/lcd-font.h drivers/video/fbdev/
if ! grep -q '^config FB_F429_TEXT$' drivers/video/fbdev/Kconfig; then
    cat >>drivers/video/fbdev/Kconfig <<'EOF'

config FB_F429_TEXT
    bool "STM32F429 Discovery SPI text panel"
    depends on FB=y && SPI_STM32=y && GPIOLIB
EOF
fi
rule='obj-$(CONFIG_FB_F429_TEXT) += f429-textfb.o'
grep -Fxq "$rule" drivers/video/fbdev/Makefile || printf '%s\n' "$rule" >> drivers/video/fbdev/Makefile
scripts/config -e SPI -e SPI_STM32 -e STM32_DMA -e FB -e FB_DEVICE -e FB_F429_TEXT \
    -d FRAMEBUFFER_CONSOLE -d VT
make ARCH=arm CROSS_COMPILE="${CROSS_COMPILE:-arm-none-eabi-}" olddefconfig
grep -Fxq CONFIG_FB_F429_TEXT=y .config
grep -Fxq CONFIG_FB_DEVICE=y .config
