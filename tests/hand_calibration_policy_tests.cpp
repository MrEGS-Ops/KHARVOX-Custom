#include "../src/hands/HandCalibrationPolicy.h"
#include "../src/hands/HandAimAlignmentPolicy.h"
#include "../src/hands/HandModelReflectionPolicy.h"

#include <cassert>

int main() {
    using namespace kharvox::hands;

    // A true left/right mirror flips the wrist's yaw/roll but preserves
    // the forward axis and produces a determinant +1 rotation. A fixed 180°
    // roll does NOT mirror an arbitrary 3D wrist and reverses the left palm.
    struct Mat3 {float m[9];};
    auto matmul=[](const Mat3&a,const Mat3&b){
        Mat3 c{};
        for(int row=0;row<3;++row)
            for(int col=0;col<3;++col)
                for(int k=0;k<3;++k)
                    c.m[row*3+col]+=a.m[row*3+k]*b.m[k*3+col];
        return c;
    };
    const Mat3 mirrorX{{-1,0,0,0,1,0,0,0,1}};
    const Mat3 yawRight{{0,-1,0,1,0,0,0,0,1}};
    const Mat3 yawMirrored=handModelReferenceRotation(
        true,yawRight,mirrorX,matmul);
    const Mat3 yawUnmodified=handModelReferenceRotation(
        false,yawRight,mirrorX,matmul);
    const Mat3 expectedLeft{{0,1,0,-1,0,0,0,0,1}};
    for(int i=0;i<9;++i){
        assert(yawMirrored.m[i]==expectedLeft.m[i]);
        assert(yawUnmodified.m[i]==yawRight.m[i]);
    }
    // Mirroring twice returns the original. The mirrored orientation
    // is shared by the physical left Fist and left GunHolding asset.
    const Mat3 doubleMirrored=handModelReferenceRotation(
        true,yawMirrored,mirrorX,matmul);
    for(int i=0;i<9;++i)assert(doubleMirrored.m[i]==yawRight.m[i]);

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
