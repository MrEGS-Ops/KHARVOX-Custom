#include "../src/openxr/VrLiveModsConfig.h"
#include <cassert>
#include <cmath>
#include <cstdio>
int main(){
    using namespace kharvox;
    VrLiveSettings s{};
    assert(vrLiveValidSettings(s));
    assert(vrLiveHipInside({.315f,-.76f,.225f},false,s.hipShift[0]));
    assert(vrLiveWheelInside({.20f,.09f,.3075f},false,s.wheelShift[1]));
    assert(vrLiveCapturePoint(s,VrLiveCapture::HipRight,
        {.40f,-.71f,.18f},0));
    assert(vrLiveHipInside({.40f,-.71f,.18f},false,s.hipShift[0]));
    assert(std::abs(s.hipShift[1].x)<.00001f); // opposite side untouched
    assert(vrLiveCapturePoint(s,VrLiveCapture::WheelLeft,
        {.30f,.10f,.37f},0));
    assert(vrLiveWheelInside({.30f,.10f,.37f},false,s.wheelShift[1]));
    assert(vrLiveCapturePoint(s,VrLiveCapture::CrouchStanding,{},.03f));
    assert(!vrLiveCapturePoint(s,VrLiveCapture::CrouchLowered,{},-.05f));
    assert(vrLiveCapturePoint(s,VrLiveCapture::CrouchLowered,{},-.53f));
    assert(std::abs(vrLiveCrouchEntry(s)+.25f)<.0001f);
    assert(std::abs(vrLiveCrouchExit(s)+.17f)<.0001f);
    assert(vrLiveEffectiveEnabled(s,VrLiveMod::Gunslinger,true));
    assert(!vrLiveEffectiveEnabled(s,VrLiveMod::Gunslinger,false));
    assert(vrLiveSetEnabled(s,VrLiveMod::Gunslinger,false));
    assert(!vrLiveEffectiveEnabled(s,VrLiveMod::Gunslinger,true));
    assert(!vrLiveSetEnabled(s,VrLiveMod::HandFocus,false));
    assert(s.enabledOverrides[1]==0);
    s.gloryKillHz=90;s.punchSpeedMetersPerSec=3.4f;s.punchHands=1;
    const std::string file="vr-live-mod-calibration-test.cfg";
    assert(vrLiveSave(file,s));
    VrLiveSettings read{};
    assert(vrLiveLoad(file,read));
    assert(std::abs(read.hipShift[0].x-s.hipShift[0].x)<.00001f);
    assert(std::abs(read.wheelShift[1].x-s.wheelShift[1].x)<.00001f);
    assert(read.gloryKillHz==90&&read.punchHands==1);
    assert(!vrLiveEffectiveEnabled(read,VrLiveMod::Gunslinger,true));
    assert(vrLiveModEditable(VrLiveMod::WeaponGrip));
    assert(vrLiveModEditable(VrLiveMod::PhysicalCrouch));
    assert(!vrLiveModEditable(VrLiveMod::HandFocus));
    assert(!vrLiveModEditable(VrLiveMod::Painsaw));
    // Native action pacing: LOWER Hz means faster, HIGHER Hz means slower.
    assert(vrLiveStepKilltimeHz(0,true)==210); // unset baseline 200
    assert(vrLiveStepKilltimeHz(0,false)==190);
    assert(vrLiveStepKilltimeHz(150,true)==160);
    assert(vrLiveStepKilltimeHz(150,false)==140);
    assert(vrLiveStepKilltimeHz(240,true)==240);
    assert(vrLiveStepKilltimeHz(30,false)==30);
    const VrLiveSettings defaults{};
    assert(defaults.gloryKillHz==0);
    assert(defaults.punchSpeedMetersPerSec==2.8f);
    std::remove(file.c_str());
    return 0;
}
