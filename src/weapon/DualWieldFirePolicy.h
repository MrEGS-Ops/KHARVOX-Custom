#pragma once
#include "BackWeaponPolicy.h"

namespace kharvox {

// Pure policy for a proposed conventional DOOM gameplay dual-wield mod.
// Does NOT install engine hooks or simulate ammunition/reload effects.
enum class DualFireChannel { Primary, Alternate };

inline bool dualWieldEligibleWeapon(BackWeaponKind weapon) {
    switch (weapon) {
        case BackWeaponKind::Unknown:
        case BackWeaponKind::GaussCannon:
        case BackWeaponKind::Chaingun:
        case BackWeaponKind::Bfg:
        case BackWeaponKind::RocketLauncher:
        case BackWeaponKind::Chainsaw:
            return false;
        default:
            return true;
    }
}

inline bool dualWieldPairAllowed(BackWeaponKind left, BackWeaponKind right) {
    return left != right && dualWieldEligibleWeapon(left)
        && dualWieldEligibleWeapon(right);
}

// Firing each Combat Shotgun channel once is intentionally independent.
// Alternate means ONE COMPLETE native mod activation, e.g. the full Charged
// Burst sequence, not one pellet/sub-shot. Only commit after native shot ack.
struct DualCombatShotgunFireState {
    bool primarySpent{};
    bool alternateSpent{};
    bool reloadPending{};
};

inline bool canDualFireCombatShotgun(const DualCombatShotgunFireState& state,
    DualFireChannel channel, bool nativeAmmoAvailable, bool modEquipped) {
    if (!nativeAmmoAvailable || state.reloadPending) return false;
    if (channel == DualFireChannel::Primary) return !state.primarySpent;
    return modEquipped && !state.alternateSpent;
}

inline void nativeDualCombatShotgunShotAccepted(
    DualCombatShotgunFireState& state, DualFireChannel channel) {
    if (channel == DualFireChannel::Primary) state.primarySpent = true;
    else state.alternateSpent = true;
    if (state.primarySpent && state.alternateSpent)
        state.reloadPending = true;
}

inline bool dualCombatShotgunCanReload(
    const DualCombatShotgunFireState& state, bool secondGunHeld) {
    // Once the other hand releases its weapon, native shotgun reload/cycle
    // is permitted; actual ammo/animation state is still decided by DOOM.
    return !secondGunHeld && (state.primarySpent || state.alternateSpent);
}

inline void nativeDualCombatShotgunReloadCompleted(
    DualCombatShotgunFireState& state) {
    state = {};
}

} // namespace kharvox
