#pragma once

namespace kharvox::weaponpose {

// The system controller pointer is an OpenXR AIM ray, NOT its GRIP pose.
// All primary weapon motion uses that AIM frame. If AIM tracking drops out,
// the runtime holds the last valid weapon pose rather than silently tilting
// the gun to a different GRIP frame.
template<class Controller>
inline const Controller& trackedPrimaryWeaponPose(
    const Controller& aim, const Controller&) noexcept {
    return aim;
}

// During normal single-hand gameplay, keep the physical AIM ray. When a
// valid support Grip has actually latched, the existing two-hand solver MUST
// control weapon orientation. Never discard its result after solving.
// Calibration/menu states are excluded from this gameplay-only restoration.
inline constexpr bool twoHandGameplaySteeringActive(
    bool enabled,bool latched,bool sameWeapon,bool supportGripHeld,
    bool supportTracked,bool holstered,bool calibrating) noexcept {
    return enabled&&latched&&sameWeapon&&supportGripHeld&&supportTracked
        &&!holstered&&!calibrating;
}

// Single-hand laser fallback, not a blanket override of solved two-hand aim.
template<class Quaternion>
inline const Quaternion& keepWeaponOnPrimaryLaser(
    const Quaternion& primaryAim, const Quaternion&) noexcept {
    return primaryAim;
}

// Do not translate the barrel's visual aim ray when the owner adjusts the
// glove. Grip calibration moves ONLY the glove, not the gun itself.
template<class Position>
inline const Position& keepGunOnPrimaryLaser(
    const Position& primaryAim, const Position&) noexcept {
    return primaryAim;
}

// Route native aim to the already-computed solver result only while
// normal gameplay has a confirmed active support grip.
template<class Quaternion>
inline const Quaternion& gameplayWeaponAim(
    const Quaternion& primaryAim,const Quaternion& twoHandSolved,
    bool twoHandSteeringActive) noexcept {
    return twoHandSteeringActive?twoHandSolved:primaryAim;
}

// Rebase the PRIMARY visual glove's position onto the *physical weapon-hand*
// controller GRIP origin when fine-tune is first activated. Preserve the
// previous normal weapon translation exactly: the difference becomes a
// hand-only fine-tune offset instead of moving the native gun.
template<class Offset>
struct GripReferencedFineTune {
    Offset handPosition;
    Offset handOnlyFineTune;
};
template<class Offset>
inline GripReferencedFineTune<Offset> rebaseFineTuneToPhysicalGrip(
    const Offset& trackedGripLocal,const Offset& savedHand,
    const Offset& savedFineTune) noexcept {
    const Offset normalBase{
        savedHand.x-savedFineTune.x,
        savedHand.y-savedFineTune.y,
        savedHand.z-savedFineTune.z};
    return {trackedGripLocal,{
        trackedGripLocal.x-normalBase.x,
        trackedGripLocal.y-normalBase.y,
        trackedGripLocal.z-normalBase.z}};
}

// A 1cm joystick nudge is a displacement in the physical weapon controller's
// GRIP axes, not the arbitrarily rotated gun-local frame. Convert to the
// local gun space used by saved visual hand positions and fine-tune offsets.
template<class Offset,class Quaternion,class Rotate>
inline Offset gripFineTuneStepInWeaponLocal(
    const Offset& stepGripLocal,const Quaternion& gripTrackingRotation,
    const Quaternion& inverseGunTrackingRotation,Rotate rotate) noexcept {
    return rotate(inverseGunTrackingRotation,
        rotate(gripTrackingRotation,stepGripLocal));
}

// The legacy 25-degree gun yaw (and old pitch/roll overrides) must not be
// layered onto the native weapon while laser-locked hands are enabled.
inline constexpr float weaponEulerOffsetDegrees(
    float legacyDegrees, bool controllerAlignedHands) noexcept {
    return controllerAlignedHands ? 0.f : legacyDegrees;
}

// Primary hand *visual* fitting can adjust only vertical Y and along-barrel Z.
// Lock lateral X. Does not constrain the visual glove's local quaternion:
// it can be oriented any way the owner chooses and locked to the gun.
template<class Offset>
inline Offset primaryHandVisualPlacement(
    Offset measured, Offset existing) noexcept {
    measured.x=existing.x;
    return measured;
}

// Native gun follows only the ordinary Y/Z calibration adjustment.
// 1cm Grip-held nudges alter the glove's *attachment* (weapon-hand anchor),
// never the gun. Sideways X belongs ONLY to hand-only fitting, and neither
// offset ever changes the OpenXR AIM quaternion or barrel direction.
template<class Offset>
struct PrimaryHandAnchorPlan {
    Offset gunTranslationLocal;   // normal calibrated Y/Z only, never fine-tune
    Offset handAttachmentLocal;   // native reference grip + fine-tune
};
template<class Offset>
inline PrimaryHandAnchorPlan<Offset> primaryHandAnchorPlan(
    const Offset& requestedHand,const Offset& referenceHand,
    const Offset& fineTune=Offset{}) noexcept {
    // Even legacy saves with a nonzero X must leave the GUN's lateral
    // translation locked. Preserve the X adjustment on the glove/anchor.
    return {
        Offset{0.f,requestedHand.y-referenceHand.y-fineTune.y,
            requestedHand.z-referenceHand.z-fineTune.z},
        Offset{requestedHand.x,referenceHand.y+fineTune.y,
            referenceHand.z+fineTune.z}
    };
}

} // namespace kharvox::weaponpose
