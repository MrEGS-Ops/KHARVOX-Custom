#pragma once
#include "BackWeaponPolicy.h"
#include <array>
#include <cmath>
namespace kharvox {
struct QuickHolsterZone {
    BackWeaponZoneBounds enter{0.12f,0.51f,-1.04f,-0.48f,0.02f,0.43f};
    BackWeaponZoneBounds exit{0.07f,0.60f,-1.14f,-0.38f,-0.05f,0.53f};
};
inline bool quickHipInside(const BackWeaponVec3& p,bool previouslyInside) {
    if(!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(p.z))return false;
    const QuickHolsterZone z{};
    return backWeaponPointInside(p,previouslyInside?z.exit:z.enter);
}
// Each hip is a persistent weapon HOTKEY: no props, hiding, stow state or
// depletion on use. Side zero is right; side one is left.
struct QuickHipGesture {
    bool inside{},enteredClosed{},enteredOpen{},gripWasDown{},consumed{};
};
struct QuickHolsters {
    std::array<BackWeaponKind,2> slots{BackWeaponKind::Unknown,BackWeaponKind::Unknown};
    std::array<QuickHipGesture,2> gestures{};
};
enum class QuickHipAction {None,Enter,Assign,Draw};
// Enter CLOSED + release INSIDE assigns whatever weapon is equipped.
// Enter OPEN + exit CLOSED selects the assigned gun without clearing it.
inline QuickHipAction updateQuickHip(QuickHolsters& h,int side,bool inside,
    bool gripDown,BackWeaponKind equipped) {
    if(side<0||side>1)return QuickHipAction::None;
    auto& g=h.gestures[side];
    const bool wasInside=g.inside,wasGrip=g.gripWasDown;
    g.inside=inside;g.gripWasDown=gripDown;
    if(inside&&!wasInside) {
        g.enteredClosed=gripDown;g.enteredOpen=!gripDown;g.consumed=false;
        return QuickHipAction::Enter;
    }
    if(inside) {
        if(g.enteredClosed&&!g.consumed&&wasGrip&&!gripDown
            &&equipped!=BackWeaponKind::Unknown) {
            h.slots[side]=equipped;g.consumed=true;
            return QuickHipAction::Assign;
        }
        return QuickHipAction::None;
    }
    if(wasInside) {
        const bool draw=g.enteredOpen&&!g.consumed&&gripDown
            &&h.slots[side]!=BackWeaponKind::Unknown;
        g.enteredClosed=false;g.enteredOpen=false;g.consumed=false;
        return draw?QuickHipAction::Draw:QuickHipAction::None;
    }
    return QuickHipAction::None;
}
inline void resetQuickHipGestures(QuickHolsters& h){h.gestures={};}
}
