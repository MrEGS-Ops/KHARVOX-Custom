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
 assert(!quickHipDrawUsesLeftHand(0)); // RIGHT hip -> RIGHT weapon hand
 assert(quickHipDrawUsesLeftHand(1));  // LEFT hip -> LEFT weapon hand
 QuickHolsters h{};
 // Actual reported path: enter OPEN, press then RELEASE inside to assign.
 assert(updateQuickHip(h,0,true,false,BackWeaponKind::Pistol)==QuickHipAction::Enter);
 assert(updateQuickHip(h,0,true,true,BackWeaponKind::Pistol)==QuickHipAction::None);
 assert(updateQuickHip(h,0,true,false,BackWeaponKind::Pistol)==QuickHipAction::Assign);
 assert(h.slots[0]==BackWeaponKind::Pistol);
 assert(updateQuickHip(h,0,true,true,BackWeaponKind::Pistol)==QuickHipAction::None);
 assert(updateQuickHip(h,0,false,true,BackWeaponKind::Pistol)==QuickHipAction::None);
 // Equip as soon as Grip PRESSES in occupied zone, before any exit.
 assert(updateQuickHip(h,0,true,false,BackWeaponKind::SuperShotgun)==QuickHipAction::Enter);
 const auto rightDraw=updateQuickHip(h,0,true,true,BackWeaponKind::SuperShotgun);
 assert(rightDraw==QuickHipAction::Draw);
 bool leftHandActive=true; // simulate player currently holding a gun on left
 if(rightDraw==QuickHipAction::Draw)leftHandActive=quickHipDrawUsesLeftHand(0);
 assert(!leftHandActive);  // retrieving RIGHT hip moves the gun to RIGHT
 assert(h.slots[0]==BackWeaponKind::Pistol);
 assert(updateQuickHip(h,0,true,false,BackWeaponKind::SuperShotgun)==QuickHipAction::None);
 assert(updateQuickHip(h,0,false,false,BackWeaponKind::SuperShotgun)==QuickHipAction::None);
 // Replace occupied right slot by entering CLOSED and releasing INSIDE.
 assert(updateQuickHip(h,0,true,true,BackWeaponKind::GaussCannon)==QuickHipAction::Enter);
 assert(updateQuickHip(h,0,true,false,BackWeaponKind::GaussCannon)==QuickHipAction::Assign);
 assert(updateQuickHip(h,0,false,false,BackWeaponKind::GaussCannon)==QuickHipAction::None);
 assert(h.slots[0]==BackWeaponKind::GaussCannon);
 assert(updateQuickHip(h,1,true,true,BackWeaponKind::SuperShotgun)==QuickHipAction::Enter);
 assert(updateQuickHip(h,1,true,false,BackWeaponKind::SuperShotgun)==QuickHipAction::Assign);
 assert(h.slots[1]==BackWeaponKind::SuperShotgun);
 assert(updateQuickHip(h,1,false,false,BackWeaponKind::SuperShotgun)==QuickHipAction::None);
 // LEFT hip draw must make LEFT the weapon hand, even when the very
 // same weapon is already active in the right hand.
 assert(updateQuickHip(h,1,true,false,BackWeaponKind::SuperShotgun)==QuickHipAction::Enter);
 const auto leftDraw=updateQuickHip(h,1,true,true,BackWeaponKind::SuperShotgun);
 assert(leftDraw==QuickHipAction::Draw);
 if(leftDraw==QuickHipAction::Draw)leftHandActive=quickHipDrawUsesLeftHand(1);
 assert(leftHandActive);
 assert(h.slots[1]==BackWeaponKind::SuperShotgun);
 assert(updateQuickHip(h,1,false,true,BackWeaponKind::SuperShotgun)==QuickHipAction::None);
 QuickHolsters empty{};
 assert(updateQuickHip(empty,1,true,false,BackWeaponKind::Pistol)==QuickHipAction::Enter);
 assert(updateQuickHip(empty,1,true,true,BackWeaponKind::Pistol)==QuickHipAction::None);
 assert(updateQuickHip(empty,1,false,true,BackWeaponKind::Pistol)==QuickHipAction::None);
 assert(empty.slots[1]==BackWeaponKind::Unknown);
 QuickHipHandoff pending{};
 assert(quickHipHandoffState(pending,BackWeaponKind::GaussCannon,10)
    ==QuickHipHandoffState::Idle);
 pending.request(1,BackWeaponKind::SuperShotgun,100);
 assert(pending.pending()&&pending.side==1);
 assert(quickHipHideStaleWeapon(pending,BackWeaponKind::GaussCannon));
 assert(!quickHipHideStaleWeapon(pending,BackWeaponKind::SuperShotgun));
 assert(quickHipHandoffState(pending,BackWeaponKind::GaussCannon,101)
    ==QuickHipHandoffState::Waiting); // Old gun cannot cross to new hand.
 assert(quickHipHandoffState(pending,BackWeaponKind::SuperShotgun,102)
    ==QuickHipHandoffState::Ready);
 pending.reset();pending.request(0,BackWeaponKind::Pistol,200);
 assert(quickHipHandoffState(pending,BackWeaponKind::GaussCannon,2000000300ULL)
    ==QuickHipHandoffState::Expired);
 pending.request(1,BackWeaponKind::Chainsaw,300);
 pending.request(0,BackWeaponKind::Chaingun,301);
 assert(pending.side==0&&pending.target==BackWeaponKind::Chaingun);
 resetQuickHipGestures(h);
 assert(h.slots[0]==BackWeaponKind::GaussCannon&&h.slots[1]==BackWeaponKind::SuperShotgun);
 return 0;
}
