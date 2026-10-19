#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace kharvox {

// Three isolated experimental motion policies. OpenXR supplies a pre-existing,
// native DOOM chainsaw execution flag; no mode starts a kill or seeks frames.
// A mode is latched for the full execution and can never control GK/ledge.
// Inputs are XR local-space metres and a world-space controller forward ray.
enum class ChainsawExperimentalMode : int {
    GripPull=1, Circular=2, PointerFocus=3
};
inline const char* chainsawModeName(ChainsawExperimentalMode mode) noexcept {
    switch(mode){
        case ChainsawExperimentalMode::GripPull:return "GRIP_PULL";
        case ChainsawExperimentalMode::Circular:return "CIRCULAR";
        case ChainsawExperimentalMode::PointerFocus:return "POINTER_FOCUS";
        default:return "INVALID";
    }
}
struct ChainsawExperimentState {
    bool active{}, finished{}, releasedGrip{}, latchedGrip{}, ringStarted{}, pointerStarted{};
    bool directionLocked{};
    ChainsawExperimentalMode mode{ChainsawExperimentalMode::GripPull};
    float originX{},originY{},originZ{};
    float previousX{},previousY{},ringPreviousAngle{};
    float ringTravelRadians{},ringDirection{};
    float rayStartX{},rayStartY{},rayStartZ{};
    float travelled{}, progress{};
    unsigned impactIndex{};
    std::uint64_t firstSeenMs{},lastUpdateMs{};
    void reset() noexcept { *this={}; }
};
struct ChainsawExperimentResult {
    int targetHz{300};
    float progress{};
    bool armed{},completed{},started{},impact{};
    const char* detail{"WAIT"};
};
inline float safeLength3(float x,float y,float z) noexcept {
    const float length=std::sqrt(x*x+y*y+z*z);
    return std::isfinite(length)?length:0.f;
}
inline float safeAngle(float a) noexcept {
    constexpr float pi=3.14159265358979323846f;
    while(a>pi)a-=2.f*pi;
    while(a< -pi)a+=2.f*pi;
    return a;
}
inline ChainsawExperimentResult updateChainsawExperiment(
    ChainsawExperimentState& state,
    ChainsawExperimentalMode mode,
    bool nativeChainsawExecution,bool gripHeld,
    bool trackingValid,float x,float y,float z,
    bool forwardValid,float forwardX,float forwardY,float forwardZ,
    std::uint64_t nowMs,int idleHz=300,int strokeDistanceCm=32) noexcept {
    idleHz=std::clamp(idleHz,30,300);
    strokeDistanceCm=std::clamp(strokeDistanceCm,18,60);
    if(!nativeChainsawExecution||mode!=ChainsawExperimentalMode::GripPull
        &&mode!=ChainsawExperimentalMode::Circular
        &&mode!=ChainsawExperimentalMode::PointerFocus){
        state.reset();return {idleHz};
    }
    const bool poseValid=trackingValid&&std::isfinite(x)&&std::isfinite(y)
        &&std::isfinite(z)&&std::abs(x)<20.f&&std::abs(y)<20.f
        &&std::abs(z)<20.f;
    const bool rayValid=forwardValid
        &&std::isfinite(forwardX)&&std::isfinite(forwardY)&&std::isfinite(forwardZ)
        &&safeLength3(forwardX,forwardY,forwardZ)>.8f
        &&safeLength3(forwardX,forwardY,forwardZ)<1.2f;
    if(!state.active){
        state.active=true;state.mode=mode;state.firstSeenMs=nowMs;
        state.lastUpdateMs=nowMs;state.releasedGrip=!gripHeld;
    }
    if(state.mode!=mode){state.reset();return {idleHz};}
    // We do not infer motion from stale tracking or improbable world-space
    // jumps. A short dropout freezes progress without resetting the mode.
    const bool timely=nowMs>=state.lastUpdateMs
        &&nowMs-state.lastUpdateMs<=175;
    state.lastUpdateMs=nowMs;
    const bool ready=(mode==ChainsawExperimentalMode::PointerFocus)
        ?rayValid:poseValid;
    bool justStarted=false;
    if(mode==ChainsawExperimentalMode::GripPull){
        if(!gripHeld)state.releasedGrip=true;
        if(!state.latchedGrip){
            if(!state.releasedGrip||!gripHeld||!poseValid)
                return {idleHz,0.f,false,false,false,false,"RELEASE_THEN_GRAB"};
            state.latchedGrip=true;justStarted=true;
            state.originX=x;state.originY=y;state.originZ=z;
            state.previousX=x;state.previousY=y;
        }
        if(gripHeld&&ready&&!state.finished){
            const float dx=x-state.originX,dy=y-state.originY,dz=z-state.originZ;
            const float distance=safeLength3(dx,dy,dz);
            // Ignore tracking teleports: distance is relative to a latched
            // grip point, with a 45cm/frame sanity cap.
            if(distance<=1.f){
                state.travelled=std::max(state.travelled,distance);
                state.progress=std::clamp((state.travelled-.035f)/
                    std::max(.10f,strokeDistanceCm/100.f-.035f),0.f,1.f);
            }
            state.previousX=x;state.previousY=y;
        }
    }else if(mode==ChainsawExperimentalMode::Circular){
        // A *single* large invisible ring, centered at the first hand pose.
        // Once the hand enters the ring (8–28 cm radius), a 155-degree arc
        // advances the chain. Neither small jitters nor back-and-forth turns
        // can fill the arc: direction locks at the first 8-degree turn.
        if(!state.ringStarted){
            if(!poseValid)return {idleHz,0.f,false,false,false,false,"TRACKING"};
            state.originX=x;state.originY=y;state.originZ=z;
            state.ringStarted=true;justStarted=true;
        }else if(ready&&!state.finished){
            const float dx=x-state.originX,dy=y-state.originY;
            const float radius=std::sqrt(dx*dx+dy*dy);
            if(radius>=.08f&&radius<=.28f){
                const float angle=std::atan2(dy,dx);
                if(!state.pointerStarted){
                    state.pointerStarted=true;state.ringPreviousAngle=angle;
                }else{
                    const float da=safeAngle(angle-state.ringPreviousAngle);
                    // Short segments only. Never count a teleport as an arc.
                    if(timely&&std::abs(da)<.45f){
                        if(!state.directionLocked&&std::abs(da)>.14f){
                            state.directionLocked=true;
                            state.ringDirection=da>0.f?1.f:-1.f;
                        }
                        if(state.directionLocked&&da*state.ringDirection>0.f)
                            state.ringTravelRadians+=da*state.ringDirection;
                        state.progress=std::clamp(
                            state.ringTravelRadians/2.70526f,0.f,1.f);
                    }
                    state.ringPreviousAngle=angle;
                }
            }
        }
    }else{
        // A virtual laser pointer from the weapon-hand controller.
        // Aim changes relative to a latched ray, not headset gaze or velocity.
        // There is intentionally NO visible beam in this diagnostic version.
        if(!state.pointerStarted){
            if(!rayValid)return {idleHz,0.f,false,false,false,false,"TRACKING"};
            state.rayStartX=forwardX;state.rayStartY=forwardY;state.rayStartZ=forwardZ;
            state.pointerStarted=true;justStarted=true;
        }else if(rayValid&&!state.finished){
            const float dot=std::clamp(
                state.rayStartX*forwardX+state.rayStartY*forwardY
                +state.rayStartZ*forwardZ,-1.f,1.f);
            const float angle=std::acos(dot);
            // One deliberate 28 degree pointer sweep, no recentering.
            // Suppress the first two degrees of rotational noise.
            state.progress=std::max(state.progress,
                std::clamp((angle-.03491f)/.45379f,0.f,1.f));
        }
    }
    if(state.progress>=.999f)state.finished=true;
    // Advance earlier than the hand arrives: DOOM's native sync animation
    // cannot be frame-scrubbed from a VR pose. 70% physical stroke is enough
    // to reach the fastest permitted engine pacing; native execution still
    // decides when it actually finishes.
    const float speedProgress=std::clamp(state.progress/.70f,0.f,1.f);
    const int hz=std::clamp(static_cast<int>(std::lround(
        idleHz-(idleHz-30)*speedProgress)),30,idleHz);
    const unsigned impacts=std::min(4u,static_cast<unsigned>(state.progress*4.f));
    const bool impact=impacts>state.impactIndex;
    state.impactIndex=std::max(state.impactIndex,impacts);
    return {hz,state.progress,state.latchedGrip||state.ringStarted||state.pointerStarted,
        state.finished,justStarted,impact,chainsawModeName(mode)};
}
} // namespace kharvox
