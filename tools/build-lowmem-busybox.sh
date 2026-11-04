#!/bin/sh
# Reuse the BusyBox applet configuration from a built SSH profile.
set -eu
if [ "$#" -ne 5 ]; then
    echo "Usage: $0 uclibc-git busybox-src ssh-busybox.config bootlin-sdk new-output-dir" >&2
    exit 2
fi
libc=$(realpath "$1")
bb=$(realpath "$2")
config=$(realpath "$3")
sdk=$(realpath "$4")
fw=$(realpath "$(dirname "$0")/../firmware")
mkdir "$5"
out=$(realpath "$5")
cd "$out"
mkdir libc sysroot busybox
git -C "$libc" archive --format=tar -o "$out/libc.tar" ecbc412d0d93c8b7b77bec6539b290c9da5401cf
tar -xf libc.tar -C libc
export PATH="$sdk/bin:$PATH" ARCH=arm CROSS_COMPILE=arm-linux-
base=$(arm-linux-gcc -print-sysroot)
cp -a "$base/." sysroot/
sed "s|^KERNEL_HEADERS=.*|KERNEL_HEADERS=\"$base/usr/include\"|" "$fw/uclibc-lowmem.config" > libc/.config
yes '' | make -C libc oldconfig
make -C libc -j"${JOBS:-4}"
make -C libc install PREFIX="$out/sysroot"
sed "s|^CONFIG_SYSROOT=.*|CONFIG_SYSROOT=\"$out/sysroot\"|" "$config" > busybox/.config
yes '' | make -C "$bb" O="$out/busybox" oldconfig
make -C "$bb" O="$out/busybox" -j"${JOBS:-4}" busybox_unstripped
