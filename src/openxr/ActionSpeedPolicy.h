#pragma once
namespace kharvox {
// Overrides are scoped global engine timing, NOT per-model animation clocks.
enum class ActionSpeedKind : unsigned char {
    None, Chainsaw, GloryKill, LedgeClimb, ScriptedCinematic
};
struct ActionSpeedCandidates {
    bool syncAttack{}, chainsawKill{}, ledgeClimb{}, scriptedCinematic{};
};
struct ActionSpeedTargets {
    int chainsaw{}, gloryKill{}, ledgeClimb{}, scriptedCinematic{};
};
struct ActionSpeedChoice { ActionSpeedKind kind{ActionSpeedKind::None}; int hz{}; };
// DOOM keeps its generic cutscene flag alive after a Glory Kill, chainsaw
// execution or ledge transition ends. That tail is NOT a new scripted scene.
// Do not let the optional scripted-cinematic Hz override steal those frames.
// Require the old cutscene to close, followed by a short classification gap.
// This is only a detection guard: it adds NO delay to the game or VR camera.
struct ScriptedCinematicBoundaryState {
    bool awaitingPreviousCutsceneEnd{};
    unsigned long long allowNewSceneAfterMs{};
};
constexpr unsigned long long safeScriptedGateDeadline(
    unsigned long long now, unsigned long long graceMs) {
    return (~0ull-now)<graceMs ? ~0ull : now+graceMs;
}
constexpr bool acceptIndependentScriptedCinematic(
    ScriptedCinematicBoundaryState& state, bool cutscene,
    bool gloryKill, bool chainsaw, bool ledge,
    unsigned long long nowMs, unsigned long long graceMs=250) {
    if(gloryKill||chainsaw||ledge) {
        state.awaitingPreviousCutsceneEnd=true;
        state.allowNewSceneAfterMs=safeScriptedGateDeadline(nowMs,graceMs);
        return false;
    }
    if(state.awaitingPreviousCutsceneEnd) {
        if(!cutscene) {
            state.awaitingPreviousCutsceneEnd=false;
            state.allowNewSceneAfterMs=safeScriptedGateDeadline(nowMs,graceMs);
        }
        return false;
    }
    return cutscene&&nowMs>=state.allowNewSceneAfterMs;
}
constexpr bool validActionSpeedTarget(int hz) { return hz>=30&&hz<=240; }
constexpr ActionSpeedChoice selectActionSpeed(ActionSpeedCandidates a,ActionSpeedTargets t) {
    if(a.chainsawKill&&validActionSpeedTarget(t.chainsaw))return {ActionSpeedKind::Chainsaw,t.chainsaw};
    if(a.syncAttack&&!a.chainsawKill&&validActionSpeedTarget(t.gloryKill))return {ActionSpeedKind::GloryKill,t.gloryKill};
    if(a.ledgeClimb&&validActionSpeedTarget(t.ledgeClimb))
        return {ActionSpeedKind::LedgeClimb,t.ledgeClimb};
    // Scripted story scenes keep original DOOM timing unconditionally.
    // The classifier remains available to RS skip diagnostics, but must
    // never create a gameplay engine tick override, even with stale values.
    return {};
}
constexpr const char* actionSpeedName(ActionSpeedKind k) {
    switch(k) {
        case ActionSpeedKind::Chainsaw:return "chainsaw";
        case ActionSpeedKind::GloryKill:return "glory-kill";
        case ActionSpeedKind::LedgeClimb:return "ledge-climb";
        case ActionSpeedKind::ScriptedCinematic:return "scripted-cinematic";
        default:return "baseline";
    }
}
} // namespace kharvox
