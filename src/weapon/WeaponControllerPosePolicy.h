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

// The weapon and its PRIMARY hand are one rigid pose group. Calibration
// translates the group's ANCHOR along weapon-local Y (up/down) and Z
// (forward/back), but NEVER sideways (X). This deliberately allows the
// gun's centreline to be OFFSET from the controller laser while staying
// exactly PARALLEL to it. No rotation, pitch/yaw/roll or steering is added.
// The primary glove's independent local quaternion is retained.
template<class Offset>
struct PrimaryHandAnchorPlan {
    Offset gunTranslationLocal;  // X locked to zero; Y/Z adjustable
    Offset handAttachmentLocal;  // original grip on the moving gun
};
template<class Offset>
inline PrimaryHandAnchorPlan<Offset> primaryHandAnchorPlan(
    const Offset& requestedHand, const Offset& referenceHand) noexcept {
    return {
        Offset{0.f,requestedHand.y-referenceHand.y,
            requestedHand.z-referenceHand.z},
        referenceHand
    };
}

} // namespace kharvox::weaponpose
