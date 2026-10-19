#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace kharvox {
// Circular Saw was retired. A single forward capture region is NOT a rotary
// gesture: reach OUTSIDE the ring with Trigger held, pull the tracked hand
// INTO the ring, then hand motion INSIDE controls the native chainsaw tick.
// 3 is the existing independent Pointer Focus experiment.
enum class ChainsawExperimentalMode:int {GripPull=1,PointerFocus=3};
inline const char* chainsawModeName(ChainsawExperimentalMode m) noexcept {
    return m==ChainsawExperimentalMode::PointerFocus?"POINTER_FOCUS":"SINGLE_PULL_ZONE";
}
struct ChainsawVec3 {float x{},y{},z{};};
inline ChainsawVec3 minus3(ChainsawVec3 a,ChainsawVec3 b) noexcept {
    return {a.x-b.x,a.y-b.y,a.z-b.z};
}
inline float dot3(ChainsawVec3 a,ChainsawVec3 b) noexcept {
    return a.x*b.x+a.y*b.y+a.z*b.z;
}
inline float length3(ChainsawVec3 a) noexcept {
    const float d=dot3(a,a);return d>=0.f&&std::isfinite(d)?std::sqrt(d):0.f;
}
inline ChainsawVec3 zonePosition(ChainsawVec3 head,float yaw,int /*legacyZone*/) noexcept {
    // Fallback where forward XR orientation is absent; NOT a target lock.
    const float s=std::sin(yaw),c=std::cos(yaw);
    return {head.x-s*1.05f,head.y-.17f,head.z-c*1.05f};
}
inline ChainsawVec3 singleZoneCenter(ChainsawVec3 head,ChainsawVec3 gaze,
                                    float yaw=0.f) noexcept {
    // Aim-aligned, ~1.05m forward, ~17cm below viewing direction to visually
    // frame an enemy's upper torso. Frozen at native execution entry. We do
    // NOT pretend to know the true demon position without an enemy-target hook.
    const float mag=length3(gaze);
    if(mag<.8f||mag>1.2f)return zonePosition(head,yaw,0);
    return {head.x+1.05f*gaze.x/mag,
            head.y+1.05f*gaze.y/mag-.17f,
            head.z+1.05f*gaze.z/mag};
}
constexpr float chainsawZoneEnterRadius=.30f; // 30cm sphere
constexpr float chainsawZoneExitRadius=.36f;  // hysteresis, avoid edge chatter

struct ChainsawExperimentState {
    bool active{},releasedGrip{},latchedGrip{},finished{},pointerStarted{},wasGripHeld{};
    bool seenOutside{},inside{},lastTracked{};
    int hovered{-1},lastHover{-1},startZone{-1},targetZone{-1};
    std::array<ChainsawVec3,4> zones{}; // only zones[0] rendered/used
    ChainsawExperimentalMode mode{ChainsawExperimentalMode::GripPull};
    ChainsawVec3 previous{},rayStart{};
    float progress{},peak{},filteredSpeed{};
    unsigned impactIndex{};
    std::uint64_t lastUpdateMs{},entryMs{};
    void reset() noexcept {*this={};}
};
struct ChainsawExperimentResult {
    int targetHz{300};
    float progress{};
    bool armed{},completed{},started{},impact{};
    const char* detail{"WAIT"};
    int hoveredZone{-1},startZone{-1},targetZone{-1};
    bool enteredZone{};
};

// Only an ACTUAL DOOM chainsaw execution owns this state. The control never
// starts a kill, steals Trigger input, invents collision, or edits other actions.
// The initial Trigger press normally starts the kill before the first execution
// frame; therefore we must NOT require a second/released Trigger press.
// 'progress' here is LIVE hand speed for the current frame, not total animation
// progress. Normal DOOM decides when the execution finishes.
inline ChainsawExperimentResult updateChainsawExperiment(
    ChainsawExperimentState& st,ChainsawExperimentalMode mode,
    bool nativeExecution,bool triggerHeld,bool handTracked,
    float x,float y,float z,bool rayValid,float rayX,float rayY,float rayZ,
    std::uint64_t nowMs,int idleHz=300,int strokeDistanceCm=32,
    ChainsawVec3 worldHead={},float worldYaw=0.f,bool headTracked=true,
    ChainsawVec3 headForward={}) noexcept {
    idleHz=std::clamp(idleHz,30,480);
    (void)strokeDistanceCm; // Retained for older saved calibration schemas.
    if(!nativeExecution){st.reset();return {idleHz};}
    if(mode!=ChainsawExperimentalMode::PointerFocus)
        mode=ChainsawExperimentalMode::GripPull;
    const bool valid=handTracked&&std::isfinite(x)&&std::isfinite(y)
        &&std::isfinite(z)&&std::abs(x)<20.f&&std::abs(y)<20.f&&std::abs(z)<20.f;
    const ChainsawVec3 hand{x,y,z};
    const bool validHead=headTracked&&std::isfinite(worldYaw)
        &&std::isfinite(worldHead.x)&&std::isfinite(worldHead.y)&&std::isfinite(worldHead.z);
    if(!st.active){
        if(!validHead)return {idleHz};
        st.active=true;st.mode=mode;st.lastUpdateMs=nowMs;
        st.zones[0]=singleZoneCenter(worldHead,headForward,worldYaw);
        st.previous=hand;st.lastTracked=valid;
        st.seenOutside=valid
            &&length3(minus3(hand,st.zones[0]))>=chainsawZoneExitRadius;
    }
    if(st.mode!=mode){st.reset();return {idleHz};}
    const bool timely=nowMs>st.lastUpdateMs
        &&nowMs-st.lastUpdateMs<=175;
    const auto elapsed=nowMs>=st.lastUpdateMs
        ?nowMs-st.lastUpdateMs:0;
    st.lastUpdateMs=nowMs;
    bool started=false,entered=false;
    if(mode==ChainsawExperimentalMode::GripPull){
        const float distance=valid?length3(minus3(hand,st.zones[0])):100.f;
        // A hand may already be inside at the very beginning of execution.
        // It must go OUTSIDE and back INSIDE; no extra Trigger press needed.
        if(valid&&distance>=chainsawZoneExitRadius) {
            st.seenOutside=true;
            st.inside=false;
        }else if(valid&&distance<=chainsawZoneEnterRadius)st.inside=true;
        st.hovered=valid&&st.inside?0:-1;
        entered=st.hovered==0&&st.lastHover!=0;
        st.lastHover=st.hovered;
        if(!st.latchedGrip){
            if(st.seenOutside&&st.inside&&valid&&triggerHeld){
                st.latchedGrip=true;st.startZone=0;st.entryMs=nowMs;
                st.filteredSpeed=.40f;started=true;
            }
        }
        // Tracking loss, release or exit NEVER applies simulated speed.
        // Do not allow stale high-speed state while controller is outside.
        if(st.latchedGrip){
            if(!triggerHeld||!valid||!st.inside){
                st.filteredSpeed=0.f;st.progress=0.f;
            }else{
                const float delta=st.lastTracked&&timely
                    ?length3(minus3(hand,st.previous)):0.f;
                const float instant=timely&&delta<.28f&&elapsed>0
                    ?std::clamp(delta*1000.f/float(elapsed),0.f,1.5f):0.f;
                // Preserve a short initial forward cutting impulse so
                // the execution visibly responds AS you pull into the ring.
                const float entryBoost=nowMs-st.entryMs<190? .55f:0.f;
                const float speed=std::max(instant,entryBoost);
                st.filteredSpeed=std::clamp(
                    .44f*speed+.56f*st.filteredSpeed,0.f,1.5f);
                // 55cm/s of real hand movement reaches fastest allowed 30Hz.
                // Stopping dynamically slows the cut; any direction is valid
                // while the hand is INSIDE the capture region.
                st.progress=std::clamp(st.filteredSpeed/.55f,0.f,1.f);
            }
        }
        if(valid)st.previous=hand;
        st.lastTracked=valid;
    }else{
        const ChainsawVec3 ray{rayX,rayY,rayZ};
        const bool rayOk=rayValid&&std::isfinite(rayX)&&std::isfinite(rayY)
            &&std::isfinite(rayZ)&&length3(ray)>.8f&&length3(ray)<1.2f;
        if(!st.pointerStarted&&rayOk){
            st.pointerStarted=true;st.rayStart=ray;started=true;
        }else if(rayOk){
            st.progress=std::max(st.progress,std::clamp(
                (std::acos(std::clamp(dot3(st.rayStart,ray),-1.f,1.f))
                 -.03491f)/.45379f,0.f,1.f));
        }
    }
    st.finished=st.progress>=.999f;
    const int hz=std::clamp(static_cast<int>(std::lround(
        idleHz-(idleHz-30)*st.progress)),30,idleHz);
    const unsigned quarters=std::min(4u,static_cast<unsigned>(st.progress*4.f));
    const bool impact=quarters>st.impactIndex;
    st.impactIndex=std::max(st.impactIndex,quarters);
    // The engine override is ACTIVE only with Trigger held and the hand
    // tracked INSIDE the circle. Leaving the circle or losing tracking
    // restores native execution speed immediately (avoids slow-motion leaks).
    const bool owningTiming=mode==ChainsawExperimentalMode::PointerFocus
        ?st.pointerStarted
        :(st.latchedGrip&&triggerHeld&&valid&&st.inside);
    return {hz,st.progress,owningTiming,
        st.finished,started,impact,chainsawModeName(mode),
        st.hovered,st.startZone,st.targetZone,entered};
}
} // namespace kharvox
