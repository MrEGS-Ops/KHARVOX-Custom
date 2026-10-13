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
    assert(quickHolsterCanStow(h,0,BackWeaponKind::Pistol));
    assert(quickHolsterCanStow(h,1,BackWeaponKind::Pistol)); // no wheel prerequisite
    assert(!quickHolsterCanStow(h,0,BackWeaponKind::GaussCannon));
    assert(!quickHolsterCanStow(h,0,BackWeaponKind::Chaingun));
    assert(!quickHolsterCanStow(h,0,BackWeaponKind::Bfg));
    h.slots[0]=BackWeaponKind::Pistol;
    assert(!quickHolsterCanStow(h,0,BackWeaponKind::Pistol)); // cannot overwrite occupied
    assert(!quickHolsterCanStow(h,0,BackWeaponKind::CombatShotgun));
    // A right-hand Super Shotgun is held, right hip stores Pistol: the
    // right-hand grip must DRAW the pistol, not replace it.
    assert(quickHipAction(h,0,BackWeaponKind::SuperShotgun,false)
        ==QuickHipAction::Draw);
    assert(quickHipAction(h,1,BackWeaponKind::SuperShotgun,false)
        ==QuickHipAction::Stow);
    const auto stored=h.slots[0]; // draw the pistol, not the hand's shotgun
    assert(stored==BackWeaponKind::Pistol);
    h.slots[0]=BackWeaponKind::Unknown; // only explicit draw empties it
    assert(quickHolsterCanStow(h,0,BackWeaponKind::CombatShotgun));
    assert(quickHolsterCanStow(h,0,BackWeaponKind::Pistol));
    h.slots[0]=BackWeaponKind::Pistol;
    // Cannot fake a duplicate pistol in the other hip.
    assert(!quickHolsterCanStow(h,1,BackWeaponKind::Pistol));
    assert(quickHipAction(h,0,BackWeaponKind::CombatShotgun,true)
        ==QuickHipAction::Draw);
    assert(quickHipAction(h,1,BackWeaponKind::CombatShotgun,true)
        ==QuickHipAction::None);
    return 0;
}
