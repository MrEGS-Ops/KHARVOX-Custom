#pragma once
#include <cstdint>
// Single DEV Hand Anchor. The 28-unit hand-relative follow remains mode 6.
// All activation experiments run automatically after Grip, with a long
// held-grip manual-kick window before any direct-follow fallback.
namespace kharvox {
inline constexpr int kWorldGrabHandAnchorMode=6;
inline constexpr unsigned kWorldGrabStageWake=1;
inline constexpr unsigned kWorldGrabStageContact=2;
inline constexpr unsigned kWorldGrabStageMicro=3;
inline constexpr unsigned kWorldGrabStageObserve=4;
inline constexpr unsigned kWorldGrabStageHeldKick=5;
inline constexpr unsigned kWorldGrabStageDirect=6;
inline constexpr std::uint64_t kWorldGrabManualWriteQuarantineMs=350;
inline constexpr bool worldGrabStrongAnchor(int mode) noexcept {
    return mode==kWorldGrabHandAnchorMode;
}
inline constexpr bool worldGrabAnchorMode(int mode) noexcept {
    return worldGrabStrongAnchor(mode);
}
inline constexpr const char* worldGrabStageName(unsigned stage) noexcept {
    switch(stage){
    case 1:return "NATIVE_WAKE";
    case 2:return "CONTACT_PROXY";
    case 3:return "MICRO_MOVE";
    case 4:return "ENTITY_TICK";
    case 5:return "HELD_GRIP_KICK_WINDOW";
    case 6:return "DIRECT_FOLLOW_FALLBACK";
    default:return "NONE";
    }
}
inline constexpr std::uint64_t worldGrabStageWindowMs(unsigned stage) noexcept {
    switch(stage){
    case 1:return 200;
    case 2:return 500;
    case 3:return 500;
    case 4:return 350;
    case 5:return 12000; // plenty of time to walk into/kick while gripping
    default:return 0;
    }
}
inline constexpr bool worldGrabStageNativeEvidence(
    unsigned stage,std::uint64_t startedAt,std::uint64_t movementAt,
    bool motionObserved,std::uint64_t lastManualWriteAt,
    std::uint64_t now) noexcept {
    return stage>=1&&stage<=5&&motionObserved&&movementAt>=startedAt
        &&movementAt!=0&&movementAt<=now
        &&(!lastManualWriteAt
            ||(now>=lastManualWriteAt
                &&now-lastManualWriteAt>kWorldGrabManualWriteQuarantineMs));
}
inline constexpr bool worldGrabAnchorFollowReady(
    int mode,bool nativeMotionObserved,std::uint64_t /*heldMs*/) noexcept {
    return worldGrabAnchorMode(mode)&&nativeMotionObserved;
}
static_assert(worldGrabAnchorMode(6)&&!worldGrabAnchorMode(1)
    &&!worldGrabAnchorMode(2)&&!worldGrabAnchorMode(3)
    &&!worldGrabAnchorMode(4)&&!worldGrabAnchorMode(5));
static_assert(worldGrabStageWindowMs(5)==12000);
static_assert(worldGrabStageNativeEvidence(5,100,110,true,0,120));
static_assert(!worldGrabStageNativeEvidence(2,100,110,true,105,120));
}
