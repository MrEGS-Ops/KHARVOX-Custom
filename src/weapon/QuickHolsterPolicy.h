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
// Physical weapon hand switches INSTANTLY. If the native gun declaration
// needs another game frame to change, hide only the old model until the
// requested hotkey weapon is active. No timed handoff or weapon-wheel delay.
struct QuickHipHandoff {
    int side{-1};
    BackWeaponKind target{BackWeaponKind::Unknown};
    std::uint64_t deadlineNanoseconds{};
    void reset() noexcept {side=-1;target=BackWeaponKind::Unknown;deadlineNanoseconds=0;}
    bool pending() const noexcept {return side>=0&&target!=BackWeaponKind::Unknown;}
    void request(int targetSide,BackWeaponKind weapon,std::uint64_t now) noexcept {
        if(targetSide<0||targetSide>1||weapon==BackWeaponKind::Unknown){reset();return;}
        side=targetSide;target=weapon;
        deadlineNanoseconds=now+2000000000ULL;
    }
};
enum class QuickHipHandoffState {Waiting,Ready,Expired,Idle};
inline bool quickHipHideStaleWeapon(
    const QuickHipHandoff& pending,BackWeaponKind active) noexcept {
    return pending.pending()&&pending.target!=active;
}
inline QuickHipHandoffState quickHipHandoffState(
    const QuickHipHandoff& pending,BackWeaponKind active,std::uint64_t now) noexcept {
    if(!pending.pending())return QuickHipHandoffState::Idle;
    if(active==pending.target)return QuickHipHandoffState::Ready;
    if(now>=pending.deadlineNanoseconds)return QuickHipHandoffState::Expired;
    return QuickHipHandoffState::Waiting;
}
// The hip that supplies the weapon becomes the combat/weapon hand.
// 0 is the physical RIGHT hip; 1 is the physical LEFT hip.
// This rule applies even when the saved weapon is already equipped.
inline bool quickHipDrawUsesLeftHand(int side) { return side==1; }
// Assign empty slot: press and release Grip INSIDE. Equip occupied slot:
// press Grip INSIDE, no exit required. Reassign occupied slot: enter with
// Grip held, release INSIDE. Slots never clear on draw. The old weapon model
// is only temporarily hidden if a new native hotkey has not taken effect.
inline QuickHipAction updateQuickHip(QuickHolsters& h,int side,bool inside,
 bool gripDown,BackWeaponKind equipped) {
 if(side<0||side>1)return QuickHipAction::None;
 auto& g=h.gestures[side];
 const bool wasInside=g.inside,wasGrip=g.gripWasDown;
 g.inside=inside;g.gripWasDown=gripDown;
 if(inside&&!wasInside) {
  g.enteredWithGrip=gripDown;g.assignmentArmed=gripDown;g.consumed=false;
  return QuickHipAction::Enter;
 }
 if(inside&&!g.consumed) {
  if(!wasGrip&&gripDown) {
   if(!g.enteredWithGrip&&h.slots[side]!=BackWeaponKind::Unknown) {
    g.consumed=true;return QuickHipAction::Draw;
   }
   g.assignmentArmed=true;
  }
  if(wasGrip&&!gripDown&&g.assignmentArmed
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
