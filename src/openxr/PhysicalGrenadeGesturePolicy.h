#pragma once
#include <cmath>
#include <cstdint>

namespace kharvox {
struct PhysicalGrenadeGestureState {
    bool held{};
    bool blocked{};
    bool thrown{};
    bool swung{};
    std::int64_t lastFastNs{};
};

// True exactly once per button hold, when a deliberate swing has ended.
// Missing tracking is fail-closed. DOOM still owns trajectory and velocity.
inline bool updatePhysicalGrenadeGesture(PhysicalGrenadeGestureState& s,
    bool gameplay, bool held, bool tracked, float speed, std::int64_t now) {
    if (!gameplay || !held) {
        s = {};
        return false;
    }
    if (!s.held) {
        s.held = true;
        s.blocked = !tracked;
        return false;
    }
    if (!tracked || !std::isfinite(speed) || speed < 0.f) {
        s.blocked = true;
        s.swung = false;
        return false;
    }
    if (s.blocked || s.thrown) return false;
    if (speed >= 1.0f) {
        s.swung = true;
        s.lastFastNs = now;
    }
    if (s.swung && now >= s.lastFastNs
        && now - s.lastFastNs > 30000000LL
        && now - s.lastFastNs < 700000000LL && speed <= 0.42f) {
        s.thrown = true;
        s.swung = false;
        return true;
    }
    if (s.swung && now - s.lastFastNs >= 700000000LL)
        s.swung = false;
    return false;
}
} // namespace kharvox
