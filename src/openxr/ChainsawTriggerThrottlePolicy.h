#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace kharvox {
// Animations use inverse engine tick targets: 240 Hz is the slow/idle end,
// 30 Hz is the fast-cutting end. These are NOT controller vibration rates.
// The gate is scoped to confirmed chainsaw executions in the OpenXR caller.
struct ChainsawTriggerThrottleOutput {
    int tickHz{240};
    float hapticAmplitude{0.20f};
    float hapticFrequencyHz{55.f};
    bool motionEnabled{};
};
inline float finiteChainsawClamp(float value,float low,float high) noexcept {
    return std::isfinite(value)?std::clamp(value,low,high):low;
}
inline ChainsawTriggerThrottleOutput evaluateChainsawTriggerThrottle(
    float trigger,float handMetersPerSecond,
    float fullMotionMetersPerSecond=1.8f) noexcept {
    trigger=finiteChainsawClamp(trigger,0.f,1.f);
    handMetersPerSecond=finiteChainsawClamp(handMetersPerSecond,0.f,20.f);
    fullMotionMetersPerSecond=
        finiteChainsawClamp(fullMotionMetersPerSecond,0.1f,20.f);
    constexpr float threshold=.10f;
    if(trigger<=threshold)
        return {}; // throttle off, ignore even rapid arm motion
    const float throttle=(trigger-threshold)/(1.f-threshold);
    const float motion=std::min(1.f,handMetersPerSecond/fullMotionMetersPerSecond);
    // Pulling the trigger revs; cutting stroke adds most of the animation
    // acceleration. Stationary full throttle CANNOT achieve full cut speed.
    const float cutting=throttle*(.20f+.80f*motion);
    const int tick=std::clamp(static_cast<int>(std::lround(
        240.f-210.f*cutting)),30,240);
    const float amplitude=std::clamp(
        .20f+.70f*throttle*(.65f+.35f*motion),.20f,.90f);
    return {tick,amplitude,55.f+145.f*throttle,true};
}
// Deliberate chunky texture, applied only to the chainsaw's physical hand.
// This changes vibration intensity, not the game's animation clock.
inline ChainsawTriggerThrottleOutput textureChainsawTriggerHaptics(
    ChainsawTriggerThrottleOutput in,std::uint64_t nowMs) noexcept {
    if(!in.motionEnabled)return in; // steady quiet idle
    const auto phase=(nowMs/28)%7;
    const float grit=phase==0||phase==4?.58f:phase==2?.79f:1.f;
    in.hapticAmplitude*=grit;
    return in;
}
} // namespace kharvox
