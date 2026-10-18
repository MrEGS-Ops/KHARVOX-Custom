#pragma once
namespace kharvox {
constexpr bool storyFocusCanUseAuthoredYaw(
    bool requested, bool immersiveHmdLook, bool world,
    bool syncAttack, bool chainsawExecution, bool ledgeClimb, bool boss,
    bool recentActionTail = false) {
    return requested&&immersiveHmdLook&&world
        &&!syncAttack&&!chainsawExecution&&!ledgeClimb&&!boss
        &&!recentActionTail;
}
}

// Preserve the user's Focus preference while temporarily overriding its
// effective state through a native execution/ledge and its camera-return tail.
// A mere fall of sync/ledge flags MUST NOT re-enable Focus.
struct StoryCameraFocusAutoSuspend {
    bool suspended{};
    constexpr bool update(bool actionActive, bool nativeCutsceneActive,
                          bool nativeGameplayCameraReturned) noexcept {
        if (actionActive) suspended=true;
        else if (suspended&&!nativeCutsceneActive&&nativeGameplayCameraReturned)
            suspended=false;
        return suspended;
    }
};
