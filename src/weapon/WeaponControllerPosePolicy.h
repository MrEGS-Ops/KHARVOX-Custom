#pragma once

namespace kharvox::weaponpose {

// AIM is for controller rays/UI; GRIP is the physical hand reference.
// Use exactly one frame for both the held gun and its primary glove.
template<class Controller>
inline const Controller& trackedPrimaryWeaponPose(
    const Controller& aim, const Controller& grip) noexcept {
    return grip.valid ? grip : aim;
}

// 6DoF tracked guns with a visible glove must not inherit the original
// 25-degree native model-yaw tuning (or arbitrary legacy pitch/roll).
inline constexpr float weaponEulerOffsetDegrees(
    float legacyDegrees, bool controllerAlignedHands) noexcept {
    return controllerAlignedHands ? 0.f : legacyDegrees;
}

// The user's primary glove is fitted LEFT/RIGHT and UP/DOWN on the weapon.
// Forward depth (Z) and its rotation are fixed by the real controller GRIP.
// The support-hand calibration remains fully 3D.
inline constexpr float weaponHandCalibrationZ(
    bool primaryHand, float measuredZ, float existingZ) noexcept {
    return primaryHand ? existingZ : measuredZ;
}

} // namespace kharvox::weaponpose
