#pragma once

#include <algorithm>
#include <cstdint>
#include <cmath>

namespace kharvox {

// OpenXR reports the compositor's nominal display period even when the game
// misses every other display interval. Retain the fastest plausible period
// seen in the session so a temporary 60 fps cinematic cannot disguise a
// 120 Hz headset as a 60 Hz device.
constexpr std::int64_t minimumPlausibleDisplayPeriodNs = 5'000'000;
constexpr std::int64_t maximumPlausibleDisplayPeriodNs = 20'000'000;

constexpr std::int64_t selectFastestPlausibleDisplayPeriod(
    std::int64_t current,
    std::int64_t candidate) {
    if (candidate < minimumPlausibleDisplayPeriodNs
        || candidate > maximumPlausibleDisplayPeriodNs) return current;
    return current == 0 ? candidate : std::min(current, candidate);
}

inline int targetGameHzForDisplayPeriod(std::int64_t periodNs) {
    if (periodNs <= 0) return 0;
    const auto rounded = static_cast<int>(std::lround(1'000'000'000.0 / periodNs));
    return std::clamp(rounded, 60, 200);
}

constexpr bool shouldHoldImmersiveRefresh(
    bool immersiveMode,
    bool quadMode,
    bool nativeCutsceneActive,
    bool upgradeTransitionGuardActive,
    bool nativeAdaptiveParticipantGuardActive) {
    return immersiveMode && !quadMode
        && (nativeCutsceneActive || upgradeTransitionGuardActive
            || nativeAdaptiveParticipantGuardActive);
}

constexpr unsigned long long updateNativeAdaptiveParticipantGuardUntil(
    bool eligible,
    bool participantObserved,
    unsigned long long now,
    unsigned long long previousUntil,
    unsigned long long releaseGraceMilliseconds) {
    if (!eligible) return 0;
    if (participantObserved) {
        const auto remaining = ~0ull - now;
        return remaining < releaseGraceMilliseconds
            ? ~0ull
            : now + releaseGraceMilliseconds;
    }
    return previousUntil && now <= previousUntil ? previousUntil : 0;
}

constexpr bool nativeAdaptiveParticipantGuardIsActive(
    unsigned long long now,
    unsigned long long until) {
    return until && now <= until;
}

// A confirmed 2026-10-05 working fast Glory Kill launch uses
// sync_noAdaptiveTick=1. The immersive-cinematic refresh hold used to
// overwrite it with 0 every Present and re-enable DOOM's sync slow motion.
// Preserve the original slow-cinematic behavior when fast kills are OFF,
// but NEVER clear the opt-in native bypass while either fast-kill mod is ON.
// The launcher can activate the exact Oct 5 native no-slowmo option from
// either custom checkbox OR the older Glory Kill Speed selector. Keep the
// OpenXR present path aligned with the launcher command-line policy.
constexpr bool nativeFastKillSyncRequested(
    bool fastGlory, bool fastChainsaw, int legacySpeedLevel,
    int cinematicSpeedLevel = 1) {
    return fastGlory || fastChainsaw
        || (legacySpeedLevel >= 0 && legacySpeedLevel < 10)
        || (cinematicSpeedLevel >= 2 && cinematicSpeedLevel <= 5);
}

// Existing native adaptive-tick Hz lever. Level 1 leaves the normal
// immersive slow cinematic target untouched. Level 5 is the exact, proven
// #473 bypass: zero forced tick Hz. The middle values are experimental.
constexpr int cinematicKillTargetTickHz(int level, int headsetTargetHz) {
    return level == 2 ? 90 : level == 3 ? 70 : level == 4 ? 50
        : level == 5 ? 0 : headsetTargetHz;
}

// The headset's 120 Hz *display period* is NOT the game's achieved frame
// rate. October 15 headset logs show immersive executions actually present
// at ~37-39 Hz. Forcing the game's minimum simulation/adaptive tick Hz to 120
// during these events can stretch fixed-tick animation time by several times.
// Fast executions must retain DOOM's OWN native tick pacing (usually min=30)
// while sync_noAdaptiveTick is kept at 1. Do not disable the OpenXR renderer
// or affect the native weapon-wheel slow motion.
constexpr bool shouldForceCinematicAdaptiveTickPacing(
    bool fastKillsEnabled, int cinematicSpeedLevel = 1) {
    // Do not touch the confirmed #473 full-speed path (level 5). Middle
    // positions use precisely the original immersive-refresh CVar pathway.
    return !fastKillsEnabled
        || (cinematicSpeedLevel >= 2 && cinematicSpeedLevel <= 4);
}

constexpr int nativeSyncAdaptiveTickDuringCinematic(bool fastKillsEnabled) {
    return fastKillsEnabled ? 1 : 0;
}

constexpr int nativeSyncAdaptiveTickAfterCinematic(
    bool fastKillsEnabled,int priorValue) {
    return fastKillsEnabled ? 1 : priorValue;
}

} // namespace kharvox
