#pragma once
#include "BackWeaponPolicy.h"
#include <array>
#include <cmath>

namespace kharvox {

// Physical hip placements are deliberately distinct from the legacy
// behind-the-shoulder Back Weapon zone. Measurements are HMD-relative metres.
struct QuickHolsterZone {
    BackWeaponZoneBounds enter{0.10f,0.53f,-1.07f,-0.38f,-0.26f,0.33f};
    BackWeaponZoneBounds exit{0.04f,0.63f,-1.17f,-0.28f,-0.34f,0.43f};
};

struct QuickHolsters {
    std::array<BackWeaponKind,2> slots{
        BackWeaponKind::Unknown,BackWeaponKind::Unknown};
    int activeStowedSide{-1}; // -1 = weapon in hand, 0 = right hip, 1 = left hip
    int lastSelectedWheelSide{-1};
};

inline bool quickHipInside(const BackWeaponVec3& point, bool previouslyInside) {
    if (!std::isfinite(point.x)||!std::isfinite(point.y)||!std::isfinite(point.z))
        return false;
    const QuickHolsterZone policy{};
    return backWeaponPointInside(point,previouslyInside?policy.exit:policy.enter);
}

inline bool quickHolsterCanStow(const QuickHolsters& state, int side,
    BackWeaponKind equipped) {
    return side>=0&&side<=1
        &&state.lastSelectedWheelSide==side
        &&equipped!=BackWeaponKind::Unknown;
}
}
