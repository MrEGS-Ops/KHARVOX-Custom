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
 assert(updateQuickHip(h,0,true,false,BackWeaponKind::Pistol,true)==QuickHipAction::Enter);
 assert(updateQuickHip(h,0,true,true,BackWeaponKind::Pistol,true)==QuickHipAction::None);
 assert(updateQuickHip(h,0,true,false,BackWeaponKind::Pistol,true)==QuickHipAction::Assign);
 assert(h.slots[0]==BackWeaponKind::Pistol);
 assert(updateQuickHip(h,0,true,true,BackWeaponKind::Pistol,true)==QuickHipAction::None);
 assert(updateQuickHip(h,0,false,true,BackWeaponKind::Pistol,true)==QuickHipAction::None);
 // Equip as soon as Grip PRESSES in occupied zone, before any exit.
 assert(updateQuickHip(h,0,true,false,BackWeaponKind::SuperShotgun,true)==QuickHipAction::Enter);
 const auto rightDraw=updateQuickHip(h,0,true,true,BackWeaponKind::SuperShotgun,true);
 assert(rightDraw==QuickHipAction::Draw);
 bool leftHandActive=true; // simulate player currently holding a gun on left
 if(rightDraw==QuickHipAction::Draw)leftHandActive=quickHipDrawUsesLeftHand(0);
 assert(!leftHandActive);  // retrieving RIGHT hip moves the gun to RIGHT
 assert(h.slots[0]==BackWeaponKind::Pistol);
 assert(updateQuickHip(h,0,true,false,BackWeaponKind::SuperShotgun,true)==QuickHipAction::None);
 assert(updateQuickHip(h,0,false,false,BackWeaponKind::SuperShotgun,true)==QuickHipAction::None);
 // Replace occupied right slot by entering CLOSED and releasing INSIDE.
 assert(updateQuickHip(h,0,true,true,BackWeaponKind::GaussCannon,true)==QuickHipAction::Enter);
 assert(updateQuickHip(h,0,true,false,BackWeaponKind::GaussCannon,true)==QuickHipAction::Assign);
 assert(updateQuickHip(h,0,false,false,BackWeaponKind::GaussCannon,true)==QuickHipAction::None);
 assert(h.slots[0]==BackWeaponKind::GaussCannon);
 assert(updateQuickHip(h,1,true,true,BackWeaponKind::SuperShotgun,true)==QuickHipAction::Enter);
 assert(updateQuickHip(h,1,true,false,BackWeaponKind::SuperShotgun,true)==QuickHipAction::Assign);
 assert(h.slots[1]==BackWeaponKind::SuperShotgun);
 assert(updateQuickHip(h,1,false,false,BackWeaponKind::SuperShotgun,true)==QuickHipAction::None);
 // LEFT hip draw must make LEFT the weapon hand, even when the very
 // same weapon is already active in the right hand.
 assert(updateQuickHip(h,1,true,false,BackWeaponKind::SuperShotgun,true)==QuickHipAction::Enter);
 const auto leftDraw=updateQuickHip(h,1,true,true,BackWeaponKind::SuperShotgun,true);
 assert(leftDraw==QuickHipAction::Draw);
 if(leftDraw==QuickHipAction::Draw)leftHandActive=quickHipDrawUsesLeftHand(1);
 assert(leftHandActive);
 assert(h.slots[1]==BackWeaponKind::SuperShotgun);
 assert(updateQuickHip(h,1,false,true,BackWeaponKind::SuperShotgun,true)==QuickHipAction::None);
 QuickHolsters empty{};
 assert(updateQuickHip(empty,1,true,false,BackWeaponKind::Pistol,true)==QuickHipAction::Enter);
 assert(updateQuickHip(empty,1,true,true,BackWeaponKind::Pistol,true)==QuickHipAction::None);
 assert(updateQuickHip(empty,1,false,true,BackWeaponKind::Pistol,true)==QuickHipAction::None);
 assert(empty.slots[1]==BackWeaponKind::Unknown);
 // Direct one-shot plan: target and physical hand; never a pending swap.
 const auto right=quickHipPlanDraw(0,BackWeaponKind::GaussCannon,BackWeaponKind::SuperShotgun);
 assert(right.valid&&right.needsNativeSelect&&!right.leftWeaponHand
        &&right.weapon==BackWeaponKind::GaussCannon);
 const auto left=quickHipPlanDraw(1,BackWeaponKind::Chaingun,BackWeaponKind::GaussCannon);
 assert(left.valid&&left.needsNativeSelect&&left.leftWeaponHand
        &&left.weapon==BackWeaponKind::Chaingun);
 const auto equipped=quickHipPlanDraw(1,BackWeaponKind::Chaingun,BackWeaponKind::Chaingun);
 assert(equipped.valid&&!equipped.needsNativeSelect&&equipped.leftWeaponHand);
 assert(!quickHipPlanDraw(2,BackWeaponKind::Pistol,BackWeaponKind::Pistol).valid);
 assert(!quickHipPlanDraw(0,BackWeaponKind::Unknown,BackWeaponKind::GaussCannon).valid);
 // Native DOOM can still display the previous weapon for a game frame
 // after the direct hotkey. Never transfer hand ownership before the native
 // weapon kind matches the target (and never issue an extra hotkey).
 QuickHipPendingTransfer pending{};
 quickHipBeginTransfer(pending,right,1000000000ULL);
 assert(pending.active&&!pending.leftWeaponHand
        &&pending.weapon==BackWeaponKind::GaussCannon);
 assert(quickHipPollTransfer(pending,BackWeaponKind::SuperShotgun,
        1100000000ULL)==QuickHipTransferResult::None);
 assert(pending.active);
 assert(quickHipPollTransfer(pending,BackWeaponKind::GaussCannon,
        1200000000ULL)==QuickHipTransferResult::Confirmed);
 assert(!pending.active);
 quickHipBeginTransfer(pending,left,1300000000ULL);
 assert(pending.active&&pending.leftWeaponHand);
 assert(quickHipPollTransfer(pending,BackWeaponKind::Unknown,
        3299999999ULL)==QuickHipTransferResult::None);
 assert(quickHipPollTransfer(pending,BackWeaponKind::Pistol,
        3300000000ULL)==QuickHipTransferResult::TimedOut);
 assert(!pending.active);
 quickHipBeginTransfer(pending,equipped,4000000000ULL);
 assert(!pending.active); // already-equipped same-weapon hand transfer is immediate
 // Specific real-world regression: shotgun held in LEFT, RIGHT is empty.
 // Releasing the right Grip IN THE RIGHT HIP must never store that shotgun.
 QuickHolsters offhand{};
 assert(!quickHipMayAssign(0,true)); // right hip is empty hand
 assert(quickHipMayAssign(1,true));  // left hip owns shotgun
 assert(quickHipMayAssign(0,false));
 assert(!quickHipMayAssign(1,false));
 assert(!quickHipMayAssign(-1,false));
 assert(!quickHipMayAssign(2,true));
 assert(updateQuickHip(offhand,0,true,true,BackWeaponKind::CombatShotgun,false)
        ==QuickHipAction::Enter);
 assert(!quickHipConsumesGrip(QuickHipAction::Enter));
 assert(updateQuickHip(offhand,0,true,false,BackWeaponKind::CombatShotgun,false)
        ==QuickHipAction::None);
 assert(offhand.slots[0]==BackWeaponKind::Unknown);
 // Same bug against an already populated right hip: never overwrite it.
 offhand.slots[0]=BackWeaponKind::Pistol;
 assert(updateQuickHip(offhand,0,false,false,BackWeaponKind::CombatShotgun,false)
        ==QuickHipAction::None);
 assert(updateQuickHip(offhand,0,true,true,BackWeaponKind::CombatShotgun,false)
        ==QuickHipAction::Enter);
 assert(updateQuickHip(offhand,0,true,false,BackWeaponKind::CombatShotgun,false)
        ==QuickHipAction::None);
 assert(offhand.slots[0]==BackWeaponKind::Pistol);
 // But opening an EMPTY right hand in that hip, then pressing Grip,
 // must still DRAW right-hip pistol immediately and transfer hand.
 assert(updateQuickHip(offhand,0,false,false,BackWeaponKind::CombatShotgun,false)
        ==QuickHipAction::None);
 assert(updateQuickHip(offhand,0,true,false,BackWeaponKind::CombatShotgun,false)
        ==QuickHipAction::Enter);
 const auto offhandDraw=updateQuickHip(offhand,0,true,true,
                                      BackWeaponKind::CombatShotgun,false);
 assert(offhandDraw==QuickHipAction::Draw);
 assert(quickHipConsumesGrip(offhandDraw));
 assert(offhand.slots[0]==BackWeaponKind::Pistol);
 assert(updateQuickHip(offhand,0,true,false,BackWeaponKind::Pistol,true)
        ==QuickHipAction::None); // draw cannot become a STORE on release
 // Same test mirrored: shotgun held RIGHT, empty LEFT may not save it.
 QuickHolsters emptyLeft{};
 assert(updateQuickHip(emptyLeft,1,true,true,BackWeaponKind::CombatShotgun,false)
        ==QuickHipAction::Enter);
 assert(updateQuickHip(emptyLeft,1,true,false,BackWeaponKind::CombatShotgun,false)
        ==QuickHipAction::None);
 assert(emptyLeft.slots[1]==BackWeaponKind::Unknown);
 // Hand ownership switching while an assignment is armed must cancel STORE.
 QuickHolsters ownershipSwap{};
 assert(updateQuickHip(ownershipSwap,0,true,true,BackWeaponKind::GaussCannon,true)
        ==QuickHipAction::Enter);
 assert(updateQuickHip(ownershipSwap,0,true,false,BackWeaponKind::GaussCannon,false)
        ==QuickHipAction::None);
 assert(ownershipSwap.slots[0]==BackWeaponKind::Unknown);
 assert(!quickHipConsumesGrip(QuickHipAction::None));
 assert(!quickHipConsumesGrip(QuickHipAction::None));
 // Holding Gauss Siege / Minigun Turret primary Grip through the hip
 // remains native ALT FIRE. Enter and assignment never consume Grip.
 assert(!quickHipConsumesGrip(QuickHipAction::Enter));
 assert(!quickHipConsumesGrip(QuickHipAction::Assign));
 assert(quickHipConsumesGrip(QuickHipAction::Draw));
 resetQuickHipGestures(h);
 assert(h.slots[0]==BackWeaponKind::GaussCannon&&h.slots[1]==BackWeaponKind::SuperShotgun);
 // Full two-hand latch: hip zones are disabled for ALL weapons.
 assert(quickHipSuspendedForTwoHand(true,false,false,true));
 // Both held Grips protect heavy-gun alt-fire even before support latch.
 assert(quickHipSuspendedForTwoHand(false,true,true,true));
 // Normal one-hand holsters remain usable (including heavy weapons).
 assert(!quickHipSuspendedForTwoHand(false,true,true,false));
 assert(!quickHipSuspendedForTwoHand(false,false,true,true));
 assert(!quickHipSuspendedForTwoHand(false,true,false,false));
 return 0;
}
