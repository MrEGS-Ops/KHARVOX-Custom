#pragma once
namespace kharvox {
constexpr bool storyFocusCanUseAuthoredYaw(
    bool requested, bool immersiveHmdLook, bool world,
    bool syncAttack, bool chainsawExecution, bool ledgeClimb,
    bool ledgeCinematicHeld, bool boss) {
    return requested&&immersiveHmdLook&&world
        &&!syncAttack&&!chainsawExecution&&!ledgeClimb
        &&!ledgeCinematicHeld&&!boss;
}
}
