#include "../src/openxr/FastKillSpeedPolicy.h"
#include <cassert>

int main() {
    using namespace kharvox;
    FastKillSpeedInput x{};
    x.fastGloryKillsEnabled=true;
    x.fastChainsawKillsEnabled=true;
    assert(selectFastKillSpeed(x)==0.f); // no kill means NO speed override
    x.syncAttackConfirmed=true;
    assert(selectFastKillSpeed(x)==1.70f);
    x.chainsawKillLatched=true;
    assert(selectFastKillSpeed(x)==1.90f);
    x.syncAttackConfirmed=false;x.chainsawCinematicConfirmed=true;
    assert(selectFastKillSpeed(x)==1.90f); // chainsaw may have no sync flag
    x.fastChainsawKillsEnabled=false;
    assert(selectFastKillSpeed(x)==0.f); // GK checkbox must not affect chainsaw
    x.syncAttackConfirmed=true;x.chainsawCinematicConfirmed=false;
    assert(selectFastKillSpeed(x)==0.f); // independent checkboxes
    x.chainsawKillLatched=false;
    assert(selectFastKillSpeed(x)==1.70f);
    x.fastGloryKillsEnabled=false;
    assert(selectFastKillSpeed(x)==0.f);
    x.fastGloryKillsEnabled=true;
    x.boxingGuardActive=true;
    assert(selectFastKillSpeed(x)==0.f);
    x.boxingGuardActive=false;x.weaponWheelActive=true;
    assert(selectFastKillSpeed(x)==0.f);
    x.weaponWheelActive=false;x.pauseMenu=true;
    assert(selectFastKillSpeed(x)==0.f);
    x.pauseMenu=false;x.fullscreenMenu=true;
    assert(selectFastKillSpeed(x)==0.f);
    x.fullscreenMenu=false;x.deathMenu=true;
    assert(selectFastKillSpeed(x)==0.f);
    x.deathMenu=false;x.syncAttackConfirmed=false;
    assert(selectFastKillSpeed(x)==0.f);
    return 0;
}
