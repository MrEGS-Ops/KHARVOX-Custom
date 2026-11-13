#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace kharvox::hands {

// Preserve the established guns-only CVar value. This CVar alone is NOT
// sufficient: native render entities cache their own depth scale at +0x14C.
inline float nativeWeaponDepthScale(bool showHands) {
    return showHands ? 1.0f : 0.0f;
}

// Keep weapon child/FX render depth in the same world-depth convention
// as the custom hand scene pass. Native DOOM may pass a separate 0.1 depth
// factor to its animated weapon child even when its main entity has been
// corrected to 1. Restrict to the verified first-person weapon caller in
// active gameplay. Do not alter walls, levels, user hand calibration or
// any non-weapon render object.
inline float nativeWeaponChildDepth(float original,
    bool verifiedGameplayWeapon,bool customHandsActive) noexcept {
    return verifiedGameplayWeapon&&customHandsActive
        &&std::isfinite(original)&&original>=0.f&&original<=1.f
        ?1.f:original;
}

// Own only overrides applied at a recognized native weapon update. Never
// dereference saved entity keys: restoration happens when that same entity is
// handed to us again by DOOM. A native rewrite/address reuse replaces the
// baseline; an inactive context must not overwrite a new native value.
template<std::size_t Capacity = 128>
class NativeWeaponDepthOverrides {
    struct Entry { std::uintptr_t entity{}; float original{}; };
    std::array<Entry, Capacity> entries_{};
public:
    bool update(std::uintptr_t entity, float current, bool active, float& desired) {
        desired = current;
        if (!entity || !std::isfinite(current) || current < 0.f || current > 1.f)
            return false;
        Entry* found{};
        Entry* free{};
        for (auto& entry : entries_) {
            if (entry.entity == entity) { found = &entry; break; }
            if (!entry.entity && !free) free = &entry;
        }
        if (!active) {
            if (!found) return false;
            if (current == 1.f) desired = found->original;
            *found = {};
            return desired != current;
        }
        if (current == 1.f) return false;
        if (!found) found = free;
        if (!found) return false; // bounded storage: do not make an unowned edit
        *found = {entity, current};
        desired = 1.f;
        return true;
    }
};

struct HandDepthRange {
    float nearMeters{0.02f};
    float farMeters{100.0f};
};

// KHARVOX launches DOOM with r_znear=1 (was native 3 units) to preserve
// weapon surfaces closer to the HMD. Hand geometry is expressed in OpenXR
// metres: its shared-depth path MUST match that same scene near plane after
// applying KHARVOX world scale. Do not change this value in isolation.
// A zero native far plane means infinity; a very distant finite plane gives
// the same stable Vulkan depth curve without producing infinities in the CPU
// matrix. The isolated OpenXR overlay deliberately retains its old range.
inline HandDepthRange doomSceneHandDepthRange(float worldUnitsPerMeter) {
    constexpr float nativeNearUnits = 1.0f;
    constexpr float effectivelyInfiniteFarUnits = 1000000.0f;
    const float scale = std::clamp(worldUnitsPerMeter, 10.0f, 200.0f);
    return {nativeNearUnits / scale, effectivelyInfiniteFarUnits / scale};
}

} // namespace kharvox::hands
