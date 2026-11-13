#include "../src/openxr/WeaponRecoilPolicy.h"
#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>

int main(){
    using namespace kharvox;
    WeaponRecoilSettings settings{};
    WeaponRecoilState state{};
    assert(!settings.enabled);
    assert(weaponRecoilIndex("shotgun")==1);
    assert(weaponRecoilIndex("chainsaw")==-1);
    const auto noEffect=advanceWeaponRecoil(state,settings,{1,true,false,true,100});
    assert(noEffect.pitchDegrees==0.f&&!noEffect.shotEstimated);
    settings.enabled=true;

    // First trigger press must NEVER bend the first projectile trajectory.
    const auto first=advanceWeaponRecoil(state,settings,{1,true,false,true,1000});
    assert(!first.shotEstimated&&first.pitchDegrees==0.f&&first.yawDegrees==0.f);
    const auto beforeFireHandoff=advanceWeaponRecoil(state,settings,
        {1,true,false,true,1000+kWeaponRecoilPostFireDelayMs-1});
    assert(!beforeFireHandoff.shotEstimated&&beforeFireHandoff.pitchDegrees==0.f);
    const auto afterFirstShot=advanceWeaponRecoil(state,settings,
        {1,true,false,true,1000+kWeaponRecoilPostFireDelayMs});
    assert(afterFirstShot.shotEstimated&&afterFirstShot.pitchDegrees>1.f);
    const auto held=advanceWeaponRecoil(state,settings,{1,true,false,true,1100});
    assert(!held.shotEstimated&&held.pitchDegrees>0.f);
    const auto recovering=advanceWeaponRecoil(state,settings,{1,false,false,true,1220});
    assert(recovering.pitchDegrees<held.pitchDegrees);

    // A new trigger press gets pristine aim even if earlier recoil lingers.
    const auto newPress=advanceWeaponRecoil(state,settings,{1,true,false,true,1240});
    assert(!newPress.shotEstimated&&newPress.pitchDegrees==0.f&&newPress.yawDegrees==0.f);
    const auto newKick=advanceWeaponRecoil(state,settings,
        {1,false,false,true,1240+kWeaponRecoilPostFireDelayMs});
    assert(newKick.shotEstimated&&newKick.pitchDegrees>0.f); // shot fired, trigger already released

    auto twoHands=advanceWeaponRecoil(state,settings,{1,false,true,true,1350});
    assert(twoHands.pitchDegrees<newKick.pitchDegrees);
    for(std::uint64_t ms=1400;ms<2300;ms+=50)
        twoHands=advanceWeaponRecoil(state,settings,{1,false,true,true,ms});
    assert(twoHands.pitchDegrees<.01f);

    // 900% is valid, persists, and produces a stronger REAL kick than 100%.
    state={};
    settings.strength[1]=9.f;
    auto highFirst=advanceWeaponRecoil(state,settings,{1,true,false,true,2400});
    assert(highFirst.pitchDegrees==0.f&&!highFirst.shotEstimated);
    auto high=advanceWeaponRecoil(state,settings,
        {1,true,false,true,2400+kWeaponRecoilPostFireDelayMs});
    assert(high.shotEstimated&&high.pitchDegrees>afterFirstShot.pitchDegrees*5.f);
    assert(validWeaponRecoilSettings(settings));

    // Automatic fire: 1st bullet is clean, recoil follows it, subsequent
    // estimated shots accumulate after their own native-fire grace period.
    state={};
    settings.strength[2]=1.f;
    const auto autoFirst=advanceWeaponRecoil(state,settings,{2,true,false,true,3000});
    assert(autoFirst.pitchDegrees==0.f&&!autoFirst.shotEstimated);
    const auto autoKick=advanceWeaponRecoil(state,settings,
        {2,true,false,true,3000+kWeaponRecoilPostFireDelayMs});
    assert(autoKick.pitchDegrees>0.f&&autoKick.shotEstimated);
    const auto autoQueued=advanceWeaponRecoil(state,settings,{2,true,false,true,3100});
    assert(!autoQueued.shotEstimated);
    const auto autoAgain=advanceWeaponRecoil(state,settings,
        {2,true,false,true,3100+kWeaponRecoilPostFireDelayMs});
    assert(autoAgain.shotEstimated&&autoAgain.pitchDegrees>autoKick.pitchDegrees);

    // Weapon swap, two-hand capture, menu and disable invalidate pending kick.
    const auto switched=advanceWeaponRecoil(state,settings,{7,false,false,true,3200});
    assert(switched.pitchDegrees==0.f);
    const auto menu=advanceWeaponRecoil(state,settings,{7,true,false,false,3300});
    assert(menu.pitchDegrees==0.f);
    state={};
    const auto pending=advanceWeaponRecoil(state,settings,{7,true,false,true,4200});
    assert(pending.pitchDegrees==0.f);
    const auto supportGrab=advanceWeaponRecoil(state,settings,{7,true,true,true,4220});
    const auto stable=advanceWeaponRecoil(state,settings,{7,true,true,true,4300});
    assert(supportGrab.pitchDegrees==0.f&&stable.pitchDegrees==0.f
        &&!stable.shotEstimated);
    const auto invalidSwap=advanceWeaponRecoil(state,settings,{1,false,false,true,4310});
    assert(invalidSwap.pitchDegrees==0.f);

    settings.strength[1]=9.1f;
    assert(!validWeaponRecoilSettings(settings));
    settings.strength[1]=9.f;
    const auto path=std::filesystem::temp_directory_path()/"kharvox_recoil_policy_test.cfg";
    std::filesystem::remove(path);
    assert(saveWeaponRecoilSettings(path.string(),settings));
    WeaponRecoilSettings loaded{};
    assert(loadWeaponRecoilSettings(path.string(),loaded));
    assert(loaded.enabled&&loaded.strength[1]==9.f);
    {
        std::ofstream bad(path,std::ios::trunc);
        bad<<"schema 1\nenabled 1\nshotgun nan 1.0\n";
    }
    assert(!loadWeaponRecoilSettings(path.string(),loaded));
    std::filesystem::remove(path);
    return 0;
}
