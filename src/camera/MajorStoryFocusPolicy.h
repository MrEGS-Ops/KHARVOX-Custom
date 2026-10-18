#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace kharvox {
// Major scripted scenes are detected by sustained native authored camera yaw,
// not by a Glory Kill/ledge flag or a simple long-animation timer. A scene's
// original focus setting is never modified; this only controls the effective
// camera rotation when the checkbox is already enabled.
struct MajorStoryFocusState {
    std::uint64_t beganMs{};
    std::uint64_t engagedMs{};
    float largestNativeYawDegrees{};
    bool active{};
    bool engaged{};
    void begin(std::uint64_t nowMs) noexcept {
        beganMs=nowMs;engagedMs=0;largestNativeYawDegrees=0.0f;
        active=true;engaged=false;
    }
    void reset() noexcept { *this={}; }
};
struct MajorStoryFocusResult {
    float weight{};
    bool newlyEngaged{};
};
inline MajorStoryFocusResult updateMajorStoryFocus(
    MajorStoryFocusState& s,std::uint64_t nowMs,bool requested,
    bool independentStory,float yawFromGameplayDegrees) noexcept {
    if(!s.active)s.begin(nowMs);
    if(!std::isfinite(yawFromGameplayDegrees))yawFromGameplayDegrees=0.0f;
    s.largestNativeYawDegrees=std::max(s.largestNativeYawDegrees,
        std::fabs(yawFromGameplayDegrees));
    if(!requested||!independentStory)return {};
    // Short interactions should remain fully HMD-oriented. A significant
    // authored heading after a continuous scripted period is a conservative
    // proxy for larger scenes (Pinky introductions, cinematic key extraction).
    constexpr std::uint64_t minSceneMs=1300;
    constexpr float minNativeYawDegrees=25.0f;
    constexpr float blendMs=500.0f;
    bool engagedNow=false;
    if(!s.engaged&&nowMs>=s.beganMs+minSceneMs
        &&s.largestNativeYawDegrees>=minNativeYawDegrees){
        s.engaged=true;s.engagedMs=nowMs;engagedNow=true;
    }
    if(!s.engaged)return {};
    const float t=std::clamp(static_cast<float>(nowMs-s.engagedMs)/blendMs,0.0f,1.0f);
    return {t*t*(3.0f-2.0f*t),engagedNow};
}
inline float shortestYawDegrees(float authored,float held) noexcept {
    const float delta=std::remainder(authored-held,360.0f);
    return std::isfinite(delta)?delta:0.0f;
}
} 
