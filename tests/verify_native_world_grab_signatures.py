"""Read-only signature contract for DOOM 2016 native trace observation.

Expected bytes were recovered from the owner's v3 offline executable report.
No DOOM binary is distributed or required. This verifies only our hard-coded
hook sites and relocation invariants, not the engine ABI or playable grabbing.
"""
from __future__ import annotations
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
source = (ROOT / "src" / "camera" / "CameraHook.cpp").read_text(encoding="utf-8")

def signature(name: str) -> bytes:
    match = re.search(
        rf"static constexpr unsigned char {re.escape(name)}\[14\]\s*\{{([^}}]+)\}};",
        source,
    )
    assert match, f"Missing {name} signature declaration"
    values = re.findall(r"0x[0-9A-Fa-f]+", match.group(1))
    result = bytes(int(value, 16) for value in values)
    assert len(result) == 14, (name, len(result))
    return result

primary = signature("sigPrimary")
secondary = signature("sigSecondary")
assert primary == bytes.fromhex("48 8B 0D 69 6B DC 04 48 8D 55 6C 48 8B 01")
assert secondary == bytes.fromhex("48 8B 0D E1 6A DC 04 48 8D 55 DC 48 8B 01")
assert primary[7:11] == bytes.fromhex("48 8D 55 6C")
assert secondary[7:11] == bytes.fromhex("48 8D 55 DC")
assert primary[11:] == secondary[11:] == bytes.fromhex("48 8B 01")

def absolute_rip_target(rva: int, sig: bytes) -> int:
    """RIP for instruction at site points seven bytes after MOV [RIP+disp32]."""
    assert sig[:3] == bytes.fromhex("48 8B 0D")
    return rva + 7 + int.from_bytes(sig[3:7], "little", signed=True)

assert absolute_rip_target(0xD48B60, primary) == 0x5B0F6D0
assert absolute_rip_target(0xD48BE8, secondary) == 0x5B0F6D0
assert "installNativeWorldGrabTraceCandidateHooks(image);" in source
assert "KharvoxCameraReadWorldGrabTraceCandidate(" in source
assert "observeNativeWorldGrabTraceCandidate" in source
print("PASS: DOOM FocusTracker candidate hook signatures, native shared manager, and integration")
# DOOM's native resolver has returned a non-null entity. These two
# fourteen-byte windows immediately begin with its own vtable load and
# predicate call. Relocating this whole span requires NO rel32 adjustment.
# The predicate must continue executing identically after the probe.
resolved = re.search(
    r"static constexpr unsigned char signature\[14\]\s*\{([^}]+)\};",
    source,
)
assert resolved, "Missing post-entity-resolution 14-byte signature"
resolved_bytes = bytes(int(x,16) for x in re.findall(
    r"0x[0-9A-Fa-f]+", resolved.group(1)
))
assert resolved_bytes == bytes.fromhex(
    "48 8B 10 48 8B C8 FF 92 00 06 00 00 84 C0"
)
assert "{0xD48B7C,0}" in source and "{0xD48C04,1}" in source
assert "installNativeWorldGrabResolvedEntityHooks(image);" in source
assert "observeNativeWorldGrabResolvedEntity" in source
assert "KharvoxCameraReadWorldGrabResolvedEntity(" in source
assert "std::memcpy(p,signature,sizeof(signature))" in source
assert "nativeGrabFocusObservationEnabled.load(" in source
print("PASS: native non-null entity resolver hook signatures and dev-mode gating")

