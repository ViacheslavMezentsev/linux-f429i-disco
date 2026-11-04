#!/bin/sh
# TC-09, run on the host with OpenSSH. Leave the transferred file for inspection.
set -eu
if [ "$#" -ne 4 ]; then
    echo "Usage: $0 board-ip client-key pinned-known-hosts new-result-directory" >&2
    exit 2
fi
address=$1
case "$address" in ''|*[!0-9.]*) echo "Expected an IPv4 address" >&2; exit 2;; esac
key=$(realpath "$2")
hosts=$(realpath "$3")
umask 077
mkdir -m 700 "$4"
cd "$4"
cat > ssh.conf <<EOF
Host disco
    HostName $address
    Port 2222
    User root
    IdentityFile "$key"
    UserKnownHostsFile "$hosts"
    HostKeyAlias linux-f429i-disco-lab
    StrictHostKeyChecking yes
    CheckHostIP no
    IdentitiesOnly yes
    BatchMode yes
    ConnectTimeout 120
    ServerAliveInterval 30
    ServerAliveCountMax 3
    ControlMaster auto
    ControlPath none
    ControlPersist 45
EOF
socket_dir=$(mktemp -d)
mux="ControlPath=$socket_dir/s"
trap 'ssh -o "$mux" -F ssh.conf -O exit disco >/dev/null 2>&1 || true; rmdir "$socket_dir" 2>/dev/null || true' EXIT
ssh -o "$mux" -F ssh.conf disco 'uname -r' > release
grep -Fx 6.14.5 release
dd if=/dev/urandom of=sent.bin bs=16384 count=1 status=none
test "$(wc -c < sent.bin)" -eq 16384
digest=$(sha256sum sent.bin | cut -d ' ' -f 1)
name=/tmp/tc09-$(printf '%.16s' "$digest").bin
# New OpenSSH defaults to SFTP. Older versions already use the SCP protocol.
legacy=-O
if LC_ALL=C scp -O 2>&1 | grep -Eq '(unknown|illegal|invalid) option'; then legacy=; fi
scp $legacy -o "$mux" -F ssh.conf sent.bin "disco:$name"
ssh -o "$mux" -F ssh.conf disco "sha256sum $name" > board.sha256
scp $legacy -o "$mux" -F ssh.conf "disco:$name" returned.bin
returned=$(sha256sum returned.bin | cut -d ' ' -f 1)
read -r onboard path < board.sha256
test "$path" = "$name"
test "$onboard" = "$digest"
test "$returned" = "$digest"
cmp sent.bin returned.bin
printf 'sent:     %s\non board: %s %s\nreturned: %s\n' "$digest" "$onboard" "$name" "$returned"
