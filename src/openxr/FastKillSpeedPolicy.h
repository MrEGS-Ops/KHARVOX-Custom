#pragma once

namespace kharvox {

struct FastKillSpeedInput {
    bool syncAttackConfirmed{};
    bool chainsawCinematicConfirmed{};
    bool chainsawKillLatched{};
    bool fastGloryKillsEnabled{};
    bool fastChainsawKillsEnabled{};
    bool boxingGuardActive{};
    bool weaponWheelActive{};
    bool pauseMenu{};
    bool fullscreenMenu{};
    bool deathMenu{};
};

// >1.0x actually accelerates animations tied to DOOM's game timescale.
// The previous 'Fast' implementation used 1.0x, indistinguishable from
// DOOM's normal rate. This policy is exclusive to confirmed kill sequences.
inline float selectFastKillSpeed(const FastKillSpeedInput& in) {
    if (in.pauseMenu || in.fullscreenMenu || in.deathMenu
        || in.weaponWheelActive || in.boxingGuardActive)
        return 0.f;
    const bool chainsawSequence = in.chainsawKillLatched
        && (in.syncAttackConfirmed || in.chainsawCinematicConfirmed);
    if (chainsawSequence)
        return in.fastChainsawKillsEnabled ? 1.90f : 0.f;
    if (in.syncAttackConfirmed)
        return in.fastGloryKillsEnabled ? 1.70f : 0.f;
    return 0.f;
}

} // namespace kharvox
