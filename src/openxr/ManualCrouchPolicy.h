#pragma once

namespace kharvox {

// Right-stick DOWN is an axial gesture, not a stick CLICK, so RS Use and
// Hand Focus remain independent. Gunslinger's behind-head wheel owns weapon
// selection and frees this direction. Legacy remap uses the same old stick
// fallback; never use it while Physical Crouch owns HMD height changes.
inline constexpr bool manualStickCrouchAllowed(
    bool physicalCrouch,bool gunslingerWheel,bool legacyWheelRemap) noexcept {
    return !physicalCrouch&&(gunslingerWheel||legacyWheelRemap);
}

// Gunslinger owns the weapon wheel even if No Weapon Wheel is enabled.
// Never let a stick-down crouch gesture re-enter the retired weapon-wheel
// route. A gesture opens the wheel ONLY if selected and allowed.
inline constexpr bool vrScopedWeaponSelectDown(
    bool gameplay,bool noWeaponWheel,bool gunslingerWheelEnabled,
    bool behindHeadGestureRequested,bool legacySelectionPressed) noexcept {
    return gameplay&&!noWeaponWheel
        &&(gunslingerWheelEnabled?behindHeadGestureRequested:legacySelectionPressed);
}

// DOOM treats crouch as a toggle on its native face-button rising edge.
// For the normal Gunslinger input path, send SHORT native crouch pulses on
// both stick-toggle transitions rather than holding its button continuously.
// The retired legacy remap path keeps its proven gamepad mapping.
inline constexpr bool manualStickCrouchPulseActive(
    bool gameplay,bool physicalCrouch,bool legacyWheelRemap,
    long long now,long long pulseUntil) noexcept {
    return gameplay&&!physicalCrouch&&!legacyWheelRemap
        &&pulseUntil>now;
}

} // namespace kharvox
