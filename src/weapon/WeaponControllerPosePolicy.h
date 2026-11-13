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

// The primary weapon barrel always retains the physical AIM orientation.
// The support hand may attach visually but must never steer the barrel.
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
