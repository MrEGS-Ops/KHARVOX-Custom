#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace kharvox {
// Existing KHARVOX action tick control is inverse: LOWER Hz speeds actions UP.
// Thus a stopped chainsaw hand uses 200Hz; swift movement drives the target
// toward 30Hz. No game time scale, animation markers, or camera values change.
constexpr int kChainsawMotionIdleHz=200;
constexpr int kChainsawMotionFastestHz=30;
constexpr int kChainsawMotionDefaultFullSpeedCmPerSec=180;
struct ChainsawMotionSpeedState {
    bool active{};
    std::uint64_t previousMs{};
    float filteredMetersPerSecond{};
    int appliedHz{kChainsawMotionIdleHz};
    void reset() noexcept { *this={}; }
};
struct ChainsawMotionSpeedSample {
    int targetHz{kChainsawMotionIdleHz};
    float filteredMetersPerSecond{};
    bool started{};
};
inline ChainsawMotionSpeedSample updateChainsawMotionSpeed(
    ChainsawMotionSpeedState& state, bool chainsawExecution, bool velocityValid,
    float velocityX, float velocityY, float velocityZ,
    std::uint64_t nowMs, int fullSpeedCmPerSec=kChainsawMotionDefaultFullSpeedCmPerSec) noexcept {
    if(!chainsawExecution){state.reset();return {};}
    if(!state.active){
        state.active=true;
        state.previousMs=nowMs;
        state.filteredMetersPerSecond=0.f;
        state.appliedHz=kChainsawMotionIdleHz;
        return {kChainsawMotionIdleHz,0.f,true};
    }
    const auto elapsed=nowMs>=state.previousMs
        ?std::min<std::uint64_t>(nowMs-state.previousMs,100):0;
    state.previousMs=nowMs;
    // Tracking may report spikes, NaN, or stale velocity when the controller
    // is occluded. In all cases treat that frame as a stationary hand.
    const bool finite=std::isfinite(velocityX)&&std::isfinite(velocityY)
        &&std::isfinite(velocityZ);
    float raw=0.f;
    if(velocityValid&&finite){
        const float squared=velocityX*velocityX+velocityY*velocityY+velocityZ*velocityZ;
        if(std::isfinite(squared))raw=std::min(std::sqrt(squared),4.f);
    }
    // Fast enough to follow a cutting stroke, but avoid violent Hz oscillation
    // from headset/controller tracking noise. Decelerate promptly when stopped.
    const float tauMs=raw>state.filteredMetersPerSecond?65.f:100.f;
    const float alpha=static_cast<float>(elapsed)/(tauMs+static_cast<float>(elapsed));
    state.filteredMetersPerSecond +=
        alpha*(raw-state.filteredMetersPerSecond);
    const float fullSpeed=std::clamp(fullSpeedCmPerSec,70,300)/100.f;
    constexpr float deadzone=0.10f;
    const float movement=std::clamp(
        (state.filteredMetersPerSecond-deadzone)/(fullSpeed-deadzone),0.f,1.f);
    const float newHz=kChainsawMotionIdleHz
        -movement*(kChainsawMotionIdleHz-kChainsawMotionFastestHz);
    state.appliedHz=std::clamp(static_cast<int>(std::lround(newHz)),
        kChainsawMotionFastestHz,kChainsawMotionIdleHz);
    return {state.appliedHz,state.filteredMetersPerSecond,false};
}
}
