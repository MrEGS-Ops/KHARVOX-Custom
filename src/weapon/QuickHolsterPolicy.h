#pragma once
#include "BackWeaponPolicy.h"
#include <array>
#include <cmath>
namespace kharvox {
struct QuickHolsterZone {
 BackWeaponZoneBounds enter{0.12f,0.51f,-1.04f,-0.48f,0.02f,0.43f};
 BackWeaponZoneBounds exit{0.07f,0.60f,-1.14f,-0.38f,-0.05f,0.53f};
};
inline bool quickHipInside(const BackWeaponVec3& p,bool wasInside) {
 if(!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(p.z))return false;
 const QuickHolsterZone z{};
 return backWeaponPointInside(p,wasInside?z.exit:z.enter);
}
struct QuickHipGesture {
 bool inside{},gripWasDown{},assignmentArmed{},consumed{},enteredWithGrip{};
};
struct QuickHolsters {
 std::array<BackWeaponKind,2> slots{BackWeaponKind::Unknown,BackWeaponKind::Unknown};
 std::array<QuickHipGesture,2> gestures{};
};
enum class QuickHipAction {None,Enter,Assign,Draw};
// A hip draw requests one native hotkey and its correct physical hand.
// No delayed handoff, gun hiding or weapon restoration.
struct QuickHipDrawPlan {
 bool valid{},needsNativeSelect{},leftWeaponHand{};
 BackWeaponKind weapon{BackWeaponKind::Unknown};
};
inline QuickHipDrawPlan quickHipPlanDraw(int side,BackWeaponKind stored,
                                        BackWeaponKind current) noexcept {
 if(side<0||side>1||stored==BackWeaponKind::Unknown)return {};
 return {true,stored!=current,side==1,stored};
}
// Selecting a different native weapon can take several game frames, even
// when its number-row hotkey is sent on the draw frame. Keep the old gun in
// its original hand until DOOM reports the requested kind; never present the
// old gun temporarily in the new hand, and never send a second selection.
struct QuickHipPendingTransfer {
 bool active{},leftWeaponHand{};
 BackWeaponKind weapon{BackWeaponKind::Unknown};
 std::uint64_t deadlineNanoseconds{};
};
enum class QuickHipTransferResult {None,Confirmed,TimedOut};
inline void quickHipBeginTransfer(QuickHipPendingTransfer& p,
    const QuickHipDrawPlan& plan,std::uint64_t now) noexcept {
 p={};
 if(!plan.valid||!plan.needsNativeSelect)return;
 p.active=true;p.weapon=plan.weapon;p.leftWeaponHand=plan.leftWeaponHand;
 p.deadlineNanoseconds=now+2000000000ULL;
}
inline QuickHipTransferResult quickHipPollTransfer(QuickHipPendingTransfer& p,
    BackWeaponKind equipped,std::uint64_t now) noexcept {
 if(!p.active)return QuickHipTransferResult::None;
 if(equipped==p.weapon){p.active=false;return QuickHipTransferResult::Confirmed;}
 if(now>=p.deadlineNanoseconds){p.active=false;return QuickHipTransferResult::TimedOut;}
 return QuickHipTransferResult::None;
}
// Only the hand that actually holds the weapon may STORE it in its own
// physical hip. The opposite hand is empty, even when DOOM reports a
// globally equipped native weapon. It can still DRAW from its saved hip.
inline bool quickHipMayAssign(int side,bool weaponHandLeft) noexcept {
    return (side==0&&!weaponHandLeft)||(side==1&&weaponHandLeft);
}
// Holster assignments occur on Grip RELEASE: holding ALT FIRE must remain
// forwarded to DOOM when the gun is lowered into its own hip. Only an actual
// quick-holster DRAW press may consume that Grip and suppress native alt fire.
inline bool quickHipConsumesGrip(QuickHipAction action) noexcept {
    return action==QuickHipAction::Draw;
}
// Disable hip-hotkey recognition while two-handed aiming is latched.
// For Gauss/Chaingun, both held Grips also protect alt-fire before latch.
inline bool quickHipSuspendedForTwoHand(bool latched,bool heavy,
                                       bool weaponGrip,bool supportGrip) noexcept {
 return latched||(heavy&&weaponGrip&&supportGrip);
}
// The hip that supplies the weapon becomes the combat/weapon hand.
// 0 is the physical RIGHT hip; 1 is the physical LEFT hip.
// This rule applies even when the saved weapon is already equipped.
inline bool quickHipDrawUsesLeftHand(int side) { return side==1; }
// STORE: only the current weapon hand may save to its OWN hip. DRAW:
// either physical hand may equip its OWN populated hip on Grip press.
// Releasing an EMPTY support hand Grip inside a hip cannot accidentally
// clone the currently equipped gun into the other side's quick slot.
inline QuickHipAction updateQuickHip(QuickHolsters& h,int side,bool inside,
 bool gripDown,BackWeaponKind equipped,bool isWeaponHand) {
 if(side<0||side>1)return QuickHipAction::None;
 auto& g=h.gestures[side];
 const bool wasInside=g.inside,wasGrip=g.gripWasDown;
 g.inside=inside;g.gripWasDown=gripDown;
 // Ownership can change mid-gesture (Gunslinger draws). Never carry a
 // previously armed STORE onto a physical hand that is now empty.
 if(!isWeaponHand)g.assignmentArmed=false;
 if(inside&&!wasInside) {
  g.enteredWithGrip=gripDown;
  g.assignmentArmed=isWeaponHand&&gripDown;
  g.consumed=false;
  return QuickHipAction::Enter;
 }
 if(inside&&!g.consumed) {
  if(!wasGrip&&gripDown) {
   if(!g.enteredWithGrip&&h.slots[side]!=BackWeaponKind::Unknown) {
    g.consumed=true;return QuickHipAction::Draw;
   }
   if(isWeaponHand)g.assignmentArmed=true;
  }
  if(wasGrip&&!gripDown&&g.assignmentArmed&&isWeaponHand
     &&equipped!=BackWeaponKind::Unknown) {
   h.slots[side]=equipped;g.assignmentArmed=false;g.consumed=true;
   return QuickHipAction::Assign;
  }
 }
 if(!inside&&wasInside) {
  g.assignmentArmed=false;g.enteredWithGrip=false;g.consumed=false;
 }
 return QuickHipAction::None;
}
inline void resetQuickHipGestures(QuickHolsters& h){h.gestures={};}
}
