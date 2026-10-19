#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace kharvox {
// Four virtual start zones. The FIRST intentional hand displacement after
// trigger press selects a side. A stroke crosses toward the opposite side.
// This is relative tracked hand displacement, not head/camera rotation.
enum class ChainsawStrokeZone : unsigned char {
    None, Left, Right, Up, Down
};
inline const char* chainsawStrokeZoneName(ChainsawStrokeZone z) noexcept {
    switch(z) {
    case ChainsawStrokeZone::Left:return "LEFT -> RIGHT";
    case ChainsawStrokeZone::Right:return "RIGHT -> LEFT";
    case ChainsawStrokeZone::Up:return "UP -> DOWN";
    case ChainsawStrokeZone::Down:return "DOWN -> UP";
    default:return "UNASSIGNED";
    }
}
struct ChainsawStrokeState {
    bool armed{}, directionLocked{}, completed{}, wasTriggerDown{}, sawNativeExecution{};
    ChainsawStrokeZone zone{ChainsawStrokeZone::None};
    float originX{}, originY{}, originZ{};
    float peakMeters{}, progress{};
    unsigned impactIndex{};
    int targetHz{300};
    void reset() noexcept { *this={}; }
};
struct ChainsawStrokeSample {
    int targetHz{300};
    float progress{};
    bool armed{}, directionLocked{}, completed{}, started{}, impact{};
    ChainsawStrokeZone zone{ChainsawStrokeZone::None};
};
// No time-based advancement. Only genuine hand travel moves the stroke.
// Monotonic progress prevents a short pause/backtrack from undoing a cut.
// Native DOOM execution-state exit remains owned exclusively by the game.
inline ChainsawStrokeSample updateChainsawStroke(
    ChainsawStrokeState& state,bool available,bool triggerDown,
    bool positionTracked,float x,float y,float z,
    bool nativeExecution,int idleHz=300,int strokeDistanceCm=32) noexcept {
    idleHz=std::clamp(idleHz,30,300);
    strokeDistanceCm=std::clamp(strokeDistanceCm,18,60);
    const bool valid=positionTracked&&std::isfinite(x)&&std::isfinite(y)
        &&std::isfinite(z)&&std::abs(x)<100.f&&std::abs(y)<100.f
        &&std::abs(z)<100.f;
    // Disarm on the genuine DOOM execution exit. Keeping the trigger held
    // should never recycle a completed stroke into the next enemy.
    if(!available || (!triggerDown&&!nativeExecution)
        ||(state.sawNativeExecution&&!nativeExecution)) {
        state.reset();
        state.wasTriggerDown=triggerDown;
        return {idleHz};
    }
    bool started=false;
    if(!state.armed){
        if(!triggerDown||!valid)return {idleHz};
        state.armed=true;
        state.originX=x;state.originY=y;state.originZ=z;
        state.targetHz=idleHz;
        started=true;
    }
    state.wasTriggerDown=triggerDown;
    state.sawNativeExecution|=nativeExecution;
    if(valid&&!state.completed){
        float dx=x-state.originX,dy=y-state.originY;
        // Reject subtle tracking jitter; choose movement axis/direction
        // once, then NEVER change it within this one trigger press.
        constexpr float latchDistance=0.045f;
        if(!state.directionLocked
           &&std::max(std::abs(dx),std::abs(dy))>=latchDistance){
            state.directionLocked=true;
            if(std::abs(dx)>=std::abs(dy))
                state.zone=dx>=0.f
                    ?ChainsawStrokeZone::Left:ChainsawStrokeZone::Right;
            else state.zone=dy>=0.f
                    ?ChainsawStrokeZone::Down:ChainsawStrokeZone::Up;
        }
        float toward=0.f;
        switch(state.zone){
        case ChainsawStrokeZone::Left: toward=dx;break;
        case ChainsawStrokeZone::Right:toward=-dx;break;
        case ChainsawStrokeZone::Down:toward=dy;break;
        case ChainsawStrokeZone::Up:toward=-dy;break;
        default:break;
        }
        // Reject huge single-frame controller tracking teleports by
        // requiring each progress step to have a physical near-neighbour.
        if(state.directionLocked&&std::isfinite(toward)
            &&toward>=0.f&&toward<1.4f)
            state.peakMeters=std::max(state.peakMeters,toward);
        state.progress=std::clamp(state.peakMeters/
            (strokeDistanceCm/100.f),0.f,1.f);
        if(state.progress>=.999f)state.completed=true;
    }
    // Inverse action engine Hz. THIS IS A SPEED ENVELOPE, not a native
    // animation-time seek. At the far zone, keep fastest Hz until DOOM exits
    // its own execution. Nothing force-finishes damage, camera or cutscenes.
    state.targetHz=std::clamp(static_cast<int>(std::lround(
        idleHz-(idleHz-30)*state.progress)),30,idleHz);
    const unsigned stage=static_cast<unsigned>(
        std::min(4.f,std::floor(state.progress*4.f)));
    const bool impact=stage>state.impactIndex;
    state.impactIndex=std::max(state.impactIndex,stage);
    return {state.targetHz,state.progress,state.armed,
        state.directionLocked,state.completed,started,impact,state.zone};
}
} // namespace kharvox
