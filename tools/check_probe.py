"""TC-01. Inspect the load addresses and entry vectors of the linked ELF."""
import struct
import sys
from pathlib import Path

data = Path(sys.argv[1]).read_bytes()
if data[:6] != b"\x7fELF\x01\x01":
    sys.exit("expected little-endian ELF32")
machine, = struct.unpack_from("<H", data, 18)
entry, offset = struct.unpack_from("<II", data, 24)
size, count = struct.unpack_from("<HH", data, 42)
base, limit = 0x20000000, 0x20001000
vectors = None
executable_entry = False
for index in range(count):
    kind, pos, vaddr, paddr, filesz, memsz, flags, _ = struct.unpack_from(
        "<8I", data, offset + index * size)
    if kind != 1:
        continue
    if not (base <= vaddr == paddr and vaddr + memsz <= limit
            and filesz <= memsz and pos + filesz <= len(data)):
        sys.exit("invalid SRAM load segment")
    if vaddr == base and filesz >= 8:
        vectors = struct.unpack_from("<II", data, pos)
    if flags & 1 and vaddr <= (entry & ~1) < vaddr + filesz:
        executable_entry = True
if machine != 40 or not executable_entry or not entry & 1 or vectors != (limit, entry):
    sys.exit("invalid ARM entry point or initial vectors")
print(f"TC-01 PASS: SP={limit:#010x}, PC={entry:#010x}")
