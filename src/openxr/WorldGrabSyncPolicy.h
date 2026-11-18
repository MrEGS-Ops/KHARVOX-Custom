#pragma once
// Passive evidence is NOT a native kick hook or model-render confirmation.
namespace kharvox {
inline constexpr bool worldGrabNativeWakeCandidate(
    bool priorKnown,int priorRest,bool currentKnown,int currentRest,
    bool recentManualWrite,bool pinned) noexcept {
    return priorKnown&&currentKnown&&priorRest>=0&&currentRest==-1
        &&!recentManualWrite&&!pinned;
}
inline constexpr bool worldGrabPassiveMotionCandidate(
    float deltaSquared,bool recentManualWrite,bool pinned) noexcept {
    return deltaSquared>0.01f*0.01f&&!recentManualWrite&&!pinned;
}
static_assert(worldGrabNativeWakeCandidate(true,1,true,-1,false,false));
static_assert(!worldGrabPassiveMotionCandidate(1.f,false,true));
}
