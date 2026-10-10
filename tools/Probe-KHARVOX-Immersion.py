#!/usr/bin/env python3
"""Read-only inventory of original DOOM (2016) pickup/glow resources.

This identifies candidate materials and FX for independently developed
KHARVOX Immersion Controls. No game files or ZIPs are written.
"""
import argparse
from pathlib import Path
import struct
import tempfile

KEYWORDS = ("pickup", "highlight", "outline", "interact", "collectible", "glow", "ammo", "loot")
ROOTS = ("generated/decls/material/", "generated/decls/fx/")


def scan_index(index: Path) -> list[str]:
    with index.open("rb") as f:
        if f.read(4) != b"\x05SER":
            raise ValueError("Unknown DOOM v5 index format")
        length = index.stat().st_size
        index_size = struct.unpack(">I", f.read(4))[0]
        if length < 36 or index_size != length - 32 or f.read(24) != bytes(24):
            raise ValueError("Invalid DOOM index header")
        number = struct.unpack(">I", f.read(4))[0]
        if number > 200000:
            raise ValueError("Too many resource records")
        def name():
            data = f.read(4)
            if len(data) != 4:
                raise ValueError("Truncated index")
            n = struct.unpack("<I", data)[0]
            if n > 16384:
                raise ValueError("Invalid name size")
            data = f.read(n)
            if len(data) != n:
                raise ValueError("Truncated name")
            return data.decode("utf-8")
        candidates = set()
        for _ in range(number):
            if len(f.read(4)) != 4:
                raise ValueError("Truncated record")
            name()  # type
            name()  # short name
            filename = name().replace("\\", "/")
            tail = f.read(21)
            if len(tail) != 21:
                raise ValueError("Truncated resource offsets")
            lowered = filename.casefold()
            if tail[-1] == 0 and lowered.startswith(ROOTS) and any(
                    word in lowered for word in KEYWORDS):
                candidates.add(filename)
        if f.tell() != length:
            raise ValueError("Unexpected index tail")
        return sorted(candidates, key=str.casefold)


def self_test():
    with tempfile.TemporaryDirectory() as folder:
        index = Path(folder) / "gameresources.index"
        def encoded(value):
            raw = value.encode("utf-8")
            return struct.pack("<I", len(raw)) + raw
        names = (
            "generated/decls/material/models/pickups/ammo_pickup.decl;material",
            "generated/decls/material/models/environment/wall.decl;material",
            "generated/decls/fx/fx/gui/pickup_glow.decl;fx")
        records = b""
        for i, filename in enumerate(names):
            records += (struct.pack(">i", i) + encoded("material")
                        + encoded("sample") + encoded(filename)
                        + struct.pack(">qiiiB", 16, 3, 3, 0, 0))
        index.write_bytes(b"\x05SER" + struct.pack(">I", len(records) + 4)
                          + bytes(24) + struct.pack(">I", len(names)) + records)
        found = scan_index(index)
        assert len(found) == 2 and any("pickup_glow" in f for f in found)
        assert not any("environment/wall" in f for f in found)
        print("KHARVOX immersion resource inventory self-test passed")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--self-test", action="store_true")
    parser.add_argument("--game-dir", type=Path)
    args = parser.parse_args()
    if args.self_test:
        self_test()
        return
    if args.game_dir is None:
        parser.error("Provide --game-dir")
    index = args.game_dir / "base" / "gameresources.index"
    for path in scan_index(index):
        print(path)
    print("Read-only inspection complete; no files changed.")


if __name__ == "__main__":
    main()
