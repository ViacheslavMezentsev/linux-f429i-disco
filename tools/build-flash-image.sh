#!/bin/sh
# Build the internal-Flash boot image in a private output directory.
set -eu
if [ "$#" -ne 5 ]; then
    echo "Usage: $0 xipImage board.dtb zlib-1.3.1.tar.gz kernel-dtc output-dir" >&2
    exit 2
fi
kernel=$(realpath "$1")
tree=$(realpath "$2")
archive=$(realpath "$3")
dtc=$(realpath "$4")
fw=$(realpath "$(dirname "$0")/../firmware")
umask 077
mkdir -p "$5"
cd "$5"
printf '%s  %s\n' 9a93b2b7dfdac77ceba5a558a580e74667dd6fede4585b91eefb60f03b72df23 "$archive" | sha256sum -c -
tar -xf "$archive" zlib-1.3.1/contrib/puff
"$dtc" -q -I dtb -O dts "$tree" -o boot.dts
python3 - "$kernel" "$dtc" <<'PY'
import pathlib
import subprocess
import sys
import zlib

path = pathlib.Path
marker = bytes(range(32))
with path("boot.dts").open("a") as out:
    out.write("\n&{/chosen} { rng-seed = [" + marker.hex(" ") + "]; };\n")
subprocess.run([sys.argv[2], "-q", "-I", "dts", "-O", "dtb", "-o", "boot.dtb", "boot.dts"], check=True)
tree = path("boot.dtb").read_bytes()
image = path(sys.argv[1]).read_bytes()
if not 0 < len(image) <= 0x2f0000 or len(tree) > 0x10000 or tree.count(marker) != 1:
    sys.exit("Unexpected SDRAM layout or rng-seed")
compressor = zlib.compressobj(9, zlib.DEFLATED, -15)
encoded = compressor.compress(image) + compressor.flush()
if zlib.decompress(encoded, -15) != image:
    sys.exit("Invalid compressed kernel")
path("image.deflate").write_bytes(encoded)
sizes = dict(IMAGE_SIZE=len(image), IMAGE_CRC=zlib.crc32(image), COMP_SIZE=len(encoded),
             TREE_SIZE=len(tree), TREE_CRC=zlib.crc32(tree), SEED_OFFSET=tree.index(marker))
path("sizes.h").write_text("".join(f"#define {name} {value}u\n" for name, value in sizes.items()))
PY
cat > payload.S <<'EOF'
.section .payload,"a"
.global compressed, tree
compressed: .incbin "image.deflate"
.balign 4
tree: .incbin "boot.dtb"
EOF
prefix=${CROSS_COMPILE:-arm-none-eabi-}
"${prefix}gcc" -Os -mcpu=cortex-m4 -mthumb -ffreestanding -fno-builtin \
    -ffunction-sections -fdata-sections -I. -Izlib-1.3.1/contrib/puff -nostartfiles \
    -T "$fw/flash.ld" -Wl,--gc-sections,-Map=flash.map -o flash.elf \
    "$fw/flash-start.S" "$fw/flash-boot.c" "$fw/flash-sdram.c" \
    zlib-1.3.1/contrib/puff/puff.c payload.S -lc -lgcc
"${prefix}objcopy" -O binary flash.elf flash.bin
wc -c flash.bin
sha256sum flash.bin
