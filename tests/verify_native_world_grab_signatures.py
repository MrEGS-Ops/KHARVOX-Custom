"""Static preflight for the active, validated world-registry discovery path.

The retired FocusTracker code-patch probes are intentionally absent.
This is NOT a runtime validation of a DOOM engine ABI or a movement test.
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
native = (ROOT / "src/camera/ObjectDiscoveryNative.inc").read_text(encoding="utf-8")
camera = (ROOT / "src/camera/CameraHook.cpp").read_text(encoding="utf-8")
xr = (ROOT / "src/openxr/OpenXRBootstrap.cpp").read_text(encoding="utf-8")

# Production discovery must use the validated game-owned entity registry,
# not retired inline code-patching / FocusTracker candidate probes.
for marker in (
    "struct ObjectDiscoveryManagedEntry",
    "static_assert(sizeof(ObjectDiscoveryManagedEntry)==32)",
    "constexpr unsigned char expectedCode[]",
    "0x48,0x8b,0x05,0x2c,0x50,0x7b,0x05",
    "imageBase+0x35a69d",
    "imageBase+0x5b0f6d0",
    "gameVtable!=imageBase+0x201e3d8",
    "std::memcmp(code,expectedCode,sizeof(code))",
    "objectDiscoveryTryManagedPool(",
    "objectDiscoveryReadBodyPose(",
    "objectDiscoveryWriteBody(",
    "if(!item.nativeMotionObserved||!objectDiscoveryAxisOk(axis))return false;",
    "if(!nearest)return;",
    "if(!held.nativeFollowConfirmed)return;",
    "objectDiscoverySourceMode.store(",
):
    assert marker in native, f"Missing validated native discovery contract: {marker}"
assert "objectDiscoveryGameTick();" in camera
assert "KharvoxCameraReadObjectGrip(objectFeedback)" in xr
assert "installNativeWorldGrabTraceCandidateHooks(" not in camera
assert "installNativeWorldGrabResolvedEntityHooks(" not in camera
print("PASS: managed registry signature gates, strict physics reads/writes, native grip feedback")
