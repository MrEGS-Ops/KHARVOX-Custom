#include "../src/openxr/PrimaryGripAnchorPolicy.h"
#include "../src/weapon/WeaponControllerPosePolicy.h"
#include <cassert>
#include <cmath>
#include <initializer_list>
struct Vec{float x,y,z;};
struct Quat{float x,y,z,w;};
static Vec rotate(Quat q,Vec v) {
    const Vec u{q.x,q.y,q.z};
    const Vec cross{u.y*v.z-u.z*v.y,u.z*v.x-u.x*v.z,
        u.x*v.y-u.y*v.x};
    const Vec second{u.y*cross.z-u.z*cross.y,
        u.z*cross.x-u.x*cross.z,u.x*cross.y-u.y*cross.x};
    return {v.x+2.f*(q.w*cross.x+second.x),
        v.y+2.f*(q.w*cross.y+second.y),
        v.z+2.f*(q.w*cross.z+second.z)};
}
static bool near(Vec a,Vec b){
    return std::abs(a.x-b.x)<.0002f&&std::abs(a.y-b.y)<.0002f
        &&std::abs(a.z-b.z)<.0002f;
}
int main(){
    // The separate OpenXR AIM and GRIP orientations may diverge by ~45°.
    // The held gun must take its pose from the SAME GRIP as its glove.
    struct TrackedPose {bool valid;int source;};
    const TrackedPose aim{true,1},grip{true,2},lostGrip{false,3};
    assert(&kharvox::weaponpose::trackedPrimaryWeaponPose(aim,grip)==&grip);
    assert(&kharvox::weaponpose::trackedPrimaryWeaponPose(aim,lostGrip)==&aim);
    // Visible hands: no extra legacy yaw/pitch/roll offset in the gun frame.
    assert(kharvox::weaponpose::weaponEulerOffsetDegrees(25.f,true)==0.f);
    assert(kharvox::weaponpose::weaponEulerOffsetDegrees(-35.f,true)==0.f);
    assert(kharvox::weaponpose::weaponEulerOffsetDegrees(25.f,false)==25.f);
    // Primary fitting is XY-only; support grips are still full-3D.
    assert(kharvox::weaponpose::weaponHandCalibrationZ(true,-.45f,.09f)==.09f);
    assert(kharvox::weaponpose::weaponHandCalibrationZ(false,-.45f,.09f)==-.45f);
    const Quat identity{0,0,0,1};
    const Vec aim{.2f,.4f,-.1f},grip{.22f,.35f,-.11f};
    const Vec gauss{-0.09830919f,.17283046f-.08f,.52416211f};
    const Vec chain{-0.11940779f,.27708763f-.08f,.67574835f};
    // Unmodified KHARVOX weapon pivots on every normal gun.
    assert(near(kharvox::primarygrip::weaponAnchor(
        aim,grip,identity,gauss,false,rotate),aim));
    for(const Vec target:{gauss,chain}){
        const auto gun=kharvox::primarygrip::weaponAnchor(
            aim,grip,identity,target,true,rotate);
        // Exactly the desired location on the gun meets the physical glove.
        assert(near(kharvox::primarygrip::lockedHand(
            gun,identity,target,rotate),grip));
        const auto local=kharvox::primarygrip::localGrip(
            gun,grip,identity,rotate);
        assert(near(local,target));
    }
    // Hold the same local attachment even when support hand rolls the gun.
    const Quat yaw90{0,.70710678118f,0,.70710678118f};
    const auto anchored=kharvox::primarygrip::weaponAnchor(
        aim,grip,yaw90,gauss,true,rotate);
    assert(near(kharvox::primarygrip::lockedHand(
        anchored,yaw90,gauss,rotate),grip));
    const auto local=kharvox::primarygrip::localGrip(
        anchored,grip,yaw90,rotate);
    assert(near(local,gauss));
    // Explicit user calibration on an ordinary gun ALSO relocates the gun
    // pivot to the saved local point, for either physical holster hand.
    for(const Vec physical:{Vec{.30f,.40f,-.22f},Vec{-.30f,.40f,-.22f}}){
        const Vec savedLocal{.10f,-.03f,-.16f};
        const auto newPivot=kharvox::primarygrip::weaponAnchor(
            aim,physical,yaw90,savedLocal,true,rotate);
        assert(near(kharvox::primarygrip::lockedHand(
            newPivot,yaw90,savedLocal,rotate),physical));
    }
    // Normal gun: model hand stays at its original XR grip on first latch
    // and moves as a rigid gun-local attachment as the gun rotates.
    const auto normalLocal=kharvox::primarygrip::localGrip(
        aim,grip,identity,rotate);
    assert(near(kharvox::primarygrip::lockedHand(
        aim,identity,normalLocal,rotate),grip));
    const auto turned=kharvox::primarygrip::lockedHand(
        aim,yaw90,normalLocal,rotate);
    assert(near(turned,Vec{aim.x+normalLocal.z,aim.y+normalLocal.y,aim.z-normalLocal.x}));
    return 0;
}
