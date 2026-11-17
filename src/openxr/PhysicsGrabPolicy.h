#pragma once
#include <array>
#include <cmath>
#include <cstdint>

namespace kharvox {

// Engine adapter MUST prove this is a live, movable physics body before
// reporting a candidate. Ray proximity, display-model type, or the fact that
// a player can bump an object is NOT proof of a safe body to manipulate.
enum class PhysicsGrabKind : unsigned char { Unknown, StaticWorld, MovableProp, Enemy };
struct PhysicsGrabCandidate {
    std::uint64_t id{};
    PhysicsGrabKind kind{PhysicsGrabKind::Unknown};
    bool nativeDynamicPhysicsConfirmed{};
    bool unobstructed{};
    bool scriptedOrProtected{};
    float handDistanceMeters{};
};

struct PhysicsGrabInput {
    bool enabled{};                       // Experimental default: OFF
    bool gameplay{};
    bool controllerTracked{};
    bool gripDown{};
    bool triggerDown{};                   // Trigger from the SAME hand
    bool weaponSupportGripOwnsPress{};
    bool calibrationOwnsPress{};
    bool nativeHoldConfirmed{};           // Returned by verified engine adapter
    bool nativeHeldObjectStillDynamic{};  // Checked after grabbing, every frame
    float handDistanceFromHeadMeters{};
    PhysicsGrabCandidate candidate{};
};

enum class PhysicsGrabAction : unsigned char {
    None, Pull, Hold, Drop, Throw, Cancel
};
struct PhysicsGrabOutput {
    PhysicsGrabAction action{PhysicsGrabAction::None};
    std::uint64_t id{};
    bool ownsGrip{};
    bool ownsTrigger{};
};

struct PhysicsGrabPolicy {
    static constexpr float maxPullDistanceMeters = 0.90f;  // ~3 feet
    static constexpr float maxHeldHandFromHeadMeters = 1.35f;
    static constexpr float throwSpeedMetersPerSecond = 6.0f;
    struct Hand {
        bool gripWasDown{}, triggerWasDown{}, occupied{}, holding{}, waitForRelease{};
        std::uint64_t id{};
    };
    std::array<Hand, 2> hands{};
    bool enemyGrabbingEnabled{}; // Separate, future opt-in; stays OFF.

    bool eligible(const PhysicsGrabCandidate& c) const {
        if (!c.id || !c.nativeDynamicPhysicsConfirmed || !c.unobstructed
            || c.scriptedOrProtected || !std::isfinite(c.handDistanceMeters)
            || c.handDistanceMeters < 0.0f
            || c.handDistanceMeters > maxPullDistanceMeters) return false;
        return c.kind == PhysicsGrabKind::MovableProp
            || (enemyGrabbingEnabled && c.kind == PhysicsGrabKind::Enemy);
    }

    PhysicsGrabOutput update(unsigned handIndex, const PhysicsGrabInput& input) {
        if (handIndex >= hands.size()) return {};
        auto& h = hands[handIndex];
        auto result = [&](PhysicsGrabAction action, bool own) {
            return PhysicsGrabOutput{action, h.id, own, own};
        };
        const bool safetyReady = input.enabled && input.gameplay && input.controllerTracked
            && !input.calibrationOwnsPress
            && !input.weaponSupportGripOwnsPress
            && std::isfinite(input.handDistanceFromHeadMeters)
            && input.handDistanceFromHeadMeters <= maxHeldHandFromHeadMeters;
        if (!safetyReady) {
            const auto action = h.occupied
                ? (h.holding ? PhysicsGrabAction::Drop : PhysicsGrabAction::Cancel)
                : PhysicsGrabAction::None;
            const auto resultBeforeReset = result(action, false);
            h = {};
            h.gripWasDown = input.gripDown;
            h.triggerWasDown = input.triggerDown;
            h.waitForRelease = input.gripDown;
            return resultBeforeReset;
        }
        const bool triggerEdge = input.triggerDown && !h.triggerWasDown;
        h.gripWasDown = input.gripDown;
        h.triggerWasDown = input.triggerDown;
        if (!input.gripDown) {
            const auto action = h.occupied
                ? (h.holding ? PhysicsGrabAction::Drop : PhysicsGrabAction::Cancel)
                : PhysicsGrabAction::None;
            const auto resultBeforeReset = result(action, false);
            h = {};
            h.triggerWasDown = input.triggerDown;
            return resultBeforeReset;
        }
        if (h.waitForRelease) return {};
        if (h.occupied) {
            if (!input.nativeHeldObjectStillDynamic && h.holding) {
                const auto resultBeforeReset = result(PhysicsGrabAction::Cancel, false);
                h.occupied = false;
                h.holding = false;
                h.waitForRelease = true;
                return resultBeforeReset;
            }
            if (!h.holding && input.nativeHoldConfirmed) h.holding = true;
            if (h.holding && triggerEdge) {
                const auto resultBeforeReset = result(PhysicsGrabAction::Throw, true);
                h.occupied = false;
                h.holding = false;
                h.waitForRelease = true;
                return resultBeforeReset;
            }
            return result(h.holding ? PhysicsGrabAction::Hold : PhysicsGrabAction::Pull, true);
        }
        if (!eligible(input.candidate)) return {};
        // Two controllers may not capture the same native object.
        if (hands[1u - handIndex].occupied
            && hands[1u - handIndex].id == input.candidate.id) return {};
        h.id = input.candidate.id;
        h.occupied = true;
        h.holding = false;
        return result(PhysicsGrabAction::Pull, true);
    }
};
} // namespace kharvox
