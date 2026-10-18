#pragma once
namespace kharvox {
// Release a captured ledge camera hold only on an actual cinematic-to-
// gameplay return, or after an abandoned ledge attempt has timed out in
// ordinary gameplay. Gameplay camera frames while a ledge is still active
// must NEVER unlock the story-camera yaw override.
constexpr bool storyFocusCanClearLedgeHold(bool gameplayCamera,
    bool ledgeStateActive, bool cinematicReturned, bool cutsceneActive,
    unsigned long long armedAt, unsigned long long now) {
    if(!gameplayCamera||ledgeStateActive)return false;
    if(cinematicReturned)return true;
    return !cutsceneActive&&armedAt&&now>=armedAt&&now-armedAt>=2000;
}
constexpr bool storyFocusCanUseAuthoredYaw(
    bool requested, bool immersiveHmdLook, bool world,
    bool syncAttack, bool chainsawExecution, bool ledgeClimb,
    bool ledgeCinematicHeld, bool boss) {
    return requested&&immersiveHmdLook&&world
        &&!syncAttack&&!chainsawExecution&&!ledgeClimb
        &&!ledgeCinematicHeld&&!boss;
}
}
