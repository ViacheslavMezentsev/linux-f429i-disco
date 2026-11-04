#!/bin/sh
# Create a private TC-11 archive, keeping the SSH account and host key of the base manifest.
set -eu
if [ "$#" -ne 5 ]; then
    echo "Usage: $0 gen_init_cpio ssh-archive.list compact-busybox tcc output.cpio" >&2
    exit 2
fi
generator=$(realpath "$1")
base=$(realpath "$2")
busybox=$(realpath "$3")
compiler=$(realpath "$4")
out=$(realpath -m "$5")
for binary in "$busybox" "$compiler"; do
    case "$binary" in *[[:space:]]*) echo "Whitespace in binary path" >&2; exit 1;; esac
    test "$(head -c 4 "$binary")" = bFLT || { echo "Expected bFLT: $binary" >&2; exit 1; }
done
umask 077
tmp=$(mktemp -d "$(dirname "$out")/.initramfs-XXXXXX")
trap 'rm -f "$tmp/archive.list" "$tmp/image"; rmdir "$tmp"' EXIT
awk -v bb="$busybox" -v cc="$compiler" '
    $2 == "/bin/tcc" { exit 1 }
    $1 == "file" && $2 == "/bin/busybox" { $3=bb; count++ }
    { print }
    END { if (count != 1) exit 1; print "file /bin/tcc", cc, "0755 0 0" }
' "$base" > "$tmp/archive.list"
# Relative source paths in the base manifest belong to its own directory.
(cd "$(dirname "$base")" && "$generator" -t 0 "$tmp/archive.list") > "$tmp/image"
mv "$tmp/image" "$out"
