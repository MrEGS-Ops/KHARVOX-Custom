#include "../src/weapon/QuickHolsterPolicy.h"
#include <cassert>
#include <cmath>

int main() {
    using namespace kharvox;
    const QuickHolsterZone zone{};
    assert(quickHipInside({.27f,-.75f,.35f},false));
    assert(!quickHipInside({.27f,-.25f,.35f},false)); // chest not hip
    assert(!quickHipInside({.27f,-.75f,-.12f},false)); // forward accidental activation
    assert(!quickHipInside({.27f,-.75f,.80f},false)); // behind back
    assert(!quickHipInside({-.27f,-.75f,.35f},false)); // wrong hip
    assert(quickHipInside({.10f,-.75f,.12f},true)); // exit hysteresis
    assert(!quickHipInside({NAN,-.75f,.35f},true)); // invalid tracking
    QuickHolsters h{};
    assert(!quickHolsterCanStow(h,0,BackWeaponKind::Pistol));
    h.lastSelectedWheelSide=0;
    assert(quickHolsterCanStow(h,0,BackWeaponKind::Pistol));
    assert(!quickHolsterCanStow(h,1,BackWeaponKind::Pistol));
    assert(!quickHolsterCanStow(h,0,BackWeaponKind::GaussCannon));
    assert(!quickHolsterCanStow(h,0,BackWeaponKind::Chaingun));
    assert(!quickHolsterCanStow(h,0,BackWeaponKind::Bfg));
    h.slots[0]=BackWeaponKind::Pistol;
    assert(!quickHolsterCanStow(h,0,BackWeaponKind::Pistol)); // cannot overwrite occupied
    assert(!quickHolsterCanStow(h,0,BackWeaponKind::CombatShotgun));
    h.slots[0]=BackWeaponKind::Unknown; // after explicitly drawing it
    assert(quickHolsterCanStow(h,0,BackWeaponKind::Pistol));
    return 0;
}
