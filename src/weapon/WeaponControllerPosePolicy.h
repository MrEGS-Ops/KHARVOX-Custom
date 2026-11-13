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

// Primary glove is anchored to physical controller GRIP, not the gun.
// Grip-held joystick fine-tune translates ONLY the GUN in X/Y/Z, never
// the primary glove or its controller pose. No gun rotation is introduced.
template<class Offset>
struct PrimaryHandAnchorPlan {
    Offset gunTranslationLocal;   // calibrated Y/Z plus gun-only XYZ fine-tune
    Offset handAttachmentLocal;   // legacy reference, glove uses physical GRIP
};
template<class Offset>
inline PrimaryHandAnchorPlan<Offset> primaryHandAnchorPlan(
    const Offset& requestedHand,const Offset& referenceHand,
    const Offset& fineTune=Offset{}) noexcept {
    // Saved position moves the gun in Y/Z; joystick offsets apply to
    // the gun in all three axes. The real tracked glove is unchanged.
    return {
        Offset{fineTune.x,requestedHand.y-referenceHand.y+fineTune.y,
            requestedHand.z-referenceHand.z+fineTune.z},
        referenceHand
    };
}

} // namespace kharvox::weaponpose
