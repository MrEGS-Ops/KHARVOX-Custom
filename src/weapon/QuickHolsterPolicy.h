#pragma once
#include "BackWeaponPolicy.h"
#include <array>
#include <cmath>

namespace kharvox {

// Physical hip placements are deliberately distinct from the legacy
// behind-the-shoulder Back Weapon zone. Measurements are HMD-relative metres.
struct QuickHolsterZone {
    BackWeaponZoneBounds enter{0.12f,0.51f,-1.04f,-0.48f,0.02f,0.43f};
    BackWeaponZoneBounds exit{0.07f,0.60f,-1.14f,-0.38f,-0.05f,0.53f};
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

// Full-size weapons are never eligible for invisible hip storage.
inline bool quickHolsterEligible(BackWeaponKind equipped) {
    return equipped!=BackWeaponKind::Unknown
        &&equipped!=BackWeaponKind::GaussCannon
        &&equipped!=BackWeaponKind::Chaingun
        &&equipped!=BackWeaponKind::Bfg;
}

// The slot is a persistent inventory assignment, NOT an equip shortcut.
// A draw from an occupied slot may change the active weapon, but must not
// replace the stored identity with the weapon already in the player's hand.
inline bool quickHolsterCanStow(const QuickHolsters& state, int side,
    BackWeaponKind equipped) {
    return side>=0&&side<=1
        &&state.lastSelectedWheelSide==side
        &&state.slots[side]==BackWeaponKind::Unknown
        &&quickHolsterEligible(equipped);
}
}
