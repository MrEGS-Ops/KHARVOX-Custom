#pragma once
namespace kharvox {
// Overrides are scoped global engine timing, NOT per-model animation clocks.
enum class ActionSpeedKind : unsigned char { None, Chainsaw, GloryKill, LedgeClimb };
struct ActionSpeedCandidates { bool syncAttack{},chainsawKill{},ledgeClimb{}; };
struct ActionSpeedTargets { int chainsaw{},gloryKill{},ledgeClimb{}; };
struct ActionSpeedChoice { ActionSpeedKind kind{ActionSpeedKind::None}; int hz{}; };
constexpr bool validActionSpeedTarget(int hz) { return hz>=30&&hz<=240; }
constexpr ActionSpeedChoice selectActionSpeed(ActionSpeedCandidates a,ActionSpeedTargets t) {
    if(a.chainsawKill&&validActionSpeedTarget(t.chainsaw))return {ActionSpeedKind::Chainsaw,t.chainsaw};
    if(a.syncAttack&&!a.chainsawKill&&validActionSpeedTarget(t.gloryKill))return {ActionSpeedKind::GloryKill,t.gloryKill};
    if(a.ledgeClimb&&validActionSpeedTarget(t.ledgeClimb))return {ActionSpeedKind::LedgeClimb,t.ledgeClimb};
    return {};
}
constexpr const char* actionSpeedName(ActionSpeedKind k) {
    switch(k) {
        case ActionSpeedKind::Chainsaw:return "chainsaw";
        case ActionSpeedKind::GloryKill:return "glory-kill";
        case ActionSpeedKind::LedgeClimb:return "ledge-climb";
        default:return "baseline";
    }
}
} // namespace kharvox
