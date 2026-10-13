#include "../src/hud/WeaponWheelHudPolicy.h"
#include <cassert>
#include <cmath>
#include <limits>

int main() {
    using namespace kharvox;
    assert(weaponWheelDistanceMeters > .5f && weaponWheelDistanceMeters < 3.f);
    assert(weaponWheelWidthMeters > .5f && weaponWheelWidthMeters < 3.f);
    const float eye[]{0.f, 0.f, 0.f};
    // Native camera matrix rows: forward, right, up.
    const float head[]{1.f,0.f,0.f, 0.f,1.f,0.f, 0.f,0.f,1.f};
    float center[3]{};
    assert(weaponWheelPose(eye,head,1.f,center));
    assert(std::abs(center[0]-weaponWheelDistanceMeters)<.0001f);
    assert(center[1]==0.f && center[2]==0.f);
    // Native menu axes may be rotated. Their lengths remain intact, but
    // headset-facing orientation must be invariant.
    const float native[]{0.f,0.f,-2.f, 2.f,0.f,0.f, 0.f,-2.f,0.f};
    float fixed[9]{};
    assert(flatWeaponWheelAxis(native,head,fixed));
    assert(std::abs(fixed[1]-2.f)<.0001f);  // right
    assert(std::abs(fixed[5]-2.f)<.0001f);  // up
    assert(std::abs(fixed[6]-2.f)<.0001f);  // forward
    assert(fixed[0]==0.f && fixed[4]==0.f && fixed[8]==0.f);
    float degenerate[9]{};
    assert(!flatWeaponWheelAxis(degenerate,head,fixed));
    float invalidHead[9]{};
    invalidHead[0]=std::numeric_limits<float>::quiet_NaN();
    assert(!flatWeaponWheelAxis(native,invalidHead,fixed));
    return 0;
}
