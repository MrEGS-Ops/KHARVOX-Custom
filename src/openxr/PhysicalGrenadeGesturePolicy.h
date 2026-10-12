#pragma once
#include <cstdint>

namespace kharvox {

// One non-weapon-hand Grip has three distinct actions:
//  1. Grip edge INSIDE chest pocket => cycle grenade and arm.
//  2. Pull hand OUT while still gripping => grenade pin pulled.
//  3. Release OUTSIDE => one equipment throw pulse.
// Releasing inside cancels. Lost tracking cancels, never throws.
enum class ChestGrenadeAction { None, Cycle, Throw };
struct PhysicalGrenadeGestureState {
    bool held{};
    bool armed{};
    bool pulled{};
    bool blockedUntilRelease{};
};

inline ChestGrenadeAction updateChestGrenadeGesture(
    PhysicalGrenadeGestureState& state,
    bool gameplay, bool gripHeld, bool trackingValid, bool insideChest) {
    if (!gameplay) { state={}; return ChestGrenadeAction::None; }
    if (!trackingValid) {
        state.armed=false; state.pulled=false;
        state.blockedUntilRelease=gripHeld;
        state.held=gripHeld;
        return ChestGrenadeAction::None;
    }
    if (!gripHeld) {
        const bool shouldThrow=state.held&&state.armed&&state.pulled
            &&!insideChest&&!state.blockedUntilRelease;
        state={};
        return shouldThrow?ChestGrenadeAction::Throw:ChestGrenadeAction::None;
    }
    if (!state.held) {
        state.held=true;
        state.armed=insideChest;
        state.pulled=false;
        state.blockedUntilRelease=!insideChest;
        return insideChest?ChestGrenadeAction::Cycle:ChestGrenadeAction::None;
    }
    if (state.armed&&!insideChest)state.pulled=true;
    return ChestGrenadeAction::None;
}
} // namespace kharvox
