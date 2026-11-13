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

// Native gun and its primary glove are one rigid pose group. Ordinary
// free-controller calibration LOCKS local X via primaryHandVisualPlacement;
// ONLY Grip-held centimeter joystick fine-tune may change saved primary X.
// Once saved, that lateral offset travels with the gun and its grip anchor.
// The gun can move in X/Y/Z but remains PARALLEL to OpenXR AIM, with
// absolutely no position-derived pitch/yaw/roll or quaternion changes.
template<class Offset>
struct PrimaryHandAnchorPlan {
    Offset gunTranslationLocal;  // local X/Y/Z (X only from fine-tune)
    Offset handAttachmentLocal;  // original grip on the moving gun
};
template<class Offset>
inline PrimaryHandAnchorPlan<Offset> primaryHandAnchorPlan(
    const Offset& requestedHand, const Offset& referenceHand) noexcept {
    return {
        Offset{requestedHand.x-referenceHand.x,
            requestedHand.y-referenceHand.y,
            requestedHand.z-referenceHand.z},
        referenceHand
    };
}

} // namespace kharvox::weaponpose
