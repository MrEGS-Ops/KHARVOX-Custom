#!/usr/bin/env python3
"""Generate original KHARVOX resource mods from the user's exported DOOM (2016) declarations.

No Bethesda/id Software game data or third-party mod assets are distributed here.
Generated ZIPs are not selected or installed automatically.
"""
from __future__ import annotations

import argparse
import os
from pathlib import Path
import re
import sys
import tempfile
import zipfile

AI = "generated/decls/aiglobalsettings/default.decl;aiGlobalSettings"
GIB = "generated/decls/gorebehavior/gorebehavior/firearm/8_gauge.decl;goreBehavior"
MODS = {
    "aggressive": (AI, "KHARVOX-Aggressive-Demons.zip"),
    "gibs": (GIB, "KHARVOX-Enhanced-Gibs.zip"),
}
NUMBER = r"[+-]?(?:\d+(?:\.\d*)?|\.\d+)"


class ModGenerationError(ValueError):
    pass


def scalar(text: str, name: str, value: str) -> tuple[str, int]:
    # Only edit existing numeric declarations. Never invent a missing field.
    pattern = re.compile(
        r"(?m)^(?P<head>[ \t]*" + re.escape(name) +
        r"[ \t]*=[ \t]*)(?P<num>" + NUMBER + r")(?P<tail>[ \t]*;)")
    changed, count = pattern.subn(
        lambda m: m.group("head") + value + m.group("tail"), text)
    if not count:
        raise ModGenerationError("Original declaration is missing " + name)
    return changed, count


def aggressive(text: str, simultaneous: int, cooldown: float) -> str:
    if not re.search(r"\btokenData\s*=\s*\{", text) or not re.search(
            r"\bdifficulty_Entries\s*=\s*\{", text):
        raise ModGenerationError("Unrecognized original AI token structure")
    text, a = scalar(text, "secondsBetweenUses",
                     format(cooldown, ".2f").rstrip("0").rstrip("."))
    text, b = scalar(text, "maxSimultaneousUsers", str(simultaneous))
    if a != b:
        raise ModGenerationError(
            "AI cooldown and simultaneous-attacker field counts differ")
    return text


def enhanced_gibs(text: str, minimum: int, maximum: int, impulse: int) -> str:
    if not re.search(r"\bmultiGibInfo\s*=\s*\{", text):
        raise ModGenerationError("Unrecognized original gore structure")
    for field, value in (("minLimbsToRemove", minimum),
                         ("maxLimbsToRemove", maximum),
                         ("gibImpulse", impulse)):
        text, _ = scalar(text, field, str(value))
    return text


def load_original(root: Path, relative: str) -> tuple[bytes, str]:
    # Only local, user-exported ORIGINAL DOOM resource declarations are read.
    root = root.resolve(strict=True)
    candidate = root.joinpath(*relative.split("/"))
    if not candidate.is_file() or candidate.is_symlink():
        raise ModGenerationError(
            "Missing original DOOM resource: " + relative +
            "\nExport this declaration from your unmodified game first.")
    if not candidate.resolve().is_relative_to(root):
        raise ModGenerationError("Original resource path escapes export folder")
    data = candidate.read_bytes()
    if not 3 <= len(data) <= 2_000_000:
        raise ModGenerationError("Unexpected original declaration size")
    try:
        text = data.decode("utf-8-sig")
    except UnicodeDecodeError as exc:
        raise ModGenerationError("Original declaration is not UTF-8 text") from exc
    return data, text


def make_archive(path: Path, resource: str, data: bytes) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    fd, temporary = tempfile.mkstemp(
        prefix=".kharvox-mod-", suffix=".zip", dir=path.parent)
    os.close(fd)
    try:
        with zipfile.ZipFile(temporary, "w", zipfile.ZIP_DEFLATED) as zipfile_out:
            entry = zipfile.ZipInfo(resource, (2026, 1, 1, 0, 0, 0))
            entry.compress_type = zipfile.ZIP_DEFLATED
            entry.external_attr = 0o644 << 16
            zipfile_out.writestr(entry, data)
        with zipfile.ZipFile(temporary) as verification:
            if verification.namelist() != [resource] or verification.read(resource) != data:
                raise ModGenerationError("Generated ZIP failed verification")
        os.replace(temporary, path)
    finally:
        if os.path.exists(temporary):
            os.unlink(temporary)


def build(root: Path, out: Path, mods: tuple[str, ...],
          attackers: int, cooldown: float, min_limbs: int,
          max_limbs: int, impulse: int) -> None:
    # Validate ALL inputs before writing output; never silently enable mods.
    pending: list[tuple[Path, str, bytes]] = []
    for name in mods:
        resource, filename = MODS[name]
        original, text = load_original(root, resource)
        changed = (aggressive(text, attackers, cooldown) if name == "aggressive"
                   else enhanced_gibs(text, min_limbs, max_limbs, impulse))
        if b"\r\n" in original:
            changed = changed.replace("\r\n", "\n").replace("\n", "\r\n")
        data = changed.encode("utf-8")
        if original.startswith(b"\xef\xbb\xbf"):
            data = b"\xef\xbb\xbf" + data
        if data == original:
            raise ModGenerationError("Values did not change original resource: " + name)
        pending.append((out / filename, resource, data))
    for destination, resource, contents in pending:
        make_archive(destination, resource, contents)
        print("Created " + str(destination))
    print("Unselected by default. Use KHARVOX Custom Mods to enable after testing.")


def self_test() -> None:
    with tempfile.TemporaryDirectory(prefix="kharvox-mod-tests-") as tmp:
        root = Path(tmp) / "originals"
        out = Path(tmp) / "out"
        for name in (AI, GIB):
            (root / name).parent.mkdir(parents=True, exist_ok=True)
        (root / AI).write_bytes(
            b"{\n edit = {\n tokenData = {\n difficulty_Entries = {\n"
            b" item[0] = {\n secondsBetweenUses = 12;\n"
            b" maxSimultaneousUsers = 2;\n } } } } }\n")
        (root / GIB).write_bytes(
            b"{\r\n edit = {\r\n multiGibInfo = {\r\n"
            b" minLimbsToRemove = 1;\r\n maxLimbsToRemove = 3;\r\n"
            b" }\r\n gibImpulse = 25;\r\n } }\r\n")
        build(root, out, ("aggressive", "gibs"), 6, .5, 2, 5, 48)
        with zipfile.ZipFile(out / MODS["aggressive"][1]) as z:
            text = z.read(AI).decode()
            assert "secondsBetweenUses = 0.5;" in text
            assert "maxSimultaneousUsers = 6;" in text
        with zipfile.ZipFile(out / MODS["gibs"][1]) as z:
            data = z.read(GIB)
            assert b"minLimbsToRemove = 2;" in data
            assert b"maxLimbsToRemove = 5;" in data
            assert b"gibImpulse = 48;" in data
            assert b"\r\n" in data
        for bad in (
            lambda: aggressive("tokenData = {}", 6, .5),
            lambda: enhanced_gibs("edit = {}", 2, 5, 48)
        ):
            try:
                bad()
            except ModGenerationError:
                pass
            else:
                raise AssertionError("Unknown baseline must be rejected")
    print("KHARVOX packaged mod generator self-test passed")


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--self-test", action="store_true")
    p.add_argument("--list", action="store_true")
    p.add_argument("--source-dir", type=Path)
    p.add_argument("--output-dir", type=Path)
    p.add_argument("--only", choices=["aggressive", "gibs", "both"], default="both")
    p.add_argument("--attackers", type=int, default=6)
    p.add_argument("--cooldown", type=float, default=.5)
    p.add_argument("--min-limbs", type=int, default=2)
    p.add_argument("--max-limbs", type=int, default=5)
    p.add_argument("--gib-impulse", type=int, default=48)
    a = p.parse_args()
    if a.self_test:
        self_test()
        return 0
    if a.list:
        for mod, (resource, filename) in MODS.items():
            print(f"{mod}: original {resource}\n  produces {filename}")
        return 0
    if not a.source_dir or not a.output_dir:
        p.error("Provide --source-dir and --output-dir (see --list)")
    if not 1 <= a.attackers <= 16 or not 0 <= a.cooldown <= 20:
        p.error("--attackers range is 1-16; --cooldown is 0-20 seconds")
    if not 1 <= a.min_limbs <= a.max_limbs <= 8:
        p.error("Limb counts must be ordered within 1-8")
    if not 1 <= a.gib_impulse <= 200:
        p.error("Gib impulse must be 1-200")
    selected = tuple(MODS) if a.only == "both" else (a.only,)
    try:
        build(a.source_dir, a.output_dir, selected, a.attackers,
              a.cooldown, a.min_limbs, a.max_limbs, a.gib_impulse)
    except (ModGenerationError, OSError, zipfile.BadZipFile) as exc:
        print("Stopped safely: " + str(exc), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
