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
