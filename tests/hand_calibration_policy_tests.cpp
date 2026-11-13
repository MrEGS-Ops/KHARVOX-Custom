#include "../src/hands/HandCalibrationPolicy.h"
#include "../src/hands/HandAimAlignmentPolicy.h"
#include "../src/hands/HandModelRollPolicy.h"

#include <cassert>

int main() {
    using namespace kharvox::hands;

    // Both left-hand variants reuse the verified upright right-hand GLB,
    // mirrored once in local X. Adding a left-only 180-degree roll on top
    // would turn that into a VERTICAL mirror (the reported upside-down bug).
    assert(handReferenceAssetIndex(true,HandModelKind::Fist)==1u);
    assert(handReferenceAssetIndex(false,HandModelKind::Fist)==1u);
    assert(handReferenceAssetIndex(true,HandModelKind::GunHolding)==3u);
    assert(handReferenceAssetIndex(false,HandModelKind::GunHolding)==3u);
    assert(handMeshMirroredFromRight(true,HandModelKind::Fist));
    assert(handMeshMirroredFromRight(true,HandModelKind::GunHolding));
    assert(!handMeshMirroredFromRight(false,HandModelKind::Fist));
    assert(!handMeshMirroredFromRight(false,HandModelKind::GunHolding));
    assert(handModelControllerForwardRollDegrees(true,HandModelKind::Fist)==0.f);
    assert(handModelControllerForwardRollDegrees(true,HandModelKind::GunHolding)==0.f);
    assert(handModelControllerForwardRollDegrees(false,HandModelKind::Fist)==0.f);
    assert(handModelControllerForwardRollDegrees(false,HandModelKind::GunHolding)==0.f);

    bool plusWasDown{};
    auto mode=updateHandCalibrationMode(CalibrationMode::Rotation,true,plusWasDown);
    assert(mode==CalibrationMode::Position);
    mode=updateHandCalibrationMode(mode,true,plusWasDown);
    assert(mode==CalibrationMode::Position); // held key / second eye
    mode=updateHandCalibrationMode(mode,false,plusWasDown);
    mode=updateHandCalibrationMode(mode,true,plusWasDown);
    assert(mode==CalibrationMode::Rotation);
    plusWasDown=false;
    assert(updateHandCalibrationMode(CalibrationMode::None,true,plusWasDown)
        ==CalibrationMode::None);

    auto profile = selectHandCalibrationProfile(false, false,
        KharvoxWeaponKind::Shotgun, true);
    assert(profile.weaponSpecific && profile.handIndex == 1u);
    assert(profile.weaponIndex ==
        static_cast<std::size_t>(KharvoxWeaponKind::Shotgun));

    profile = selectHandCalibrationProfile(true, true,
        KharvoxWeaponKind::GaussCannon, true);
    assert(profile.weaponSpecific && profile.handIndex == 0u);

    profile = selectHandCalibrationProfile(true, false,
        KharvoxWeaponKind::Shotgun, false);
    assert(!profile.weaponSpecific && profile.handIndex == 0u);

    profile = selectHandCalibrationProfile(false, false,
        KharvoxWeaponKind::Unknown, true);
    assert(!profile.weaponSpecific);
    // With the same world-facing AIM direction, a canted LEFT GRIP should
    // still produce a straight-forward glove. Do not rotate controller
    // positions or the separately calibrated weapon-holding hand.
    HandAimQuaternion aligned{};
    const HandAimQuaternion identity{0.f,0.f,0.f,1.f};
    const HandAimQuaternion leftCant{0.f,.25881905f,0.f,.96592583f};
    assert(alignFreeHandGripToAim(leftCant,leftCant,identity,aligned));
    assert(std::fabs(aligned.x)<.0001f);
    assert(std::fabs(aligned.y)<.0001f);
    assert(std::fabs(aligned.z)<.0001f);
    assert(std::fabs(aligned.w-1.f)<.0001f);
    // A completed eye pair with a differently rotated saved grip must use
    // the controller-local delta, not replace its source-time orientation.
    const HandAimQuaternion saved{0.f,.5f,0.f,.8660254f}; // yaw 60
    assert(alignFreeHandGripToAim(saved,leftCant,identity,aligned));
    assert(std::fabs(aligned.y-.25881905f)<.0001f); // yaw 30
    assert(std::fabs(aligned.w-.96592583f)<.0001f);
    // Tracking loss: reject and leave the caller's saved orientation alone.
    const HandAimQuaternion invalid{0.f,0.f,0.f,0.f};
    const auto before=aligned;
    assert(!alignFreeHandGripToAim(saved,invalid,identity,aligned));
    assert(aligned.x==before.x&&aligned.y==before.y
        &&aligned.z==before.z&&aligned.w==before.w);
    return 0;
}
