#pragma once
#include <algorithm>
#include <cstdint>

namespace kharvox {

// VR input-level reload persistence, independent of Quick Holsters and the
// separate proposed dual-wield game-side mod. DOOM still owns ammo, reload
// animations, and whether a native shot was actually accepted.
//
// This policy tracks a conservative INPUT-LEVEL shot attempt: until a true
// native fired/reload-complete callback has been established it MUST be
// described as experimental, not native animation/state restoration.
inline constexpr std::uint64_t persistentSsgActiveReloadNanoseconds = 1600000000ull;
inline constexpr std::uint64_t persistentSsgFirePulseNanoseconds = 90000000ull;
inline constexpr std::uint64_t persistentSsgMaxFrameStepNanoseconds = 100000000ull;

struct PersistentSuperShotgunReloadState {
    bool reloadRequired{};
    bool triggerPreviouslyDown{};
    bool lastFrameEquipped{};
    bool replayNeededOnDraw{};
    std::uint64_t equippedProgressNanoseconds{};
    std::uint64_t lastFrameNanoseconds{};
    std::uint64_t nativeFirePulseUntilNanoseconds{};
};

struct PersistentSuperShotgunReloadInput {
    bool enabled{};
    bool gameplayActive{};
    bool superShotgunEquipped{};
    bool weaponHolstered{};
    bool weaponWheelActive{};
    bool fireRequested{};
    bool physicalTriggerHeld{};
    bool nativeAmmoKnownEmpty{};
    std::uint64_t nowNanoseconds{};
};

struct PersistentSuperShotgunReloadOutput {
    bool forwardFire{};
    bool shotAttemptAccepted{};
    bool nativeReloadWindowCompleted{};
    bool nativeReloadReplayRequested{};
    bool blockedByPersistentReload{};
    std::uint64_t remainingEquippedNanoseconds{};
};

inline PersistentSuperShotgunReloadOutput updatePersistentSuperShotgunReload(
    PersistentSuperShotgunReloadState& state,
    const PersistentSuperShotgunReloadInput& in) {
    PersistentSuperShotgunReloadOutput out{};
    // If switched OFF, pass input through untouched and clear our own state.
    if (!in.enabled) {
        state = {};
        out.forwardFire = in.fireRequested;
        return out;
    }

    const bool active = in.gameplayActive && in.superShotgunEquipped
        && !in.weaponHolstered && !in.weaponWheelActive;
    const auto now = in.nowNanoseconds;
    // An equipped Super Shotgun that is put away before its reload has
    // finished must ask the *native* hands animation system to restart
    // its regular reload on the next draw. No request during menu pauses.
    if(state.reloadRequired && in.gameplayActive
        &&(!in.superShotgunEquipped||in.weaponHolstered))
        state.replayNeededOnDraw=true;
    if(state.reloadRequired && active && !state.lastFrameEquipped
        && state.replayNeededOnDraw){
        out.nativeReloadReplayRequested=true;
        // The player asked to RESTART the original SSG reload, not resume
        // some leftover invisible delay. A cancelled animation starts over.
        state.equippedProgressNanoseconds=0;
        state.nativeFirePulseUntilNanoseconds=0;
        state.replayNeededOnDraw=false;
    }
    // Accumulate actual equipped GAMEPLAY time only. No menu, no wheel,
    // no cutscene and no gun-swapping time counts towards reloading.
    if (active && state.lastFrameEquipped
        && now >= state.lastFrameNanoseconds && state.reloadRequired) {
        const auto delta = std::min(
            now-state.lastFrameNanoseconds,
            persistentSsgMaxFrameStepNanoseconds);
        state.equippedProgressNanoseconds = std::min(
            persistentSsgActiveReloadNanoseconds,
            state.equippedProgressNanoseconds+delta);
        if (state.equippedProgressNanoseconds>=persistentSsgActiveReloadNanoseconds) {
            state.reloadRequired = false;
            state.nativeFirePulseUntilNanoseconds = 0;
            state.replayNeededOnDraw=false;
            out.nativeReloadWindowCompleted = true;
        }
    }
    state.lastFrameNanoseconds = now;
    state.lastFrameEquipped = active;

    // Require a new physical trigger edge. Holding the trigger through a
    // swap cannot initiate an unwanted shot immediately after returning.
    const bool freshPress = in.fireRequested && !state.triggerPreviouslyDown;
    // Track physical trigger *through* weapon wheel, holster and other guns,
    // so grabbing an SSG while holding Fire never grants a free shot.
    state.triggerPreviouslyDown = in.physicalTriggerHeld;
    if (!active) {
        out.remainingEquippedNanoseconds = state.reloadRequired
            ? persistentSsgActiveReloadNanoseconds -
                state.equippedProgressNanoseconds : 0;
        out.forwardFire = in.fireRequested && !in.superShotgunEquipped;
        return out;
    }

    if (freshPress && !state.reloadRequired && !in.nativeAmmoKnownEmpty) {
        state.reloadRequired = true;
        state.equippedProgressNanoseconds = 0;
        state.replayNeededOnDraw=false;
        state.nativeFirePulseUntilNanoseconds = now +
            persistentSsgFirePulseNanoseconds;
        out.shotAttemptAccepted = true;
    }
    // Provide a brief native trigger window for DOOM's independent polling.
    // The rest of the held trigger is suppressed so a weapon-switch exploit
    // cannot slip through while our own reload state is pending.
    out.forwardFire = in.fireRequested
        && now < state.nativeFirePulseUntilNanoseconds;
    out.blockedByPersistentReload = in.fireRequested && !out.forwardFire
        && (state.reloadRequired || in.nativeAmmoKnownEmpty);
    out.remainingEquippedNanoseconds = state.reloadRequired
        ? persistentSsgActiveReloadNanoseconds -
            state.equippedProgressNanoseconds : 0;
    return out;
}

} // namespace kharvox
