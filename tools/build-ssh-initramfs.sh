#!/bin/sh
# Prepare a private lab initramfs for a single public-key SSH account.
set -eu
if [ "$#" -ne 7 ]; then
    echo "Usage: $0 built-linux busybox-src dropbear-src sdk hostkey authorized.pub new-output-dir" >&2
    exit 2
fi
kernel=$(realpath "$1")
busybox=$(realpath "$2")
dropbear=$(realpath "$3")
sdk=$(realpath "$4")
hostkey=$(realpath "$5")
authorized=$(realpath "$6")
fw=$(realpath "$(dirname "$0")/../firmware")
test -s "$hostkey" && test -s "$authorized"
test -x "$kernel/usr/gen_init_cpio"
umask 077
# gen_init_cpio's manifest takes paths separated by whitespace.
mkdir -m 700 "$7"
out=$(realpath "$7")
cd "$out"
mkdir target busybox
export PATH="$sdk/bin:$PATH" ARCH=arm CROSS_COMPILE=arm-linux-
(
    cd target
    cp "$fw/dropbear-localoptions.h" localoptions.h
    CC=arm-linux-gcc AR=arm-linux-ar RANLIB=arm-linux-ranlib \
      CFLAGS="-Os -ffunction-sections -fdata-sections" \
      LDFLAGS="-Wl,--gc-sections -Wl,-elf2flt=-s32768" ac_cv_func_fork=no \
      "$dropbear/configure" --host=arm-buildroot-uclinux-uclibcgnueabi --enable-static \
        --disable-zlib --disable-shadow --disable-syslog --disable-lastlog \
        --disable-utmp --disable-wtmp --disable-loginfunc
    arm-linux-gcc -Os -c "$fw/dropbear-time.c" -o dropbear-time.o
    make -j"${JOBS:-4}" PROGRAMS="dropbear scp" \
        LDFLAGS="-Wl,--gc-sections -Wl,-elf2flt=-s32768 dropbear-time.o -Wl,--wrap=clock_gettime,--wrap=time,--wrap=gettimeofday,--wrap=select,--wrap=sigaction"
)
make -C "$busybox" O="$out/busybox" allnoconfig
while IFS= read -r setting; do
    setting=${setting%=y}
    "$kernel/scripts/config" --file "$out/busybox/.config" --enable "${setting#CONFIG_}"
done < "$fw/busybox-shell.config"
for feature in INETD MOUNT CAT SHA256SUM HUSH_READ HUSH_IF HUSH_TEST TEST MKDIR SLEEP; do
    "$kernel/scripts/config" --file "$out/busybox/.config" --enable "$feature"
done
"$kernel/scripts/config" --file "$out/busybox/.config" --disable FEATURE_EDITING_FANCY_PROMPT
yes '' | make -C "$busybox" O="$out/busybox" oldconfig
make -C "$busybox" O="$out/busybox" -j"${JOBS:-4}" busybox_unstripped
printf '%s\n' '2222 stream tcp nowait root /bin/sh sh /etc/dropbear/session' > inetd.conf
printf '%s\n' 'root:x:0:0:root:/root:/bin/sh' > passwd
printf '%s\n' 'root:x:0:' > group
echo /bin/sh > shells
echo 'exec /bin/dropbear -i -r /etc/dropbear/hostkey 2>/dev/console' > session
cat > archive.list <<EOF
dir /bin 0755 0 0
dir /dev 0755 0 0
nod /dev/console 0600 0 0 c 5 1
dir /etc 0755 0 0
dir /etc/dropbear 0700 0 0
dir /root 0700 0 0
dir /root/.ssh 0700 0 0
dir /tmp 01777 0 0
dir /var 0755 0 0
dir /var/run 0755 0 0
dir /proc 0555 0 0
file /bin/busybox busybox/busybox_unstripped 0755 0 0
file /bin/dropbear target/dropbear 0755 0 0
file /bin/scp target/scp 0755 0 0
file /init $fw/ssh-init 0755 0 0
file /etc/dropbear/hostkey $hostkey 0600 0 0
file /etc/passwd passwd 0644 0 0
file /etc/group group 0644 0 0
file /etc/shells shells 0644 0 0
file /etc/inetd.conf inetd.conf 0600 0 0
file /etc/dropbear/session session 0600 0 0
file /root/.ssh/authorized_keys $authorized 0600 0 0
EOF
for applet in mount mkdir inetd sh uname cat sha256sum; do
    echo "slink /bin/$applet busybox 0777 0 0" >> archive.list
done
"$kernel/usr/gen_init_cpio" -t 0 archive.list > ssh.cpio
