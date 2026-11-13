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
    // Physical controller laser is OpenXR AIM, never the tilted GRIP.
    // Grip may differ by 45 degrees but native gun must use the AIM pose.
    struct TrackedPose {bool valid;int source;};
    const TrackedPose trackedAim{true,1},trackedGrip{true,2},lostAim{false,3};
    assert(&kharvox::weaponpose::trackedPrimaryWeaponPose(
        trackedAim,trackedGrip)==&trackedAim);
    assert(&kharvox::weaponpose::trackedPrimaryWeaponPose(
        lostAim,trackedGrip)==&lostAim);
    const Quat laser{0,0,0,1},supportTilt{0,.38268343f,0,.92387953f};
    assert(&kharvox::weaponpose::keepWeaponOnPrimaryLaser(laser,supportTilt)==&laser);
    const Vec laserOrigin{0,0,0},glovePreview{1,3,-.3f};
    assert(&kharvox::weaponpose::keepGunOnPrimaryLaser(laserOrigin,glovePreview)==&laserOrigin);
    // Native extra 25-degree yaw MUST NOT tilt the bore off the laser.
    assert(kharvox::weaponpose::weaponEulerOffsetDegrees(25.f,true)==0.f);
    assert(kharvox::weaponpose::weaponEulerOffsetDegrees(-35.f,true)==0.f);
    assert(kharvox::weaponpose::weaponEulerOffsetDegrees(25.f,false)==25.f);
    // Owner can position the visible glove up/down (Y) or along the bore
    // (Z) and rotate it; its lateral X remains fixed to gun grip profile.
    const Vec original{.12f,.2f,-.1f},measured{.55f,-.08f,-.55f};
    const auto fitted=kharvox::weaponpose::primaryHandVisualPlacement(
        measured,original);
    assert(near(fitted,Vec{original.x,measured.y,measured.z}));
    const Quat identity{0,0,0,1};
    // Gun and primary glove MUST move together in the gun's Y/Z axes.
    // The controller laser fixes orientation, not the gun's world position.
    // The barrel may be vertically offset, but must remain PARALLEL.
    const Vec referenceHand{.06f,.04f,.12f};
    const Vec adjustedHand{.06f,-.09f,-.23f};
    const auto anchorPlan=kharvox::weaponpose::primaryHandAnchorPlan(
        adjustedHand,referenceHand);
    assert(near(anchorPlan.gunTranslationLocal,
        Vec{0,adjustedHand.y-referenceHand.y,
            adjustedHand.z-referenceHand.z}));
    assert(near(anchorPlan.handAttachmentLocal,referenceHand));
    const Vec laserWorld{.35f,.61f,-.25f};
    for(const Quat orientation:{identity,
        Quat{0,.38268343f,0,.92387953f}}){
        const auto gunDelta=rotate(orientation,anchorPlan.gunTranslationLocal);
        const Vec newGunAnchor{laserWorld.x+gunDelta.x,
            laserWorld.y+gunDelta.y,laserWorld.z+gunDelta.z};
        const auto handDelta=rotate(orientation,anchorPlan.handAttachmentLocal);
        const Vec gloveWorld{newGunAnchor.x+handDelta.x,
            newGunAnchor.y+handDelta.y,newGunAnchor.z+handDelta.z};
        const auto requestedDelta=rotate(orientation,adjustedHand);
        assert(near(gloveWorld,Vec{laserWorld.x+requestedDelta.x,
            laserWorld.y+requestedDelta.y,laserWorld.z+requestedDelta.z}));
        // Native gun receives *only translation*: unchanged yaw/pitch/roll.
        // Local right/lateral displacement stays exactly zero, even when
        // the controller is rotated 45 degrees in world space.
        const Vec right=rotate(orientation,Vec{1,0,0});
        const float lateral=gunDelta.x*right.x+gunDelta.y*right.y+
            gunDelta.z*right.z;
        assert(std::abs(lateral)<.0002f);
        // Vertical displacement is nonzero, intentionally allowing a
        // centreline OFFSET from (but parallel to) the laser pointer.
        const Vec up=rotate(orientation,Vec{0,1,0});
        const float vertical=gunDelta.x*up.x+gunDelta.y*up.y+gunDelta.z*up.z;
        assert(std::abs(vertical-
            (adjustedHand.y-referenceHand.y))<.0002f);
    }
    // 1cm Grip-held fine tune is ONLY glove + attachment anchor.
    // The gun must not change position, lateral placement or orientation
    // even while the hand moves in X, Y, and Z. Verify at two AIM angles.
    const Vec tune{+.01f,-.02f,-.03f};
    const Vec fineTunedHand{
        referenceHand.x+tune.x,referenceHand.y+tune.y,
        referenceHand.z+tune.z};
    const auto finePlan=kharvox::weaponpose::primaryHandAnchorPlan(
        fineTunedHand,referenceHand,tune);
    assert(near(finePlan.gunTranslationLocal,Vec{0,0,0}));
    assert(near(finePlan.handAttachmentLocal,fineTunedHand));
    for(const Quat pose:{identity,Quat{0,.38268343f,0,.92387953f}}){
        const auto gunDelta=rotate(pose,finePlan.gunTranslationLocal);
        const auto handDelta=rotate(pose,finePlan.handAttachmentLocal);
        const auto expected=rotate(pose,fineTunedHand);
        assert(near(gunDelta,Vec{0,0,0}));
        assert(near(handDelta,expected));
        const auto& unchanged=kharvox::weaponpose::keepWeaponOnPrimaryLaser(
            pose,laser);
        assert(&unchanged==&pose);
    }
    // Normal calibrated Y/Z and hand-only XYZ fine-tune can coexist.
    const Vec normalAndFine{referenceHand.x+.01f,
        referenceHand.y+.08f-.02f,referenceHand.z-.12f-.03f};
    const auto combined=kharvox::weaponpose::primaryHandAnchorPlan(
        normalAndFine,referenceHand,tune);
    assert(near(combined.gunTranslationLocal,Vec{0,+.08f,-.12f}));
    assert(near(combined.handAttachmentLocal,Vec{
        referenceHand.x+.01f,referenceHand.y-.02f,referenceHand.z-.03f}));
    for(const Quat pose:{identity,Quat{0,.38268343f,0,.92387953f}}){
        const auto g=rotate(pose,combined.gunTranslationLocal);
        const auto h=rotate(pose,combined.handAttachmentLocal);
        const auto expected=rotate(pose,normalAndFine);
        assert(near(Vec{g.x+h.x,g.y+h.y,g.z+h.z},expected));
    }
    const auto zeroPlan=kharvox::weaponpose::primaryHandAnchorPlan(
        referenceHand,referenceHand);
    assert(near(zeroPlan.gunTranslationLocal,Vec{0,0,0}));
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
