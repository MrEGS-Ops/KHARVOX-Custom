#include "../src/weapon/QuickHolsterPolicy.h"
#include <cassert>
#include <cmath>

int main() {
    using namespace kharvox;
    const QuickHolsterZone zone{};
    assert(quickHipInside({.27f,-.73f,.02f},false));
    assert(!quickHipInside({.27f,-.25f,.02f},false)); // chest not hip
    assert(!quickHipInside({.27f,-.73f,.65f},false)); // behind back
    assert(!quickHipInside({-.27f,-.73f,.02f},false)); // wrong hip
    assert(quickHipInside({.08f,-.73f,.02f},true)); // exit hysteresis
    assert(!quickHipInside({NAN,-.73f,.02f},true)); // invalid tracking
    QuickHolsters h{};
    assert(!quickHolsterCanStow(h,0,BackWeaponKind::Pistol));
    h.lastSelectedWheelSide=0;
    assert(quickHolsterCanStow(h,0,BackWeaponKind::Pistol));
    assert(!quickHolsterCanStow(h,1,BackWeaponKind::Pistol));
    h.slots[0]=BackWeaponKind::Pistol;
    assert(!quickHolsterCanStow(h,0,BackWeaponKind::Pistol));
    return 0;
}
