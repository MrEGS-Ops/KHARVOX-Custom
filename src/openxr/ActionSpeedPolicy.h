#pragma once
namespace kharvox {
// Overrides are scoped global engine timing, NOT per-model animation clocks.
enum class ActionSpeedKind : unsigned char { None, Chainsaw, GloryKill, LedgeClimb, GaussCharge, Scripted };
struct ActionSpeedCandidates { bool syncAttack{},chainsawKill{},ledgeClimb{},gaussCharge{},scripted{}; };
struct ActionSpeedTargets { int chainsaw{},gloryKill{},ledgeClimb{},gaussCharge{},scripted{}; };
struct ActionSpeedChoice { ActionSpeedKind kind{ActionSpeedKind::None}; int hz{}; };
constexpr bool validActionSpeedTarget(int hz) { return hz>=30&&hz<=240; }
constexpr ActionSpeedChoice selectActionSpeed(ActionSpeedCandidates a,ActionSpeedTargets t) {
    if(a.chainsawKill&&validActionSpeedTarget(t.chainsaw))return {ActionSpeedKind::Chainsaw,t.chainsaw};
    if(a.syncAttack&&!a.chainsawKill&&validActionSpeedTarget(t.gloryKill))return {ActionSpeedKind::GloryKill,t.gloryKill};
    if(a.ledgeClimb&&validActionSpeedTarget(t.ledgeClimb))return {ActionSpeedKind::LedgeClimb,t.ledgeClimb};
    if(a.gaussCharge&&validActionSpeedTarget(t.gaussCharge))return {ActionSpeedKind::GaussCharge,t.gaussCharge};
    if(a.scripted&&validActionSpeedTarget(t.scripted))return {ActionSpeedKind::Scripted,t.scripted};
    return {};
}
constexpr const char* actionSpeedName(ActionSpeedKind k) {
    switch(k) {
        case ActionSpeedKind::Chainsaw:return "chainsaw";
        case ActionSpeedKind::GloryKill:return "glory-kill";
        case ActionSpeedKind::LedgeClimb:return "ledge-climb";
        case ActionSpeedKind::GaussCharge:return "gauss-charge";
        case ActionSpeedKind::Scripted:return "scripted";
        default:return "baseline";
    }
}
constexpr unsigned actionProbeMelee=1u<<0,actionProbeReload=1u<<1,
    actionProbeGrenade=1u<<2,actionProbeEnemies=1u<<3,
    actionProbeBoss=1u<<4,actionProbeWeaponWheel=1u<<5;
} // namespace kharvox
