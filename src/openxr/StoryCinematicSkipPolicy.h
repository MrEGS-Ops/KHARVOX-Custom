#pragma once
#include "ActionSpeedPolicy.h"

namespace kharvox {

// RS toggles a *scoped* native game-timescale request; never jumps mission
// state, advances scripts, or changes camera/HMD pose.
struct StoryCinematicSkipState {
    ScriptedCinematicBoundaryState boundary{};
    unsigned long long sceneSinceMs{};
    unsigned long long acceleratingSinceMs{};
    bool rsWasDown{};
    bool accelerating{};
};

struct StoryCinematicSkipDecision {
    bool consumeRs{};
    bool changed{};
    bool accelerate{};
    bool expired{};
    bool eligible{};
};

constexpr StoryCinematicSkipDecision updateStoryCinematicSkip(
    StoryCinematicSkipState& state, bool enabled, bool cutscene,
    bool gloryKill, bool chainsaw, bool ledge, bool boss, bool menu,
    bool world, bool rsDown, unsigned long long nowMs) {
    // Existing execution-tail classifier prevents treating finishing blows
    // and ledge exit frames as a brand-new story sequence.
    const bool independent=acceptIndependentScriptedCinematic(
        state.boundary,cutscene,gloryKill,chainsaw,ledge,nowMs);
    const bool scene=enabled&&independent&&!boss&&!menu&&world;
    const bool wasAccelerating=state.accelerating;
    const bool edge=rsDown&&!state.rsWasDown;
    state.rsWasDown=rsDown;

    if(!scene) {
        state.accelerating=false;
        state.sceneSinceMs=0;
        state.acceleratingSinceMs=0;
        return {false,wasAccelerating,false,false,false};
    }
    if(!state.sceneSinceMs)state.sceneSinceMs=nowMs?nowMs:1;
    // Avoid one-frame/camera-handoff false positives.
    const bool eligible=nowMs>=state.sceneSinceMs
        && nowMs-state.sceneSinceMs>=400;
    // Even when not yet eligible, never emit an unintended gameplay melee
    // while the cutscene owns the controller.
    if(edge&&eligible) {
        state.accelerating=!state.accelerating;
        state.acceleratingSinceMs=state.accelerating?nowMs:0;
        return {true,true,state.accelerating,false,true};
    }
    // Automatic restoration on unusually long sequences, including a
    // stuck cutscene classifier; another RS click may restart acceleration.
    if(state.accelerating&&nowMs>=state.acceleratingSinceMs
        &&nowMs-state.acceleratingSinceMs>=20000) {
        state.accelerating=false;
        state.acceleratingSinceMs=0;
        return {true,true,false,true,eligible};
    }
    return {true,false,state.accelerating,false,eligible};
}

} // namespace kharvox
