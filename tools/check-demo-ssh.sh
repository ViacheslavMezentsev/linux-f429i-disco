#!/bin/sh
# TC-12. Pin the board's Ed25519 public key read on the physical UART.
set -eu
if [ "$#" -ne 3 ]; then
    echo "Usage: $0 board-ip uart-public-key-hex new-results-directory" >&2
    exit 2
fi
ip=$1
case "$ip" in ''|*[!0-9.]*) exit 2;; esac
umask 077
mkdir "$3"
cd "$3"
python3 - "$2" <<'PY'
import base64
from pathlib import Path
import struct
import sys
key = bytes.fromhex(sys.argv[1])
if len(key) != 32:
    sys.exit("Expected the 32-byte public key from COM21")
wire = struct.pack(">I", 11) + b"ssh-ed25519" + struct.pack(">I", 32) + key
Path("known_hosts").write_text("disco-demo ssh-ed25519 " + base64.b64encode(wire).decode() + "\n")
PY
cat > ssh.conf <<EOF
Host disco
    HostName $ip
    Port 2222
    User user
    HostKeyAlias disco-demo
    UserKnownHostsFile "$PWD/known_hosts"
    GlobalKnownHostsFile /dev/null
    StrictHostKeyChecking yes
    PubkeyAuthentication no
    PreferredAuthentications password
    NumberOfPasswordPrompts 1
    ConnectTimeout 90
EOF
reject() {
    account=$1
    pass=$2
    if SSHPASS="$pass" sshpass -e ssh -o ControlPath=none -F ssh.conf -l "$account" disco 'echo unexpected' >"$account.out" 2>"$account.err"; then
        echo "Unexpected login: $account" >&2; exit 1
    fi
    grep -qi 'permission denied' "$account.err" && test ! -s "$account.out"
}
reject user invalid-password
reject root user
tmp=$(mktemp -d)
sock=$tmp/s
trap 'ssh -S "$sock" -F ssh.conf -O exit disco >/dev/null 2>&1 || :; rmdir "$tmp" || :' EXIT
export SSHPASS=user
sshpass -e ssh -o ControlMaster=yes -o ControlPersist=60 -S "$sock" -F ssh.conf disco 'exec uname -r' > release
grep -Fx 6.14.5 release
run() { ssh -S "$sock" -F ssh.conf disco "$@"; }
run 'exec cat /proc/self/status' > status
awk '/^Uid:/ || /^Gid:/ { print; if ($2!=1000 || $3!=1000 || $4!=1000 || $5!=1000) exit 1; seen++ } END { if(seen!=2) exit 1 }' status
if run 'exec cat /etc/dropbear/hostkey' > /dev/null 2>protected.err; then exit 1; fi
grep -qi 'permission denied' protected.err
dd if=/dev/urandom of=sent.bin bs=16384 count=1 status=none
digest=$(sha256sum sent.bin | cut -d' ' -f1)
name=/tmp/user-$(printf '%.16s' "$digest")
legacy=-O
if LC_ALL=C scp -O 2>&1 | grep -Eq '(unknown|illegal|invalid) option'; then legacy=; fi
scp $legacy -o "ControlPath=$sock" -F ssh.conf sent.bin "disco:$name"
run "exec sha256sum $name" > board.sha256
scp $legacy -o "ControlPath=$sock" -F ssh.conf "disco:$name" returned.bin
cmp sent.bin returned.bin
grep -Fx "$digest  $name" board.sha256
cat > hello.c <<'C'
int puts(const char *);
int main(void) { puts("Hello, World!"); return 0; }
C
scp $legacy -o "ControlPath=$sock" -F ssh.conf hello.c "disco:$name.c"
run "/bin/tcc $name.c $name.bflt && exec $name.bflt" > stdout
printf 'Hello, World!\n' | cmp - stdout
cat stdout
ssh-keygen -lf known_hosts
