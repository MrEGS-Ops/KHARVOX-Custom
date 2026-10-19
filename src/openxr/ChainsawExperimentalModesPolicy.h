#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace kharvox {
// Circular Saw retired. 2 is accepted only as a saved legacy value and maps
// to Grip Zones. 3 remains Pointer Focus for independent A/B testing.
enum class ChainsawExperimentalMode:int {GripPull=1,PointerFocus=3};
inline const char* chainsawModeName(ChainsawExperimentalMode m) noexcept {
    return m==ChainsawExperimentalMode::PointerFocus?"POINTER_FOCUS":"GRIP_ZONES";
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
inline ChainsawVec3 zonePosition(ChainsawVec3 head,float yaw,int zone) noexcept {
    // Head-relative four-zone cross in REAL OpenXR tracking space.
    // User headset feedback #591: targets were up in their face and cramped.
    // Bring the targets into reachable hand range: 65cm ahead, roughly
    // shoulder-/chest-height. Previous 84cm-forward/100cm-wide targets
    // were outside the headset FOV and difficult to physically reach.
    // Keep a 13cm capture sphere, exactly matched by the visual cue.
    // Zones remain fixed for the duration of the native chainsaw execution.
    const float sx=std::sin(yaw),cx=std::cos(yaw);
    const float sideways=(zone==0?-.30f:zone==1?.30f:0.f);
    const float vertical=(zone==2?.29f:zone==3?-.29f:0.f);
    return {head.x+cx*sideways-sx*.65f,
            head.y-.37f+vertical,
            head.z-sx*sideways-cx*.65f};
}
struct ChainsawExperimentState {
    bool active{}, releasedGrip{},latchedGrip{},finished{},pointerStarted{},wasGripHeld{};
    int hovered{-1},lastHover{-1},startZone{-1},targetZone{-1};
    std::array<ChainsawVec3,4> zones{};
    ChainsawExperimentalMode mode{ChainsawExperimentalMode::GripPull};
    ChainsawVec3 previous{},rayStart{};
    float progress{},peak{};
    unsigned impactIndex{};
    std::uint64_t lastUpdateMs{};
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
// 'worldHead' and 'yaw' are captured from the XR LOCAL reference at the
// first confirmed DOOM execution frame, never updated until execution exit.
// After triggering a native execution, RELEASE then PRESS weapon Trigger inside a zone.
inline ChainsawExperimentResult updateChainsawExperiment(
    ChainsawExperimentState& st,ChainsawExperimentalMode mode,
    bool nativeExecution,bool triggerHeld,bool handTracked,
    float x,float y,float z,bool rayValid,float rayX,float rayY,float rayZ,
    std::uint64_t nowMs,int idleHz=300,int strokeDistanceCm=32,
    ChainsawVec3 worldHead={},float worldYaw=0.f,bool headTracked=true) noexcept {
    idleHz=std::clamp(idleHz,30,480);
    strokeDistanceCm=std::clamp(strokeDistanceCm,18,60);
    if(!nativeExecution){st.reset();return {idleHz};}
    if(mode!=ChainsawExperimentalMode::GripPull
        &&mode!=ChainsawExperimentalMode::PointerFocus)
        mode=ChainsawExperimentalMode::GripPull;
    const bool valid=handTracked&&std::isfinite(x)&&std::isfinite(y)
        &&std::isfinite(z)&&std::abs(x)<20.f&&std::abs(y)<20.f&&std::abs(z)<20.f;
    const ChainsawVec3 hand{x,y,z};
    const bool validHead=headTracked&&std::isfinite(worldYaw)
        &&std::isfinite(worldHead.x)&&std::isfinite(worldHead.y)&&std::isfinite(worldHead.z);
    if(!st.active){
        if(!validHead)return {idleHz};
        st.active=true;st.mode=mode;st.releasedGrip=!triggerHeld;st.lastUpdateMs=nowMs;
        for(int i=0;i<4;++i)st.zones[i]=zonePosition(worldHead,worldYaw,i);
    }
    if(st.mode!=mode){st.reset();return {idleHz};}
    const bool timely=nowMs>=st.lastUpdateMs&&nowMs-st.lastUpdateMs<=175;
    st.lastUpdateMs=nowMs;
    bool started=false,impact=false,entered=false;
    if(mode==ChainsawExperimentalMode::GripPull){
        // Primary weapon trigger only. Fresh edge INSIDE a zone after
        // native kill started; do not steal the primary fire/XInput route.
        const bool freshTrigger=triggerHeld&&!st.wasGripHeld;
        st.wasGripHeld=triggerHeld;
        if(!triggerHeld)st.releasedGrip=true;
        int nearest=-1;float best=.13f;
        if(valid){
            for(int i=0;i<4;++i){
                const float d=length3(minus3(hand,st.zones[i]));
                if(d<best){best=d;nearest=i;}
            }
        }
        st.hovered=nearest;
        entered=nearest>=0&&nearest!=st.lastHover&&!st.latchedGrip;
        st.lastHover=nearest;
        if(!st.latchedGrip){
            if(st.releasedGrip&&freshTrigger&&nearest>=0&&valid){
                st.latchedGrip=true;st.startZone=nearest;
                st.previous=hand;started=true;
            }
        }else if(triggerHeld&&valid&&!st.finished){
            const float step=length3(minus3(hand,st.previous));
            if(timely&&step<=.30f){
                const ChainsawVec3 from=st.zones[st.startZone];
                if(st.targetZone<0&&length3(minus3(hand,from))>=.055f){
                    const ChainsawVec3 toward=minus3(hand,from);
                    float highest=-1.f;
                    for(int i=0;i<4;++i){
                        if(i==st.startZone)continue;
                        const ChainsawVec3 path=minus3(st.zones[i],from);
                        const float advance=dot3(toward,path)/length3(path);
                        if(advance>highest){highest=advance;st.targetZone=i;}
                    }
                }
                if(st.targetZone>=0){
                    const ChainsawVec3 path=minus3(st.zones[st.targetZone],from);
                    const float distance=length3(path);
                    const float toward=dot3(minus3(hand,from),path)/distance;
                    st.peak=std::max(st.peak,std::max(0.f,toward));
                    st.progress=std::max(st.progress,std::clamp(
                        (st.peak-.035f)/(distance-.035f),0.f,1.f));
                    if(length3(minus3(hand,st.zones[st.targetZone]))<=.13f)
                        st.progress=1.f; // arrive in another ACTUAL zone
                }
            }
            st.previous=hand;
        }
    }else{
        const ChainsawVec3 ray{rayX,rayY,rayZ};
        const bool rayOk=rayValid&&std::isfinite(rayX)&&std::isfinite(rayY)
            &&std::isfinite(rayZ)&&length3(ray)>.8f&&length3(ray)<1.2f;
        if(!st.pointerStarted&&rayOk){st.pointerStarted=true;st.rayStart=ray;started=true;}
        else if(rayOk&&!st.finished){
            const float dot=std::clamp(dot3(st.rayStart,ray),-1.f,1.f);
            st.progress=std::max(st.progress,std::clamp(
                (std::acos(dot)-.03491f)/.45379f,0.f,1.f));
        }
    }
    if(st.progress>=.999f)st.finished=true;
    const float speed=std::clamp(st.progress/.70f,0.f,1.f);
    const int hz=std::clamp(static_cast<int>(std::lround(
        idleHz-(idleHz-30)*speed)),30,idleHz);
    const unsigned quarters=std::min(4u,static_cast<unsigned>(st.progress*4.f));
    impact=quarters>st.impactIndex;
    st.impactIndex=std::max(st.impactIndex,quarters);
    return {hz,st.progress,st.latchedGrip||st.pointerStarted,
        st.finished,started,impact,chainsawModeName(mode),
        st.hovered,st.startZone,st.targetZone,entered};
}
} // namespace kharvox
