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
    assert(vrLiveEffectiveEnabled(s,VrLiveMod::Gunslinger,false));
    assert(vrLiveEffectiveEnabled(s,VrLiveMod::WeaponGrip,false));
    assert(vrLiveEffectiveEnabled(s,VrLiveMod::Killtime,false));
    assert(!vrLiveModToggleable(VrLiveMod::Gunslinger));
    assert(!vrLiveModToggleable(VrLiveMod::WeaponGrip));
    assert(!vrLiveModToggleable(VrLiveMod::Killtime));
    assert(!vrLiveModToggleable(VrLiveMod::PhysicalGloryKills));
    assert(vrLiveModToggleable(VrLiveMod::PhysicalCrouch));
    assert(vrLiveModToggleable(VrLiveMod::Painsaw));
    assert(!vrLiveSetEnabled(s,VrLiveMod::Gunslinger,false));
    assert(!vrLiveSetEnabled(s,VrLiveMod::WeaponGrip,false));
    assert(!vrLiveSetEnabled(s,VrLiveMod::Killtime,false));
    assert(!vrLiveSetEnabled(s,VrLiveMod::PhysicalGloryKills,false));
    assert(!vrLiveSetEnabled(s,VrLiveMod::HandFocus,false));
    assert(s.enabledOverrides[1]==-1);
    s.gloryKillHz=90;s.punchSpeedMetersPerSec=3.4f;s.punchHands=1;
    const std::string file="vr-live-mod-calibration-test.cfg";
    assert(vrLiveSave(file,s));
    VrLiveSettings read{};
    assert(vrLiveLoad(file,read));
    assert(std::abs(read.hipShift[0].x-s.hipShift[0].x)<.00001f);
    assert(std::abs(read.wheelShift[1].x-s.wheelShift[1].x)<.00001f);
    assert(read.gloryKillHz==90&&read.punchHands==1);
    assert(vrLiveEffectiveEnabled(read,VrLiveMod::Gunslinger,true));
    assert(vrLiveModEditable(VrLiveMod::WeaponGrip));
    assert(vrLiveModEditable(VrLiveMod::PhysicalCrouch));
    assert(!vrLiveModEditable(VrLiveMod::HandFocus));
    assert(vrLiveModEditable(VrLiveMod::Painsaw));
    assert(vrLiveSetEnabled(s,VrLiveMod::Painsaw,false));
    assert(!vrLiveEffectiveEnabled(s,VrLiveMod::Painsaw,true));
    assert(vrLiveSetEnabled(s,VrLiveMod::Painsaw,true));
    assert(vrLiveEffectiveEnabled(s,VrLiveMod::Painsaw,false));
    assert(s.enabledOverrides[5]==1);
    assert(vrLiveSetEnabled(s,VrLiveMod::Painsaw,false));
    auto nativeSpeed=vrLivePainsawModes(s,true,true);
    assert(!nativeSpeed.triggerThrottle&&!nativeSpeed.legacyMotion);
    nativeSpeed=vrLivePainsawModes(s,true,false);
    assert(!nativeSpeed.triggerThrottle&&!nativeSpeed.legacyMotion);
    nativeSpeed=vrLivePainsawModes(s,false,true);
    assert(!nativeSpeed.triggerThrottle&&!nativeSpeed.legacyMotion);
    assert(vrLiveSetEnabled(s,VrLiveMod::Painsaw,true));
    auto enabledModes=vrLivePainsawModes(s,false,true);
    assert(!enabledModes.triggerThrottle&&enabledModes.legacyMotion);
    enabledModes=vrLivePainsawModes(s,true,false);
    assert(enabledModes.triggerThrottle&&!enabledModes.legacyMotion);
    assert(vrLiveSetEnabled(s,VrLiveMod::PhysicalCrouch,false));
    assert(!vrLiveEffectiveEnabled(s,VrLiveMod::PhysicalCrouch,true));
    assert(vrLiveSetEnabled(s,VrLiveMod::PhysicalCrouch,true));
    assert(vrLiveEffectiveEnabled(s,VrLiveMod::PhysicalCrouch,false));
    // Users with a five-toggle schema-1 save retain their old flags and
    // inherit PAINSAW from desktop until explicitly changed in VR.
    {std::ofstream legacy(file,std::ios::trunc);
     legacy<<"schema 1\n"
           <<"enabled 1 0 1 -1 0\n"
           <<"hip_right 0 0 0\nhip_left 0 0 0\n"
           <<"wheel_right 0 0 0\nwheel_left 0 0 0\n"
           <<"standing 0 0\nlowered 0 -0.56\n"
           <<"killtime_hz 0\npunch_speed 2.8\npunch_hands 2\n";}
    VrLiveSettings old{};
    assert(vrLiveLoad(file,old));
    assert(old.enabledOverrides[0]==1);
    assert(old.enabledOverrides[1]==0);
    assert(old.enabledOverrides[2]==1);
    assert(old.enabledOverrides[3]==-1);
    assert(old.enabledOverrides[4]==0);
    assert(old.enabledOverrides[5]==-1);
    // Previously saved OFF values for locked core mods cannot disable them.
    assert(vrLiveEffectiveEnabled(old,VrLiveMod::Gunslinger,false));
    assert(vrLiveEffectiveEnabled(old,VrLiveMod::WeaponGrip,false));
    assert(vrLiveEffectiveEnabled(old,VrLiveMod::Killtime,false));
    assert(!vrLiveSetEnabled(old,VrLiveMod::Gunslinger,false));
    // Physical GK remains desktop-controlled, not a live toggle.
    assert(vrLiveEffectiveEnabled(old,VrLiveMod::PhysicalGloryKills,true));
    assert(!vrLiveEffectiveEnabled(old,VrLiveMod::PhysicalGloryKills,false));
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
