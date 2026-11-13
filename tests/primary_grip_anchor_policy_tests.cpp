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
static Vec add(Vec a,Vec b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
static Vec subtract(Vec a,Vec b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
static Quat conjugate(Quat q){return {-q.x,-q.y,-q.z,q.w};}
static Quat multiplyQuat(Quat a,Quat b){
    return {
        a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y,
        a.w*b.y-a.x*b.z+a.y*b.w+a.z*b.x,
        a.w*b.z+a.x*b.y-a.y*b.x+a.z*b.w,
        a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z
    };
}
static bool sameOrientation(Quat a,Quat b){
    const float dot=a.x*b.x+a.y*b.y+a.z*b.z+a.w*b.w;
    return std::abs(std::abs(dot)-1.f)<.0002f;
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
    // With a second hand LATCHED in NORMAL gameplay, the glove MUST use
    // the same solved gun quaternion + saved local wrist rotation. Otherwise
    // it appears that the barrel rotates independently inside the glove.
    // This is VISUAL ONLY; rotating the physical controller can still
    // contribute to normal gameplay steering and does not change solver.
    const Quat rotatedPrimaryController{.38268343f,0,0,.92387953f};
    const Quat changedPrimaryController{0,0,.25881905f,.96592583f};
    const Quat savedGloveLocal{0,0,.38268343f,.92387953f};
    const auto& twoHandGloveFrame=kharvox::weaponpose::
        primaryGloveVisualFrame(rotatedPrimaryController,supportTilt,
            true,true);
    assert(&twoHandGloveFrame==&supportTilt);
    const auto visibleTwoHandGlove=kharvox::weaponpose::
        weaponLockedPrimaryGloveOrientation(
            twoHandGloveFrame,savedGloveLocal,multiplyQuat);
    // Relative glove-to-weapon angle stays the saved calibration.
    assert(sameOrientation(
        multiplyQuat(conjugate(supportTilt),visibleTwoHandGlove),
        savedGloveLocal));
    const auto& stillWeaponFrame=kharvox::weaponpose::
        primaryGloveVisualFrame(changedPrimaryController,supportTilt,
            true,true);
    const auto stillWeaponGlove=kharvox::weaponpose::
        weaponLockedPrimaryGloveOrientation(
            stillWeaponFrame,savedGloveLocal,multiplyQuat);
    assert(sameOrientation(stillWeaponGlove,visibleTwoHandGlove));
    // Once the secondary Grip is released, revert to normal primary
    // controller visual orientation; keep calibration paths unchanged.
    const auto& oneHandFrame=kharvox::weaponpose::
        primaryGloveVisualFrame(rotatedPrimaryController,supportTilt,
            true,false);
    assert(&oneHandFrame==&rotatedPrimaryController);
    const auto visibleOneHandGlove=kharvox::weaponpose::
        weaponLockedPrimaryGloveOrientation(
            oneHandFrame,savedGloveLocal,multiplyQuat);
    assert(sameOrientation(
        multiplyQuat(conjugate(rotatedPrimaryController),
            visibleOneHandGlove),savedGloveLocal));
    const auto& invisibleHandsFrame=kharvox::weaponpose::
        primaryGloveVisualFrame(rotatedPrimaryController,supportTilt,
            false,true);
    assert(&invisibleHandsFrame==&supportTilt);
    const auto& noSteerFrame=kharvox::weaponpose::
        primaryGloveVisualFrame(rotatedPrimaryController,
            rotatedPrimaryController,true,false);
    assert(&noSteerFrame==&rotatedPrimaryController);

    // The previous frozen wall-pushback locator was removed after a user-
    // observed persistent pose offset. Native animated attachment remains
    // active and user factory profiles are NOT rewritten here.
    // The mode handoff blends POSITION ONLY during normal two-hand play.
    // Factory profiles remain the exact endpoints; no jump at t=0 and no
    // extra interpolation once the 320ms transition has completed.
    const Vec supportNormal{-.21f,.02f,-.19f};
    const Vec supportTurret{-.08f,-.07f,-.39f};
    const float t0=kharvox::weaponpose::heavyGripTransitionEase(0);
    const float tHalf=kharvox::weaponpose::heavyGripTransitionEase(160);
    const float tEnd=kharvox::weaponpose::heavyGripTransitionEase(320);
    const float tBeyond=kharvox::weaponpose::heavyGripTransitionEase(1000);
    assert(t0==0.f&&std::abs(tHalf-.5f)<.00001f);
    assert(tEnd==1.f&&tBeyond==1.f);
    assert(near(kharvox::weaponpose::heavyGripBlendVector(
        supportNormal,supportTurret,t0),supportNormal));
    assert(near(kharvox::weaponpose::heavyGripBlendVector(
        supportNormal,supportTurret,tEnd),supportTurret));
    const auto intermediate=kharvox::weaponpose::heavyGripBlendVector(
        supportNormal,supportTurret,tHalf);
    assert(!near(intermediate,supportNormal)
        &&!near(intermediate,supportTurret));
    const auto back=kharvox::weaponpose::heavyGripBlendVector(
        supportTurret,supportNormal,tHalf);
    assert(near(back,intermediate)); // reverse transition is symmetrical
    const Vec gunNormal{.0f,.01f,-.11f};
    const Vec gunTurret{.02f,-.08f,-.62f};
    assert(near(kharvox::weaponpose::heavyGripBlendVector(
        gunNormal,gunTurret,t0),gunNormal));
    assert(near(kharvox::weaponpose::heavyGripBlendVector(
        gunNormal,gunTurret,tEnd),gunTurret));

    // Support hand live preview and SAVED playback use the exact SAME
    // pre-fine-tune weapon AIM reference. Its physical GRIP must stay in
    // the same place when entering calibration, and after a gun fine-tune
    // offset is applied. Both physical weapon-hand sides are supported.
    const Quat identityGrip{0.f,0.f,0.f,1.f};
    const Quat yawGrip{0.f,.38268343f,0.f,.92387953f};
    for(const Quat weaponOrientation:{identityGrip,yawGrip}){
        const Vec actualSupportGrip{-.30f,.45f,-.40f};
        const Vec actualFreeAim{-.30f,.45f,-.54f};
        const Vec unadjustedAimAnchor{.11f,.55f,-.05f};
        const Vec gunTranslationLocal{.04f,-.06f,-.12f};
        const Vec shiftedGunOrigin=add(unadjustedAimAnchor,
            rotate(weaponOrientation,gunTranslationLocal));
        const Vec measuredLocal=rotate(conjugate(weaponOrientation),
            subtract(actualSupportGrip,unadjustedAimAnchor));
        const auto preview=kharvox::weaponpose::
            supportGloveAtCalibrationReference(unadjustedAimAnchor,
                weaponOrientation,measuredLocal,add,rotate);
        assert(near(preview,actualSupportGrip));
        assert(!near(preview,actualFreeAim)); // do not capture AIM offset
        // 'SAVE' persists measuredLocal. A later gameplay draw/Grip
        // computes the same physical location, NOT shiftedGunOrigin +
        // the saved support offset (the old double-translation bug).
        for(const bool leftPrimary:{false,true}){
            Vec stored=measuredLocal;
            if(leftPrimary)stored.x=-stored.x; // existing mirrored file
            Vec active=stored;
            if(leftPrimary)active.x=-active.x; // runtime mirrors back
            const auto reloaded=kharvox::weaponpose::
                supportGloveAtCalibrationReference(unadjustedAimAnchor,
                    weaponOrientation,active,add,rotate);
            assert(near(reloaded,actualSupportGrip));
            assert(!near(reloaded,add(shiftedGunOrigin,
                rotate(weaponOrientation,active))));
        }
    }

    // Quest/OpenXR AIM and GRIP action spaces do not occupy the same
    // physical position. The gun's BASE attachment and visible hand must
    // share GRIP, not use AIM for one and GRIP for the other. Fine-tune
    // remains a separate gun-only translation, never moving the glove.
    const Vec actualWeaponGrip{.21f,.52f,-.32f};
    const Vec displacedAimOrigin{.27f,.61f,-.40f};
    for(const bool showCustomHands:{false,true}){
        const auto& gunBase=kharvox::weaponpose::sharedPhysicalWeaponGripOrigin(
            actualWeaponGrip,displacedAimOrigin,showCustomHands);
        assert(near(gunBase,showCustomHands
            ?actualWeaponGrip:displacedAimOrigin));
        if(showCustomHands){
            assert(&gunBase==&actualWeaponGrip);
            const Vec savedFineTune{.01f,-.03f,-.02f};
            for(const Quat gunFrame:{Quat{0.f,0.f,0.f,1.f},
                    Quat{0.f,.38268343f,0.f,.92387953f}}){
                const Vec gunAfterFineTune=add(gunBase,
                    rotate(gunFrame,savedFineTune));
                assert(near(subtract(gunAfterFineTune,gunBase),
                    rotate(gunFrame,savedFineTune)));
                assert(near(gunBase,actualWeaponGrip));
                const auto& unchangedGlove=
                    kharvox::weaponpose::physicalPrimaryGlovePosition(
                        actualWeaponGrip,gunAfterFineTune);
                assert(&unchangedGlove==&actualWeaponGrip);
                assert(near(unchangedGlove,actualWeaponGrip));
            }
        }else assert(&gunBase==&displacedAimOrigin);
    }
    // Keep old support offsets measured from AIM even though the moving
    // gun now attaches at GRIP. The saved/reloaded support point is not
    // inadvertently re-based onto GRIP, preserving the prior capture.
    const Quat neutralAim{0.f,0.f,0.f,1.f};
    const Vec supportMeasuredLocal{-.17f,.04f,-.28f};
    const auto oldSupportPoint=kharvox::weaponpose::
        supportGloveAtCalibrationReference(displacedAimOrigin,
            neutralAim,supportMeasuredLocal,add,rotate);
    const auto stillSavedSupport=kharvox::weaponpose::
        supportGloveAtCalibrationReference(displacedAimOrigin,
            neutralAim,supportMeasuredLocal,add,rotate);
    assert(near(oldSupportPoint,stillSavedSupport));
    assert(!near(stillSavedSupport,add(actualWeaponGrip,
        supportMeasuredLocal)));

    // A physical primary controller is the sole authoritative attachment
    // anchor for glove AND two-hand rotation. Weapon model/root may be
    // translated and rotated independently without ever moving that anchor.
    struct ControllerPose {Vec position;Quat orientation;bool valid;};
    const ControllerPose physicalGrip{{.25f,.62f,-.18f},
        {0.f,0.f,0.f,1.f},true};
    const ControllerPose movedGunModel{{.15f,.75f,-.40f},
        {0.f,.38268343f,0.f,.92387953f},true};
    const auto& handAnchor=kharvox::weaponpose::
        controllerFixedPrimaryHandAnchor(physicalGrip,movedGunModel);
    assert(&handAnchor==&physicalGrip);
    assert(near(handAnchor.position,physicalGrip.position));
    const auto gunOriginAtAim=Vec{.20f,.66f,-.22f};
    const Quat aimOrientation{0.f,0.f,0.f,1.f};
    const Quat twoHandOrientation{0.f,.38268343f,0.f,.92387953f};
    const auto modelAtAim=kharvox::weaponpose::
        gunOriginAboutControllerHandAnchor(physicalGrip.position,
            gunOriginAtAim,aimOrientation,aimOrientation,
            subtract,add,rotate,conjugate);
    assert(near(modelAtAim,gunOriginAtAim));
    const auto modelTwoHand=kharvox::weaponpose::
        gunOriginAboutControllerHandAnchor(physicalGrip.position,
            gunOriginAtAim,aimOrientation,twoHandOrientation,
            subtract,add,rotate,conjugate);
    assert(!near(modelTwoHand,modelAtAim));
    const auto unrotatedGripToGun=subtract(gunOriginAtAim,
        physicalGrip.position);
    const auto expectedTwoHand=add(physicalGrip.position,
        rotate(twoHandOrientation,unrotatedGripToGun));
    assert(near(modelTwoHand,expectedTwoHand));
    assert(near(handAnchor.position,physicalGrip.position));
    // Rebased support offset retains the exact original acquisition point
    // before the second grip latches, despite changing the solver pivot.
    const Vec supportLocal{.10f,-.05f,-.32f};
    const auto controllerLocalSupport=kharvox::weaponpose::
        supportOffsetFromControllerAnchor(supportLocal,
            physicalGrip.position,gunOriginAtAim,aimOrientation,
            subtract,add,rotate,conjugate);
    assert(near(add(physicalGrip.position,
          rotate(aimOrientation,controllerLocalSupport)),
          add(gunOriginAtAim,rotate(aimOrientation,supportLocal))));
    // This invariant also holds for a rotated tracked controller.
    const auto yawGripToGun=rotate(twoHandOrientation,
        unrotatedGripToGun);
    const auto yawAimOrigin=add(physicalGrip.position,yawGripToGun);
    const auto yawSupport=kharvox::weaponpose::
        supportOffsetFromControllerAnchor(supportLocal,
            physicalGrip.position,yawAimOrigin,twoHandOrientation,
            subtract,add,rotate,conjugate);
    assert(near(add(physicalGrip.position,
        rotate(twoHandOrientation,yawSupport)),
        add(yawAimOrigin,rotate(twoHandOrientation,supportLocal))));
    // Fine-tune translates the weapon model, not the fixed hand anchor:
    // no nudge can affect its position, even while support steers the gun.
    const Vec centimeters{+.01f,-.02f,-.03f};
    for(const Quat gunRotation:{aimOrientation,twoHandOrientation}){
        const Vec gunOnlyNudge=rotate(gunRotation,centimeters);
        const Vec shiftedGun=add(modelTwoHand,gunOnlyNudge);
        assert(!near(shiftedGun,modelTwoHand));
        const auto& stillFixed=kharvox::weaponpose::
            controllerFixedPrimaryHandAnchor(physicalGrip,movedGunModel);
        assert(near(stillFixed.position,physicalGrip.position));
        assert(&stillFixed==&physicalGrip);
    }
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
