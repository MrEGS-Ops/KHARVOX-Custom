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

// The Super Shotgun's reload state belongs to its INVENTORY ID, not the
// hand or equip animation. Stowing, dropping or drawing it cannot reload it.
struct DualSuperShotgunFireState {
    bool reloadRequired{};
    bool reloadInProgress{};
};

inline bool dualSuperShotgunCanFire(
    const DualSuperShotgunFireState& state, bool nativeAmmoAvailable) {
    return nativeAmmoAvailable && !state.reloadRequired;
}

inline void nativeDualSuperShotgunShotAccepted(
    DualSuperShotgunFireState& state) {
    state.reloadRequired = true;
    state.reloadInProgress = false;
}

inline bool dualSuperShotgunCanReload(
    const DualSuperShotgunFireState& state, bool superShotgunInHand,
    bool otherHandFree, bool nativeAmmoAvailable) {
    // Dropping the SSG itself does nothing. It must remain in hand while
    // the OTHER hand is empty, with ammunition available.
    return state.reloadRequired && superShotgunInHand && otherHandFree
        && nativeAmmoAvailable;
}

// Hand changes cancel an unfinished reload, but never reset ammo/readiness.
inline void dualSuperShotgunUpdateReloadHands(
    DualSuperShotgunFireState& state, bool superShotgunInHand,
    bool otherHandFree) {
    if (!superShotgunInHand || !otherHandFree)
        state.reloadInProgress = false;
}

inline bool dualSuperShotgunBeginReload(
    DualSuperShotgunFireState& state, bool superShotgunInHand,
    bool otherHandFree, bool nativeAmmoAvailable) {
    if (!dualSuperShotgunCanReload(state, superShotgunInHand,
            otherHandFree, nativeAmmoAvailable))
        return false;
    state.reloadInProgress = true;
    return true;
}

// A native completion acknowledgement only succeeds if reload was started
// and both hand constraints REMAIN true. Native DOOM owns shell accounting.
inline bool nativeDualSuperShotgunReloadCompleted(
    DualSuperShotgunFireState& state, bool superShotgunInHand,
    bool otherHandFree, bool nativeAmmoAvailable) {
    if (!state.reloadInProgress ||
        !dualSuperShotgunCanReload(state, superShotgunInHand,
            otherHandFree, nativeAmmoAvailable))
        return false;
    state = {};
    return true;
}

} // namespace kharvox
