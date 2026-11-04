#!/bin/sh
# Build the board's limited two-argument native compiler in a new directory.
set -eu
if [ "$#" -ne 3 ]; then
    echo "Usage: $0 tinycc-git-checkout bootlin-sdk new-output-dir" >&2
    exit 2
fi
src=$(realpath "$1")
sdk=$(realpath "$2")
fw=$(realpath "$(dirname "$0")/../firmware")
rev=d9c7e2e1a4db0107f0a7cdf00791d56961ee813a
mkdir "$3"
cd "$3"
git -C "$src" archive --format=tar -o "$PWD/source.tar" "$rev"
tar -xf source.tar
patch -s -p1 < "$fw/tcc-thumb-bflt.patch"
./configure --enable-cross >/dev/null
cp "$fw/tcc-driver.c" driver.c
mkdir -p shim/sys
touch shim/sys/mman.h
cc=arm-none-eabi-gcc
flags="-mcpu=cortex-m4 -mthumb -mfloat-abi=soft -Os -ffunction-sections -fdata-sections"
arm-none-eabi-as -mcpu=cortex-m4 -mthumb "$fw/tcc-puts.S" -o puts.o
arm-none-eabi-objcopy -O binary -j .text puts.o puts.bin
python3 -c 'from pathlib import Path; print("static const unsigned char puts_thumb[] = {" + ",".join(map(str, Path("puts.bin").read_bytes())) + "};")' > runtime.h
$cc $flags -fno-strict-aliasing -DONE_SOURCE -DTCC_TARGET_ARM -DTCC_ARM_EABI -DTCC_ARM_VFP \
    -DCONFIG_TCC_STATIC -Ishim -I. -c driver.c -o driver.o
$cc $flags -c "$fw/tcc-sys.S" -o sys.o
$cc $flags -c "$fw/tcc-glue.c" -o glue.o
"$sdk/bin/arm-linux-gcc" -nostdlib -Wl,--gc-sections,-elf2flt=-s8192 -o tcc sys.o driver.o glue.o \
    -Wl,--start-group "$($cc $flags -print-file-name=libc_nano.a)" "$($cc $flags -print-libgcc-file-name)" -Wl,--end-group
