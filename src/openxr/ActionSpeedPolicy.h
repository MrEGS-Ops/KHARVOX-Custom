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
constexpr bool validActionSpeedTarget(int hz) { return hz>=30&&hz<=240; }
constexpr ActionSpeedChoice selectActionSpeed(ActionSpeedCandidates a,ActionSpeedTargets t) {
    if(a.chainsawKill&&validActionSpeedTarget(t.chainsaw))return {ActionSpeedKind::Chainsaw,t.chainsaw};
    if(a.syncAttack&&!a.chainsawKill&&validActionSpeedTarget(t.gloryKill))return {ActionSpeedKind::GloryKill,t.gloryKill};
    if(a.ledgeClimb&&validActionSpeedTarget(t.ledgeClimb))
        return {ActionSpeedKind::LedgeClimb,t.ledgeClimb};
    // Passive scripted cinematic is distinct from all execution and ledge
    // states. Never fall back to this target when a higher-priority action
    // was detected but its own override is intentionally OFF.
    if(a.scriptedCinematic&&!a.syncAttack&&!a.chainsawKill&&!a.ledgeClimb
        &&validActionSpeedTarget(t.scriptedCinematic))
        return {ActionSpeedKind::ScriptedCinematic,t.scriptedCinematic};
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
