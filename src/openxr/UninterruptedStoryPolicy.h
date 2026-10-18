#pragma once
#include "ActionSpeedPolicy.h"
#include <cstdint>

namespace kharvox {

// Noncombat story hold: opt-in auto-release of *verified* movement-only
// inhibition, without changing audio, native script clocks, interaction
// callbacks, door triggers, camera pose, or objective state.
struct UninterruptedStoryState {
    ScriptedCinematicBoundaryState boundary{};
    std::uint64_t sceneStartMs{};
    std::uint64_t autoStartMs{};
    bool active{};
    bool timedOut{};
};

struct UninterruptedStoryDecision {
    bool eligible{};
    bool active{};
    bool entered{};
    bool exited{};
    bool expired{};
};

// Allow authored interactions to start before attempting to give the body
// control. Restores DOOM ownership after exit, combat, menu, or a bounded
// duration even when DOOM's generic cinematic flag sticks.
constexpr UninterruptedStoryDecision updateUninterruptedStory(
    UninterruptedStoryState& state, bool enabled, bool cutscene,
    bool gloryKill, bool chainsaw, bool ledge, bool boss, bool menu,
    bool world, bool tracking, std::uint64_t nowMs) {
    const bool independent=acceptIndependentScriptedCinematic(
        state.boundary,cutscene,gloryKill,chainsaw,ledge,nowMs);
    const bool candidate=enabled&&independent&&world&&tracking
        &&!boss&&!menu;
    const bool previous=state.active;
    if(!candidate) {
        state.active=false;
        state.sceneStartMs=0;
        state.autoStartMs=0;
        state.timedOut=false;
        return {false,false,false,previous,false};
    }
    if(!state.sceneStartMs) state.sceneStartMs=nowMs?nowMs:1;
    if(state.timedOut)return {false,false,false,previous,false};
    const bool ready=nowMs>=state.sceneStartMs
        &&nowMs-state.sceneStartMs>=900;
    if(ready&&!state.active) {
        state.active=true;
        state.autoStartMs=nowMs;
        return {true,true,true,false,false};
    }
    if(state.active && nowMs>=state.autoStartMs
        &&nowMs-state.autoStartMs>=45000) {
        state.active=false;
        state.timedOut=true;
        return {false,false,false,true,true};
    }
    return {ready,state.active,false,false,false};
}

constexpr bool shouldRequestStoryMovement(
    bool active, bool movingStick, bool playerRecordVerified) {
    return active&&movingStick&&playerRecordVerified;
}

} // namespace kharvox
