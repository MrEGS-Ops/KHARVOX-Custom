#include "../src/hands/HandDepthPolicy.h"

#include <cassert>
#include <cmath>

int main() {
    assert(kharvox::hands::nativeWeaponDepthScale(true) == 1.f);
    assert(kharvox::hands::nativeWeaponDepthScale(false) == 0.f);
    // The child weapon may still submit DOOM's native depth factor .1.
    // In verified active gameplay with custom hands, it must match scene
    // depth=1 so a closer hand can correctly occlude the gun.
    assert(kharvox::hands::nativeWeaponChildDepth(.1f,true,true)==1.f);
    assert(kharvox::hands::nativeWeaponChildDepth(.1f,true,false)==.1f);
    assert(kharvox::hands::nativeWeaponChildDepth(.1f,false,true)==.1f);
    assert(kharvox::hands::nativeWeaponChildDepth(1.f,true,true)==1.f);
    assert(kharvox::hands::nativeWeaponChildDepth(NAN,true,true)!=1.f);
    assert(kharvox::hands::nativeWeaponChildDepth(2.f,true,true)==2.f);
    kharvox::hands::NativeWeaponDepthOverrides<2> overrides;
    float desired{};
    assert(!overrides.update(1, .1f, false, desired) && desired == .1f);
    // Actual r133 live entity values: CVar=1, root=.1, weapon prop=.1.
    assert(overrides.update(1, .1f, true, desired) && desired == 1.f);
    assert(overrides.update(2, .1f, true, desired) && desired == 1.f);
    assert(!overrides.update(1, 1.f, true, desired));
    assert(!overrides.update(3, .2f, true, desired) && desired == .2f);
    // Cinematic/tracking loss restores each entity's own original value.
    assert(overrides.update(1, 1.f, false, desired) && desired == .1f);
    assert(overrides.update(2, 1.f, false, desired) && desired == .1f);
    assert(!overrides.update(1, .1f, false, desired));
    assert(overrides.update(3, 0.f, true, desired));
    assert(overrides.update(3, 1.f, false, desired) && desired == 0.f);
    // Native updates are authoritative; do not restore stale baselines.
    assert(overrides.update(1, .1f, true, desired));
    assert(!overrides.update(1, .3f, false, desired) && desired == .3f);
    assert(overrides.update(1, .2f, true, desired));
    assert(overrides.update(1, .4f, true, desired));
    assert(overrides.update(1, 1.f, false, desired) && desired == .4f);
    assert(!overrides.update(4, 1.f, true, desired));
    assert(!overrides.update(4, 1.f, false, desired));
    assert(!overrides.update(0, .1f, true, desired));
    assert(!overrides.update(1, -1.f, true, desired));
    assert(!overrides.update(1, 2.f, true, desired));
    assert(!overrides.update(1, NAN, true, desired));
    assert(!overrides.update(1, INFINITY, true, desired));
    const auto defaultRange =
        kharvox::hands::doomSceneHandDepthRange(39.3701f);
    assert(std::fabs(defaultRange.nearMeters - 0.0254f) < 0.0001f);
    assert(defaultRange.farMeters > 1000.0f);
    assert(defaultRange.farMeters > defaultRange.nearMeters);

    const auto clampedLow = kharvox::hands::doomSceneHandDepthRange(0.0f);
    const auto clampedHigh =
        kharvox::hands::doomSceneHandDepthRange(1000.0f);
    assert(std::fabs(clampedLow.nearMeters - 0.1f) < 0.0001f);
    assert(std::fabs(clampedHigh.nearMeters - 0.005f) < 0.0001f);
    return 0;
}
