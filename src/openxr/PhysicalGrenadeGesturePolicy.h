#pragma once
#include <cstdint>

namespace kharvox {
// Quick tap INSIDE chest = cycle on release. Long hold + withdrawal = prime.
// Release OUTSIDE = throw once. Release inside after a hold = cancel.
// The throw pulse still delegates projectile spawning/physics to KHARVOX/DOOM.
enum class ChestGrenadeAction { None, Cycle, Throw };
struct PhysicalGrenadeGestureState {
    bool held{},armed{},pulled{},blockedUntilRelease{};
    std::int64_t pressStartedNs{};
};
inline ChestGrenadeAction updateChestGrenadeGesture(
    PhysicalGrenadeGestureState& state, bool gameplay, bool gripHeld,
    bool trackingValid, bool insideChest, std::int64_t now) {
    constexpr std::int64_t tapMaximumNs=260000000LL;
    if(!gameplay){state={};return ChestGrenadeAction::None;}
    if(!trackingValid){
        state.armed=false;state.pulled=false;
        state.blockedUntilRelease=gripHeld;
        state.held=gripHeld;
        return ChestGrenadeAction::None;
    }
    if(!gripHeld){
        const bool throwNow=state.held&&state.armed&&state.pulled
            &&!insideChest&&!state.blockedUntilRelease;
        const bool cycleNow=state.held&&state.armed&&!state.pulled
            &&insideChest&&!state.blockedUntilRelease
            &&now>=state.pressStartedNs
            &&now-state.pressStartedNs<=tapMaximumNs;
        state={};
        return throwNow?ChestGrenadeAction::Throw:
            cycleNow?ChestGrenadeAction::Cycle:ChestGrenadeAction::None;
    }
    if(!state.held){
        state.held=true;
        state.armed=insideChest;
        state.pulled=false;
        state.blockedUntilRelease=!insideChest;
        state.pressStartedNs=now;
        return ChestGrenadeAction::None;
    }
    if(state.armed&&!insideChest)state.pulled=true;
    return ChestGrenadeAction::None;
}
} // namespace kharvox
