#pragma once

namespace kharvox::weaponpose {

// The controller's OpenXR AIM pose is the visual laser ray. The barrel
// centreline follows that exact pose, never the differently tilted GRIP pose.
// The physical GRIP remains available independently for glove visuals.
template<class Controller>
inline const Controller& trackedPrimaryWeaponPose(
    const Controller& aim, const Controller& grip) noexcept {
    return aim.valid ? aim : grip;
}

// A second hand must not steer the gun off its physical primary AIM ray.
// The support hand may attach visually at its calibrated grip point.
template<class Quaternion>
inline const Quaternion& keepWeaponOnPrimaryLaser(
    const Quaternion& primaryAim, const Quaternion&) noexcept {
    return primaryAim;
}

// An older generic 25-degree yaw adds an extra rotation even after the
// native gun is driven by OpenXR AIM; suppress legacy tuning for visible
// hand/controller-aimed guns without altering hand or depth rendering.
inline constexpr float weaponEulerOffsetDegrees(
    float legacyDegrees, bool controllerAlignedHands) noexcept {
    return controllerAlignedHands ? 0.f : legacyDegrees;
}

// Primary glove calibration is limited to X/Y placement around the gun.
// Preserve gun-local Z so the controller laser and barrel don't move.
// Support hand remains 3D adjustable.
inline constexpr float weaponHandCalibrationZ(
    bool primaryHand, float measuredZ, float existingZ) noexcept {
    return primaryHand ? existingZ : measuredZ;
}

} // namespace kharvox::weaponpose
