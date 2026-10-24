"""Validate the supplied Doom IWAD before flashing the 11 MiB WAD partition."""
import struct
import sys
from pathlib import Path

def check(path):
    data = Path(path).read_bytes()
    if not 12 <= len(data) <= 0xB00000:
        raise ValueError("WAD size must fit the 11 MiB partition")
    magic, count, directory = struct.unpack_from("<4sII", data)
    if magic != b"IWAD" or not count or directory < 12 or directory + count*16 > len(data):
        raise ValueError("Invalid IWAD header or directory")
    entries = {}
    for i in range(count):
        pos, size, name = struct.unpack_from("<II8s", data, directory + i*16)
        if pos + size > len(data):
            raise ValueError("Lump exceeds file size: " + repr(name))
        entries[name.rstrip(b"\0").upper()] = data[pos:pos+size]
    for name in (b"PLAYPAL", b"COLORMAP", b"PNAMES", b"TEXTURE1", b"E1M1"):
        if name not in entries:
            raise ValueError("Missing required Doom 1 lump: " + name.decode())
    if len(entries[b"PLAYPAL"]) < 768:
        raise ValueError("Invalid PLAYPAL")
    pnames = entries[b"PNAMES"]
    if len(pnames) < 4:
        raise ValueError("Invalid PNAMES")
    patches, = struct.unpack_from("<I", pnames)
    if 4 + 8*patches > len(pnames):
        raise ValueError("PNAMES table exceeds lump length")
    print("WAD OK:", len(data), "bytes,", count, "lumps,", patches, "patch names")
    return len(data), count

if __name__ == "__main__":
    try:
        check(sys.argv[1])
    except (OSError, ValueError, IndexError, struct.error) as exc:
        print("WAD ERROR:", exc, file=sys.stderr)
        sys.exit(1)
