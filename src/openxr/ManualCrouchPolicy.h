#pragma once
#include <cmath>

namespace kharvox {
// Right-stick DOWN remains available only as the legacy weapon-wheel fallback,
// never manual crouch. R3 CLICK remains independent for Hand Focus/menu.
inline bool vrRightStickDownGesture(float x,float normalizedY) noexcept {
    return std::isfinite(x)&&std::isfinite(normalizedY)
        &&std::abs(x)<.35f&&normalizedY<-.75f;
}

// Gunslinger owns the weapon wheel even if No Weapon Wheel is enabled.
// The right-stick-down fallback must not bypass Gunslinger and reopen it.
inline constexpr bool vrScopedWeaponSelectDown(
    bool gameplay,bool noWeaponWheel,bool gunslingerWheelEnabled,
    bool behindHeadGestureRequested,bool legacySelectionPressed) noexcept {
    return gameplay&&!noWeaponWheel
        &&(gunslingerWheelEnabled?behindHeadGestureRequested:legacySelectionPressed);
}

// Physical Crouch toggles only the headset-height input. The face A button
// remains an independent manual crouch toggle in either physical mode.
inline constexpr bool vrCombinedCrouchDesired(bool manual,bool physical) noexcept {
    return manual||physical;
}
// DOOM handles crouch using rising native B button edges; pulse transitions
// with an OFF gap, including when A is pressed rapidly or while physically
// crouched. The queue coalesces cancelled transitions.
inline constexpr bool vrCrouchPulseReady(
    bool queued,long long now,long long lastPulseUntil) noexcept {
    return queued&&now>=lastPulseUntil+20000000LL;
}
} // namespace kharvox
