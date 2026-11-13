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
    // Normal gameplay two-hand support Grip must steer the native weapon.
    // Former regression solved to supportTilt then unconditionally replaced
    // it with laser whenever custom hand models were visible.
    const auto& single=kharvox::weaponpose::gameplayWeaponAim(
        laser,supportTilt,false);
    const auto& twoHand=kharvox::weaponpose::gameplayWeaponAim(
        laser,supportTilt,true);
    assert(&single==&laser);
    assert(&twoHand==&supportTilt);
    using kharvox::weaponpose::twoHandGameplaySteeringActive;
    assert(twoHandGameplaySteeringActive(
        true,true,true,true,true,false,false));
    // Release grip, lose tracking or change guns: single-hand AIM fallback.
    assert(!twoHandGameplaySteeringActive(
        true,true,true,false,true,false,false));
    assert(!twoHandGameplaySteeringActive(
        true,true,true,true,false,false,false));
    assert(!twoHandGameplaySteeringActive(
        true,true,false,true,true,false,false));
    assert(!twoHandGameplaySteeringActive(
        true,false,true,true,true,false,false));
    assert(!twoHandGameplaySteeringActive(
        false,true,true,true,true,false,false));
    assert(!twoHandGameplaySteeringActive(
        true,true,true,true,true,true,false));
    // Keep the existing calibration modes unaffected.
    assert(!twoHandGameplaySteeringActive(
        true,true,true,true,true,false,true));
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
    // Baseline calibrated Y/Z translates only the gun. Its visual glove
    // is always aligned to physical controller GRIP, never the weapon.
    const Vec referenceHand{.06f,.04f,.12f};
    const Vec adjustedHand{.06f,-.09f,-.23f};
    const auto anchorPlan=kharvox::weaponpose::primaryHandAnchorPlan(
        adjustedHand,referenceHand);
    assert(near(anchorPlan.gunTranslationLocal,
        Vec{0,adjustedHand.y-referenceHand.y,
            adjustedHand.z-referenceHand.z}));
    assert(near(anchorPlan.handAttachmentLocal,referenceHand));
    const Vec laserWorld{.35f,.61f,-.25f};
    const Vec actualTrackedGrip{.21f,.54f,-.11f};
    for(const Quat orientation:{identity,
        Quat{0,.38268343f,0,.92387953f}}){
        const auto gunDelta=rotate(orientation,anchorPlan.gunTranslationLocal);
        const Vec newGunAnchor{laserWorld.x+gunDelta.x,
            laserWorld.y+gunDelta.y,laserWorld.z+gunDelta.z};
        assert(near(newGunAnchor,
            Vec{laserWorld.x+gunDelta.x,laserWorld.y+gunDelta.y,
                laserWorld.z+gunDelta.z}));
        const auto& glove=kharvox::weaponpose::physicalPrimaryGlovePosition(
            actualTrackedGrip,newGunAnchor);
        assert(&glove==&actualTrackedGrip);
        assert(near(glove,actualTrackedGrip));
        const Vec right=rotate(orientation,Vec{1,0,0});
        const float lateral=gunDelta.x*right.x+gunDelta.y*right.y+
            gunDelta.z*right.z;
        assert(std::abs(lateral)<.0002f);
    }

    // Grip-held fine-tune is a GUN-only translation, 1cm in arbitrary
    // X/Y/Z combinations. The primary controller glove is invariant.
    const Vec tune{+.01f,-.02f,-.03f};
    const auto finePlan=kharvox::weaponpose::primaryHandAnchorPlan(
        referenceHand,referenceHand,tune);
    assert(near(finePlan.gunTranslationLocal,tune));
    assert(near(finePlan.handAttachmentLocal,referenceHand));
    for(const Quat pose:{identity,Quat{0,.38268343f,0,.92387953f}}){
        const auto gunDelta=rotate(pose,finePlan.gunTranslationLocal);
        const auto expected=rotate(pose,tune);
        assert(near(gunDelta,expected));
        const Vec shiftedGun{laserWorld.x+gunDelta.x,
            laserWorld.y+gunDelta.y,laserWorld.z+gunDelta.z};
        const auto& unchangedGlove=
            kharvox::weaponpose::physicalPrimaryGlovePosition(
                actualTrackedGrip,shiftedGun);
        assert(&unchangedGlove==&actualTrackedGrip);
        assert(near(unchangedGlove,actualTrackedGrip));
        const auto& unchangedAim=kharvox::weaponpose::keepWeaponOnPrimaryLaser(
            pose,laser);
        assert(&unchangedAim==&pose);
    }
    // Independent ordinary gun calibration and gun-only XYZ fine-tune.
    const Vec normalHand{
        referenceHand.x,referenceHand.y+.08f,referenceHand.z-.12f};
    const auto combined=kharvox::weaponpose::primaryHandAnchorPlan(
        normalHand,referenceHand,tune);
    assert(near(combined.gunTranslationLocal,Vec{+.01f,+.06f,-.15f}));
    assert(near(combined.handAttachmentLocal,referenceHand));
    const Vec differentGrip{-.31f,.45f,-.20f};
    const auto& glove=kharvox::weaponpose::physicalPrimaryGlovePosition(
        differentGrip,combined.gunTranslationLocal);
    assert(&glove==&differentGrip);

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
