#pragma once

#include <cmath>
#include <cstdint>

namespace kharvox {

constexpr std::int64_t postCinematicYawMinimumHoldNanoseconds = 250000000;
constexpr std::int64_t postCinematicYawQuietNanoseconds = 500000000;
constexpr std::int64_t postCinematicYawTimeoutNanoseconds = 6000000000;
constexpr float postCinematicYawMotionThresholdDegrees = 0.05f;
constexpr int postCinematicYawStableFramesRequired = 12;

enum class PostCinematicYawGuardExit {
    None,
    Stable,
    Timeout
};

struct PostCinematicYawGuard {
    bool active{};
    std::int64_t startedAt{};
    std::int64_t lastMotionAt{};
    int stableFrames{};
    float absorbedDegrees{};
};

struct PostCinematicYawGuardUpdate {
    bool absorbBodyDelta{};
    PostCinematicYawGuardExit exit{PostCinematicYawGuardExit::None};
};

inline void armPostCinematicYawGuard(
    PostCinematicYawGuard& guard, std::int64_t displayTime) noexcept {
    guard.active = true;
    guard.startedAt = displayTime;
    guard.lastMotionAt = displayTime;
    guard.stableFrames = 0;
    guard.absorbedDegrees = 0.0f;
}

inline void resetPostCinematicYawGuard(PostCinematicYawGuard& guard) noexcept {
    guard = {};
}

inline bool cancelPostCinematicYawGuard(PostCinematicYawGuard& guard) noexcept {
    if (!guard.active) return false;
    guard.active = false;
    guard.stableFrames = 0;
    return true;
}

inline PostCinematicYawGuardUpdate updatePostCinematicYawGuard(
    PostCinematicYawGuard& guard,
    std::int64_t displayTime,
    float bodyDeltaDegrees,
    bool bodyDeltaValid) noexcept {
    PostCinematicYawGuardUpdate result{};
    if (!guard.active) return result;

    result.absorbBodyDelta = bodyDeltaValid && std::isfinite(bodyDeltaDegrees);
    if (result.absorbBodyDelta) {
        guard.absorbedDegrees += bodyDeltaDegrees;
        if (std::abs(bodyDeltaDegrees) > postCinematicYawMotionThresholdDegrees) {
            guard.lastMotionAt = displayTime;
            guard.stableFrames = 0;
        } else {
            ++guard.stableFrames;
        }
    } else {
        guard.stableFrames = 0;
    }

    const std::int64_t elapsed = displayTime > guard.startedAt
        ? displayTime - guard.startedAt : 0;
    const std::int64_t quiet = displayTime > guard.lastMotionAt
        ? displayTime - guard.lastMotionAt : 0;
    if (elapsed >= postCinematicYawTimeoutNanoseconds) {
        guard.active = false;
        result.exit = PostCinematicYawGuardExit::Timeout;
    } else if (elapsed >= postCinematicYawMinimumHoldNanoseconds
        && quiet >= postCinematicYawQuietNanoseconds
        && guard.stableFrames >= postCinematicYawStableFramesRequired) {
        guard.active = false;
        result.exit = PostCinematicYawGuardExit::Stable;
    }
    return result;
}

// Native DOOM auto body-follow is 600 degrees/second. Immediately after
// a kill, the yaw guard ends at ~500ms and this native turn can look like
// an unwanted Meta-view recenter. Do not start an automatic correction while
// the player is running away. Once locomotion stops, wait briefly and catch
// the body up slowly. Intentional right-stick turning bypasses this policy.
constexpr std::int64_t postCinematicBodyTurnIdleWaitNanoseconds = 250000000;
constexpr float postCinematicBodyTurnRecoveryDegreesPerSecond = 90.0f;

struct PostCinematicBodyTurnRecovery {
    bool active{};
    std::int64_t idleSince{};
};

inline void armPostCinematicBodyTurnRecovery(
    PostCinematicBodyTurnRecovery& recovery) noexcept {
    recovery.active=true;
    recovery.idleSince=0;
}

inline void resetPostCinematicBodyTurnRecovery(
    PostCinematicBodyTurnRecovery& recovery) noexcept {
    recovery={};
}

inline bool suppressAutomaticBodyTurnAfterCinematic(
    PostCinematicBodyTurnRecovery& recovery,
    std::int64_t now,
    bool locomotionStickActive,
    bool manualTurnActive,
    bool nativeYawGuardActive) noexcept {
    if(!recovery.active)return false;
    if(manualTurnActive){resetPostCinematicBodyTurnRecovery(recovery);return false;}
    if(nativeYawGuardActive || locomotionStickActive){
        recovery.idleSince=0;
        return true;
    }
    if(recovery.idleSince<=0||now<recovery.idleSince){
        recovery.idleSince=now;
        return true;
    }
    return now-recovery.idleSince < postCinematicBodyTurnIdleWaitNanoseconds;
}

} // namespace kharvox
