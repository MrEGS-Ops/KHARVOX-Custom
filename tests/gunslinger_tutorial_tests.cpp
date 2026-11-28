#include "../src/openxr/GunslingerTutorialPolicy.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
int main(){
    using namespace kharvox;
    using E=GunslingerLessonEvent;using W=BackWeaponKind;
    GunslingerTutorial t{};
    assert(gunslingerTutorialVisible(t,1));
    assert(gunslingerWheelTarget(0,0)==W::Unknown);
    assert(gunslingerWheelTarget(0,1)==W::CombatShotgun);
    assert(gunslingerWheelTarget(-1,0)==W::RocketLauncher);
    gunslingerExpectWheel(t,1,W::Pistol,10);
    assert(!gunslingerPollWheel(t,1,W::Pistol,11));
    gunslingerExpectWheel(t,0,W::Pistol,20);
    assert(!gunslingerPollWheel(t,0,W::CombatShotgun,21));
    assert(!gunslingerPollWheel(t,1,W::Pistol,22));
    assert(gunslingerPollWheel(t,0,W::Pistol,23));
    assert(!gunslingerLessonObserve(t,E::HipAssign,1,W::Pistol,24));
    assert(gunslingerLessonObserve(t,E::HipAssign,0,W::Pistol,25));
    gunslingerExpectWheel(t,1,W::CombatShotgun,30);
    assert(!gunslingerPollWheel(t,1,W::CombatShotgun,2030));
    gunslingerExpectWheel(t,1,W::CombatShotgun,2040);
    assert(gunslingerPollWheel(t,1,W::CombatShotgun,2041));
    assert(gunslingerLessonObserve(t,E::HipAssign,1,W::CombatShotgun,2042));
    assert(!gunslingerLessonObserve(t,E::HipDraw,0,W::Unknown,2043));
    assert(gunslingerLessonObserve(t,E::HipDraw,0,W::Pistol,2044));
    assert(gunslingerLessonObserve(t,E::HipDraw,1,W::CombatShotgun,2045));
    assert(gunslingerTutorialPreference(t)==2&&gunslingerTutorialVisible(t,2045));
    assert(!gunslingerTutorialVisible(t,6045));
    gunslingerTutorialToggle(t);assert(t.enabled&&t.step==0);
    gunslingerTutorialToggle(t);assert(!t.enabled&&!gunslingerTutorialVisible(t,1));
    gunslingerTutorialApplyPreference(t,2);assert(!gunslingerTutorialVisible(t,1));
    gunslingerTutorialToggle(t);assert(t.enabled&&t.step==0);
}
