#!/bin/sh
# Usage: sh tools/build-initramfs.sh /path/to/configured/linux output-directory
set -eu
if [ "$#" -ne 2 ]; then
    echo "Usage: $0 /path/to/built/linux output-directory" >&2
    exit 2
fi
kernel=$(realpath "$1")
if [ ! -x "$kernel/usr/gen_init_cpio" ]; then
    echo "Build usr/gen_init_cpio in the kernel tree first." >&2
    exit 1
fi
mkdir -p "$2"
out=$(realpath "$2")
src=$(dirname "$0")/../firmware/linux-init.S
cross=${CROSS_COMPILE:-arm-none-eabi-}
"${cross}gcc" -mcpu=cortex-m4 -mthumb -nostdlib -Wl,--build-id=none,-Ttext=0,-e,_start "$src" -o "$out/init.elf"
"${cross}objcopy" -O binary -j .text "$out/init.elf" "$out/init.text"
# bFLT v4. Code is Thumb, position independent and contains no relocations.
python3 - "$out" <<'PY'
import struct
import sys
from pathlib import Path
out = Path(sys.argv[1])
code = (out / "init.text").read_bytes()
end = 64 + len(code)
header = struct.pack(">4s15I", b"bFLT", 4, 65, end, end, end, 4096,
                     end, 0, 1, 0, 0, 0, 0, 0, 0)
(out / "init").write_bytes(header + code)
PY
(cd "$out"
 printf '%s\n' 'dir /dev 0755 0 0' 'nod /dev/console 0600 0 0 c 5 1' 'file /init init 0755 0 0' > initramfs.list
 "$kernel/usr/gen_init_cpio" -t 0 initramfs.list > initramfs.cpio
)
