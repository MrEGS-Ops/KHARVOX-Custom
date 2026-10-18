#pragma once
#include "ActionSpeedPolicy.h"

namespace kharvox {

// Distinct from the 4x story-skip experiment. A user explicitly opts into
// requesting normal left-stick locomotion while DOOM's story keeps running.
// Input gate is intentionally narrow: no kill, chainsaw, ledge or boss scene.
struct StoryFreeWalkState {
    ScriptedCinematicBoundaryState boundary{};
    unsigned long long sceneSinceMs{};
    unsigned long long enabledSinceMs{};
    bool rsWasDown{};
    bool requested{};
};
struct StoryFreeWalkDecision {
    bool eligible{};
    bool changed{};
    bool requested{};
    bool expired{};
};
constexpr StoryFreeWalkDecision updateStoryFreeWalk(
    StoryFreeWalkState& state, bool enabled, bool cutscene,
    bool glory, bool chainsaw, bool ledge, bool boss, bool menu,
    bool world, bool rsDown, unsigned long long nowMs) {
    const bool independent=acceptIndependentScriptedCinematic(
        state.boundary,cutscene,glory,chainsaw,ledge,nowMs);
    const bool allowed=enabled&&independent&&!boss&&!menu&&world;
    const bool edge=rsDown&&!state.rsWasDown;
    state.rsWasDown=rsDown;
    const bool old=state.requested;
    if(!allowed) {
        state.requested=false;
        state.sceneSinceMs=0;
        state.enabledSinceMs=0;
        return {false,old,false,false};
    }
    if(!state.sceneSinceMs)state.sceneSinceMs=nowMs?nowMs:1;
    const bool eligible=nowMs>=state.sceneSinceMs
        &&nowMs-state.sceneSinceMs>=400;
    if(eligible&&edge) {
        state.requested=!old;
        state.enabledSinceMs=state.requested?nowMs:0;
        return {true,true,state.requested,false};
    }
    // Maximum duration of one free-walk request. Do not keep a player
    // control override armed forever if the engine's scene flag sticks.
    if(old&&nowMs>=state.enabledSinceMs
        &&nowMs-state.enabledSinceMs>=30000) {
        state.requested=false;
        state.enabledSinceMs=0;
        return {eligible,true,false,true};
    }
    return {eligible,false,state.requested,false};
}

// Only horizontal movement inhibit bits are ever candidates for removal.
// This idTech flag mapping is independently documented but must still be
// corroborated against a real DOOM 2016 runtime baseline before any write.
constexpr unsigned storyHorizontalInhibitMask=0x03;
constexpr bool storyHorizontalWriteAllowed(
    bool validatedField, bool samePlayer, bool inStory,
    bool baselineKnown, unsigned baselineFlags, unsigned currentFlags) {
    return validatedField&&samePlayer&&inStory&&baselineKnown
        &&(baselineFlags&storyHorizontalInhibitMask)==0
        &&(currentFlags&storyHorizontalInhibitMask)!=0;
}

} // namespace kharvox
