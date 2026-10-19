#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace kharvox {
// Animations use inverse engine tick targets: 240 Hz is the slow/idle end,
// 30 Hz is the fast-cutting end. These are NOT controller vibration rates.
// The gate is scoped to confirmed chainsaw executions in the OpenXR caller.
// Launch-scoped Haptic Lab settings, intentionally separate from the
// 30–240Hz simulation tick override. All six controls tune only the weapon
// hand's haptic amplitude/texture/frequency, NEVER action execution speed.
struct ChainsawHapticConfig {
    int idlePercent{20};
    int cuttingPercent{90};
    int pulseRate{45};
    int pulseDurationMs{20};
    int roughnessPercent{35};
    int motionSensitivityPercent{75};
};
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
    float fullMotionMetersPerSecond=1.8f,
    const ChainsawHapticConfig& config={}) noexcept {
    trigger=finiteChainsawClamp(trigger,0.f,1.f);
    handMetersPerSecond=finiteChainsawClamp(handMetersPerSecond,0.f,20.f);
    fullMotionMetersPerSecond=
        finiteChainsawClamp(fullMotionMetersPerSecond,0.1f,20.f);
    constexpr float threshold=.10f;
    const float idle=std::clamp(config.idlePercent,0,100)/100.f;
    const float cuttingMax=std::clamp(config.cuttingPercent,0,100)/100.f;
    const float hapticRate=std::clamp(
        55.f+(.4f*(std::clamp(config.pulseRate,5,100)-45)),40.f,240.f);
    if(trigger<=threshold)
        return {240,idle,hapticRate,false}; // no trigger -> no motion acceleration
    const float throttle=(trigger-threshold)/(1.f-threshold);
    const float motion=std::min(1.f,handMetersPerSecond/fullMotionMetersPerSecond);
    // Pulling the trigger revs; cutting stroke adds most of the animation
    // acceleration. Stationary full throttle CANNOT achieve full cut speed.
    const float cutting=throttle*(.20f+.80f*motion);
    const int tick=std::clamp(static_cast<int>(std::lround(
        240.f-210.f*cutting)),30,240);
    // Motion Sensitivity is haptics-only; never influence 'cutting' or tickHz.
    // 75% is the legacy baseline. Higher/lower values adjust moving-hand feel.
    const float sensitivity=std::clamp(config.motionSensitivityPercent,0,100)/100.f;
    const float motionGain=1.f+(sensitivity-.75f)*.30f*motion;
    const float base=idle+(cuttingMax-idle)*throttle*(.65f+.35f*motion);
    const float amplitude=std::clamp(base*motionGain,0.f,1.f);
    return {tick,amplitude,
        std::clamp(hapticRate+145.f*throttle,40.f,240.f),true};
}
// Deliberate chunky texture, applied only to the chainsaw's physical hand.
// This changes vibration intensity, not the game's animation clock.
inline ChainsawTriggerThrottleOutput textureChainsawTriggerHaptics(
    ChainsawTriggerThrottleOutput in,std::uint64_t nowMs,
    const ChainsawHapticConfig& config={}) noexcept {
    if(!in.motionEnabled)return in; // idle is steady and unaffected by pulses
    const auto rate=static_cast<std::uint64_t>(std::clamp(config.pulseRate,5,100));
    const auto period=std::max<std::uint64_t>(8,1000/rate);
    const auto phase=(nowMs/period)%7;
    const float rough=std::clamp(config.roughnessPercent,0,100)/100.f;
    const float grit=phase==0||phase==4?.58f:phase==2?.79f:1.f;
    const auto pulseDuration=static_cast<std::uint64_t>(
        std::clamp(config.pulseDurationMs,5,100));
    const bool on=(nowMs%period)<std::min(period,pulseDuration);
    const float pulseScale=on?1.f:1.f-.75f*rough;
    in.hapticAmplitude*=pulseScale*(1.f-rough*(1.f-grit));
    return in;
}
} // namespace kharvox
