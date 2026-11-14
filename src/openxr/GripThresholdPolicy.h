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

// Controller-only safety latch. The runtime can report a valid "controller"
// pose even while tracking bare hands, so we also require a fresh, deliberate
// two-stick acknowledgement after losing a controller or its input profile.
// The lock starts engaged; it never auto-rearms when emulated inputs appear.
struct ControllerOnlySafetyState {
    bool armed{},bothSticksAcknowledged{};
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
        state={};
        return false;
    }
    if(state.armed)return true;
    if(state.stableFrames<12)++state.stableFrames;
    if(state.stableFrames<12||!triggerAndGripNeutral){
        state.bothSticksAcknowledged=false;
        return false;
    }
    if(bothSticksPressed)state.bothSticksAcknowledged=true;
    if(state.bothSticksAcknowledged&&bothSticksReleased){
        state.armed=true;
        return true;
    }
    return false;
}

} // namespace kharvox
