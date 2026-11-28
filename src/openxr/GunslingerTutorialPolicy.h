#pragma once
#include "../weapon/BackWeaponPolicy.h"
#include "GestureWheelExitPolicy.h"
#include <cstdint>
#include <cmath>
namespace kharvox {
enum class GunslingerLessonEvent { WheelEquip, HipAssign, HipDraw };
struct GunslingerTutorial {
    bool enabled{true};
    unsigned step{};
    std::uint64_t completedAt{}, pendingUntil{}, holsterConfirmedAt{};
    bool holsterConfirmation{};
    bool hipReleaseReady{};
    int pendingSide{-1};
    BackWeaponKind pendingWeapon{BackWeaponKind::Unknown};
};
inline constexpr unsigned gunslingerLessonCount=6;
inline int gunslingerLessonSide(unsigned step) noexcept {
    return step<2||step==4?0:1;
}
inline GunslingerLessonEvent gunslingerLessonEvent(unsigned step) noexcept {
    return step==0||step==2?GunslingerLessonEvent::WheelEquip:
        step==1||step==3?GunslingerLessonEvent::HipAssign:GunslingerLessonEvent::HipDraw;
}
inline void gunslingerCancelPending(GunslingerTutorial& t) noexcept {
    t.pendingSide=-1;t.pendingWeapon=BackWeaponKind::Unknown;t.pendingUntil=0;
}
inline bool gunslingerLessonObserve(GunslingerTutorial& t,GunslingerLessonEvent event,
    int side,BackWeaponKind weapon,std::uint64_t now) noexcept {
    if(!t.enabled||t.step>=gunslingerLessonCount||weapon==BackWeaponKind::Unknown
        ||side!=gunslingerLessonSide(t.step)||event!=gunslingerLessonEvent(t.step))return false;
    t.holsterConfirmation=event==GunslingerLessonEvent::HipAssign;
    t.holsterConfirmedAt=t.holsterConfirmation?now:0;
    t.hipReleaseReady=false;
    ++t.step;
    gunslingerCancelPending(t);
    if(t.step==gunslingerLessonCount)t.completedAt=now;
    return true;
}
// Observe the same armed gesture used by native hip assignment, not a
// separate approximate tutorial zone. Leaving or disabling it restores HOLD.
inline void gunslingerHipReady(GunslingerTutorial& t,int side,bool ready) noexcept {
    if(t.enabled&&t.step<gunslingerLessonCount
        &&gunslingerLessonEvent(t.step)==GunslingerLessonEvent::HipAssign
        &&side==gunslingerLessonSide(t.step))t.hipReleaseReady=ready;
}
inline bool gunslingerHolsterConfirmedVisible(const GunslingerTutorial& t,
    std::uint64_t now) noexcept {
    return t.enabled&&t.holsterConfirmation&&now>=t.holsterConfirmedAt
        &&now-t.holsterConfirmedAt<2000;
}
// Use the existing native wheel sector map. A centered or invalid stick is
// not a selected gun, even if DOOM still reports the previously held weapon.
inline BackWeaponKind gunslingerWheelTarget(float x,float y) noexcept {
    if(!std::isfinite(x)||!std::isfinite(y)||x*x+y*y<.09f)return BackWeaponKind::Unknown;
    BackWeaponKind best=BackWeaponKind::Unknown;float score=-1.f;
    for(int i=0;i<=int(BackWeaponKind::Chaingun);++i){
        const auto kind=static_cast<BackWeaponKind>(i);
        const auto sector=previousWeaponSector(kind);
        const float dot=x*sector.x+y*sector.y;
        if(sector.valid&&dot>score){score=dot;best=kind;}
    }
    return best;
}
inline void gunslingerExpectWheel(GunslingerTutorial& t,int side,BackWeaponKind weapon,
    std::uint64_t now) noexcept {
    gunslingerCancelPending(t);
    if(t.enabled&&t.step<gunslingerLessonCount&&gunslingerLessonEvent(t.step)==GunslingerLessonEvent::WheelEquip
        &&side==gunslingerLessonSide(t.step)&&weapon!=BackWeaponKind::Unknown){
        t.pendingSide=side;t.pendingWeapon=weapon;t.pendingUntil=now+2000;
    }
}
inline bool gunslingerPollWheel(GunslingerTutorial& t,int activeSide,BackWeaponKind equipped,
    std::uint64_t now) noexcept {
    if(t.pendingSide<0)return false;
    if(now>=t.pendingUntil){gunslingerCancelPending(t);return false;}
    if(activeSide!=t.pendingSide||equipped!=t.pendingWeapon)return false;
    return gunslingerLessonObserve(t,GunslingerLessonEvent::WheelEquip,activeSide,equipped,now);
}
inline bool gunslingerTutorialVisible(const GunslingerTutorial& t,std::uint64_t now) noexcept {
    return t.enabled&&(t.step<gunslingerLessonCount||
        (t.completedAt&&now>=t.completedAt&&now-t.completedAt<4000));
}
inline void gunslingerTutorialToggle(GunslingerTutorial& t) noexcept {
    const bool enable=!t.enabled||t.step>=gunslingerLessonCount;
    t={};t.enabled=enable;
}
inline int gunslingerTutorialPreference(const GunslingerTutorial& t) noexcept {
    return !t.enabled?0:t.step>=gunslingerLessonCount?2:1;
}
inline void gunslingerTutorialApplyPreference(GunslingerTutorial& t,int value) noexcept {
    t={};t.enabled=value!=0;if(value==2)t.step=gunslingerLessonCount;
}
} // namespace kharvox
