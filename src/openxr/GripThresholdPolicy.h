#pragma once

#include <cmath>

namespace kharvox {

enum class GripControllerProfile {
    Default,
    ValveIndex
};

inline constexpr const char* gripInputComponent(
    GripControllerProfile profile) noexcept {
    // Valve Index exposes a dedicated force sensor. squeeze/value is the
    // appropriate normalized grip axis for Touch-style controllers, while
    // squeeze/force measures an intentional Index-controller squeeze.
    return profile == GripControllerProfile::ValveIndex
        ? "squeeze/force"
        : "squeeze/value";
}

struct GripThresholds {
    float press;
    float release;
};

inline constexpr GripThresholds gripThresholds(
    GripControllerProfile profile) noexcept {
    // Require a deliberate Index force-sensor squeeze, then retain the grip
    // down to the former global threshold to avoid flicker while holding it.
    return profile == GripControllerProfile::ValveIndex
        ? GripThresholds{0.70f, 0.55f}
        : GripThresholds{0.55f, 0.55f};
}

inline bool updateGripPressed(
    float value,
    bool wasPressed,
    GripControllerProfile profile) noexcept {
    if (!std::isfinite(value)) return false;
    const auto thresholds = gripThresholds(profile);
    return value > (wasPressed ? thresholds.release : thresholds.press);
}

// Controller-only safety latch. Cold startup unlocks automatically after a
// stable, neutral pair of verified physical controllers: there is no visible
// reconnect warning or thumbstick requirement on DOOM's startup/menu screen.
// After a previously armed controller is LOST, retain an explicit two-stick
// acknowledgement requirement even when tracking comes back. Emulated hands
// never satisfy the physical-profile and tracked-pose requirements.
// Aim poses are the physical controller tracking evidence required by the
// safety lock. Grip poses may be late or unavailable on a healthy controller
// (especially during DOOM's front menu); refusing the whole input stack in
// that case can leave the menu waiting for an apparent L3+R3 acknowledgement.
// Valid interaction profiles are still checked separately, and genuine AIM
// tracking loss after arming still requires deliberate reconnection.
inline constexpr bool physicalControllerAimPairTracked(
    bool leftAimValid,bool leftAimTracked,
    bool rightAimValid,bool rightAimTracked) noexcept {
    return leftAimValid&&leftAimTracked&&rightAimValid&&rightAimTracked;
}

struct ControllerOnlySafetyState {
    bool armed{},bothSticksAcknowledged{},reconnectAckRequired{};
    unsigned stableFrames{};
};
inline constexpr bool updateControllerOnlySafety(
    ControllerOnlySafetyState& state,
    bool bothPhysicalControllerProfiles,
    bool bothControllerPosesTracked,
    bool bothSticksPressed,
    bool bothSticksReleased,
    bool triggerAndGripNeutral) noexcept {
    if(!bothPhysicalControllerProfiles||!bothControllerPosesTracked){
        state.reconnectAckRequired=state.reconnectAckRequired||state.armed;
        state.armed=false;
        state.bothSticksAcknowledged=false;
        state.stableFrames=0;
        return false;
    }
    if(state.armed)return true;
    if(state.stableFrames<12)++state.stableFrames;
    if(state.stableFrames<12||!triggerAndGripNeutral){
        state.bothSticksAcknowledged=false;
        return false;
    }
    if(!state.reconnectAckRequired){
        // First healthy start: do not leave the player blocked by a hidden
        // two-stick acknowledgement prompt.
        state.armed=true;
        return true;
    }
    if(bothSticksPressed)state.bothSticksAcknowledged=true;
    if(state.bothSticksAcknowledged&&bothSticksReleased){
        state.armed=true;
        state.reconnectAckRequired=false;
        state.bothSticksAcknowledged=false;
        return true;
    }
    return false;
}

} // namespace kharvox
