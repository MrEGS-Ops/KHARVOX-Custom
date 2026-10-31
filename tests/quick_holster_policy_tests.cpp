#include "../src/weapon/QuickHolsterPolicy.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cmath>
int main(){
    using namespace kharvox;
    assert(quickHipInside({.27f,-.75f,.35f},false));
    assert(!quickHipInside({.27f,-.25f,.35f},false));
    assert(!quickHipInside({.27f,-.75f,-.12f},false));
    assert(!quickHipInside({.27f,-.75f,.80f},false));
    assert(!quickHipInside({-.27f,-.75f,.35f},false));
    assert(quickHipInside({.10f,-.75f,.12f},true));
    assert(!quickHipInside({NAN,-.75f,.35f},true));
    QuickHolsters h{};
    assert(updateQuickHip(h,0,true,false,BackWeaponKind::Pistol)==QuickHipAction::Enter);
    assert(updateQuickHip(h,0,false,true,BackWeaponKind::Pistol)==QuickHipAction::None);
    assert(updateQuickHip(h,0,true,true,BackWeaponKind::SuperShotgun)==QuickHipAction::Enter);
    assert(updateQuickHip(h,0,true,false,BackWeaponKind::SuperShotgun)==QuickHipAction::Assign);
    assert(h.slots[0]==BackWeaponKind::SuperShotgun);
    assert(updateQuickHip(h,0,true,false,BackWeaponKind::Pistol)==QuickHipAction::None);
    assert(updateQuickHip(h,0,false,false,BackWeaponKind::Pistol)==QuickHipAction::None);
    assert(updateQuickHip(h,0,true,false,BackWeaponKind::Pistol)==QuickHipAction::Enter);
    assert(updateQuickHip(h,0,true,true,BackWeaponKind::Pistol)==QuickHipAction::None);
    assert(updateQuickHip(h,0,false,true,BackWeaponKind::Pistol)==QuickHipAction::Draw);
    assert(h.slots[0]==BackWeaponKind::SuperShotgun);
    assert(updateQuickHip(h,0,true,false,BackWeaponKind::Pistol)==QuickHipAction::Enter);
    assert(updateQuickHip(h,0,false,false,BackWeaponKind::Pistol)==QuickHipAction::None);
    assert(updateQuickHip(h,0,true,true,BackWeaponKind::Pistol)==QuickHipAction::Enter);
    assert(updateQuickHip(h,0,false,true,BackWeaponKind::Pistol)==QuickHipAction::None);
    assert(updateQuickHip(h,1,true,true,BackWeaponKind::SuperShotgun)==QuickHipAction::Enter);
    assert(updateQuickHip(h,1,true,false,BackWeaponKind::SuperShotgun)==QuickHipAction::Assign);
    assert(updateQuickHip(h,1,false,false,BackWeaponKind::SuperShotgun)==QuickHipAction::None);
    assert(h.slots[1]==BackWeaponKind::SuperShotgun);
    assert(updateQuickHip(h,0,true,true,BackWeaponKind::GaussCannon)==QuickHipAction::Enter);
    assert(updateQuickHip(h,0,true,false,BackWeaponKind::GaussCannon)==QuickHipAction::Assign);
    assert(updateQuickHip(h,0,false,false,BackWeaponKind::GaussCannon)==QuickHipAction::None);
    assert(h.slots[0]==BackWeaponKind::GaussCannon&&h.slots[1]==BackWeaponKind::SuperShotgun);
    resetQuickHipGestures(h);
    assert(h.slots[0]==BackWeaponKind::GaussCannon&&h.slots[1]==BackWeaponKind::SuperShotgun);
    return 0;
}
