#!/bin/sh
# Build a standalone BusyBox shell with a no-MMU ARM Linux toolchain.
set -eu
if [ "$#" -ne 3 ]; then
    echo "Usage: CROSS_COMPILE=... $0 /path/to/built/linux /path/to/busybox-source output-dir" >&2
    exit 2
fi
: "${CROSS_COMPILE:?set the no-MMU ARM Linux compiler prefix}"
kernel=$(realpath "$1")
busybox=$(realpath "$2")
mkdir -p "$3"
out=$(realpath "$3")
src=$(realpath "$(dirname "$0")/../firmware")
build=$out/busybox-build
mkdir -p "$build"
export ARCH=arm
make -C "$busybox" O="$build" allnoconfig
while IFS= read -r setting; do
    name=${setting%=y}
    "$kernel/scripts/config" --file "$build/.config" --enable "${name#CONFIG_}"
done < "$src/busybox-shell.config"
yes '' | make -C "$busybox" O="$build" oldconfig
while IFS= read -r setting; do
    grep -Fxq "$setting" "$build/.config" || { echo "Missing $setting" >&2; exit 1; }
done < "$src/busybox-shell.config"
make -C "$busybox" O="$build" -j"${JOBS:-4}" busybox_unstripped
# elf2flt already emitted bFLT; GNU strip only understands the intermediate ELF.
cp "$build/busybox_unstripped" "$out/busybox"
cp "$src/shell-init" "$out/shell-init"
(cd "$out"
 cat > shell-archive.list <<'LIST'
dir /bin 0755 0 0
dir /dev 0755 0 0
nod /dev/console 0600 0 0 c 5 1
file /bin/busybox busybox 0755 0 0
slink /bin/sh busybox 0777 0 0
slink /bin/uname busybox 0777 0 0
file /init shell-init 0755 0 0
LIST
 "$kernel/usr/gen_init_cpio" -t 0 shell-archive.list > shell.cpio
)
