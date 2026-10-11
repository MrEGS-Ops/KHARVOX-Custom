#pragma once

#include <algorithm>
#include <cstdint>

namespace kharvox {

// Haptic Overhaul: constant-intensity impact, duration proportional to
// combined health + armour loss. Pure policy: no game memory assumptions.
struct DamageHapticConfig {
    std::uint32_t millisecondsPerPoint{50};
    std::uint32_t maximumDurationMilliseconds{600};
    std::uint32_t minimumQuietMilliseconds{65};
    float amplitude{0.7f};
    float frequencyHz{160.0f};
};

struct PlayerVitalsSample {
    int health{};
    int armour{};
    bool valid{};
    bool gameplay{};
    std::uint64_t epoch{}; // Increment on respawn/map load; stable per life.
};

struct DamageHapticState {
    PlayerVitalsSample previous{};
    bool hasBaseline{};
    std::uint64_t startMilliseconds{};
    std::uint64_t activeUntilMilliseconds{};
    std::uint64_t lastPulseEndedMilliseconds{};
};

struct DamageHapticOutput {
    bool active{};
    float amplitude{};
    float frequencyHz{};
    std::uint32_t remainingMilliseconds{};
};

// Baseline after starting gameplay or changing lives. Pickups, menus,
// loading screens and invalid/uninitialized values never register as damage.
inline int detectVitalsDamage(DamageHapticState& state,
    const PlayerVitalsSample& sample) {
    if (!sample.valid || !sample.gameplay
        || sample.health < 0 || sample.health > 10000
        || sample.armour < 0 || sample.armour > 10000) {
        state.previous = {};
        state.hasBaseline = false;
        return 0;
    }
    if (!state.hasBaseline || sample.epoch != state.previous.epoch
        || (state.previous.health == 0 && sample.health > 0)) {
        state.previous = sample;
        state.hasBaseline = true;
        return 0;
    }
    const int healthLost = std::max(0, state.previous.health - sample.health);
    const int armourLost = std::max(0, state.previous.armour - sample.armour);
    state.previous = sample;
    // Health & armour lost from the same hit are additive, never double-count
    // the whole hit as both resources, and never trigger on increases.
    return healthLost + armourLost;
}

inline void resetDamageHaptics(DamageHapticState& state) {
    state = {};
}

inline std::uint32_t damagePulseDuration(int damage,
    const DamageHapticConfig& config = {}) {
    if (damage <= 0) return 0;
    const std::uint64_t total =
        static_cast<std::uint64_t>(damage) * config.millisecondsPerPoint;
    return static_cast<std::uint32_t>(std::min<std::uint64_t>(
        total, config.maximumDurationMilliseconds));
}

// A rapid second hit extends the ongoing effect, but never queues a long
// backlog or stretches one continuous vibration beyond 600 ms. After a
// saturated hit, wait for quiet before beginning another pulse.
inline DamageHapticOutput updateDamageHaptics(
    DamageHapticState& state, const PlayerVitalsSample& sample,
    std::uint64_t now, const DamageHapticConfig& config = {}) {
    const int damage = detectVitalsDamage(state, sample);
    const bool active = state.activeUntilMilliseconds > now;
    if (!sample.valid || !sample.gameplay) {
        state.activeUntilMilliseconds = 0;
        state.startMilliseconds = 0;
        return {};
    }
    if (!active && state.activeUntilMilliseconds != 0) {
        state.lastPulseEndedMilliseconds = state.activeUntilMilliseconds;
        state.activeUntilMilliseconds = 0;
    }
    if (damage > 0) {
        const auto requested = damagePulseDuration(damage, config);
        if (active) {
            const auto hardCap = state.startMilliseconds
                + config.maximumDurationMilliseconds;
            state.activeUntilMilliseconds = std::min(hardCap,
                std::max(state.activeUntilMilliseconds, now + requested));
        } else if (state.lastPulseEndedMilliseconds == 0
            || now >= state.lastPulseEndedMilliseconds
                + config.minimumQuietMilliseconds) {
            state.startMilliseconds = now;
            state.activeUntilMilliseconds = now + requested;
        }
    }
    if (state.activeUntilMilliseconds <= now) return {};
    return {true, config.amplitude, config.frequencyHz,
        static_cast<std::uint32_t>(state.activeUntilMilliseconds - now)};
}

} // namespace kharvox
