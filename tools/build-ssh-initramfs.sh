#!/bin/sh
# Prepare a private lab initramfs for a single public-key SSH account.
set -eu
demo=false
if [ "$#" -eq 7 ] && [ "$1" = --demo ]; then
    demo=true
    kernel=$(realpath "$2")
    compact=$(realpath "$3")
    compiler=$(realpath "$4")
    dropbear=$(realpath "$5")
    sdk=$(realpath "$6")
    test "$(head -c 4 "$compact")" = bFLT && test "$(head -c 4 "$compiler")" = bFLT
elif [ "$#" -eq 7 ]; then
    kernel=$(realpath "$1")
    busybox=$(realpath "$2")
    dropbear=$(realpath "$3")
    sdk=$(realpath "$4")
    hostkey=$(realpath "$5")
    authorized=$(realpath "$6")
    test -s "$hostkey" && test -s "$authorized"
else
    echo "Usage: $0 built-linux busybox-src dropbear-src sdk hostkey authorized.pub new-output-dir" >&2
    echo "       $0 --demo built-linux compact-busybox tcc dropbear-src sdk new-output-dir" >&2
    exit 2
fi
fw=$(realpath "$(dirname "$0")/../firmware")
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
    if "$demo"; then
        sed -i 's/^#define DROPBEAR_SVR_PASSWORD_AUTH 0$/#define DROPBEAR_SVR_PASSWORD_AUTH 1/' localoptions.h
        echo '#define DROPBEAR_SVR_PUBKEY_AUTH 0' >> localoptions.h
    fi
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
if "$demo"; then
    cp "$compact" busybox/busybox_unstripped
else
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
fi
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
file /etc/passwd passwd 0644 0 0
file /etc/group group 0644 0 0
file /etc/shells shells 0644 0 0
file /etc/inetd.conf inetd.conf 0600 0 0
file /etc/dropbear/session session 0600 0 0
EOF
if "$demo"; then
    hash=$(printf '%s\n' user | openssl passwd -6 -stdin)
    printf 'root:!:0:0:root:/root:/bin/sh\nuser:%s:1000:1000:Demo:/home/user:/bin/sh\n' "$hash" > passwd
    printf 'root:x:0:\nuser:x:1000:\n' > group
    echo 'exec /bin/dropbear -i -w -r /etc/dropbear/hostkey 2>/dev/console' > session
    python3 -c 'from pathlib import Path; Path("placeholder").write_bytes(b"\x00\x00\x00\x0bssh-ed25519\x00\x00\x00\x40" + b"F429 HOST KEY PLACEHOLDER".ljust(64,b"!"))'
    cat >> archive.list <<EOF
dir /home 0755 0 0
dir /home/user 0700 1000 1000
file /etc/dropbear/hostkey placeholder 0600 0 0
file /bin/tcc $compiler 0755 0 0
EOF
else
    printf 'file /etc/dropbear/hostkey %s 0600 0 0\n' "$hostkey" >> archive.list
    printf 'file /root/.ssh/authorized_keys %s 0600 0 0\n' "$authorized" >> archive.list
fi
for applet in mount mkdir inetd sh uname cat sha256sum; do
    echo "slink /bin/$applet busybox 0777 0 0" >> archive.list
done
"$kernel/usr/gen_init_cpio" -t 0 archive.list > ssh.cpio
