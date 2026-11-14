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
    assert(kWeaponRecoilKinds.size()==10);
    assert(weaponRecoilIndex("chaingun")==7);
    assert(weaponRecoilIndex("chaingun",true)==9);
    assert(weaponRecoilIndex("chaingun_turret")==9);
    assert(weaponRecoilIndex("gauss_cannon",true)==6);
    assert(settings.strength[7]==3.0f&&settings.recovery[7]==.45f);
    assert(settings.strength[9]==1.7f&&settings.recovery[9]==.75f);
    assert(settings.strength[5]==3.0f&&settings.recovery[5]==.55f);
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
    assert(high.shotEstimated&&high.pitchDegrees>afterFirstShot.pitchDegrees*3.f);
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

    // Turret is independent of normal firing: while holding Fire, changing
    // mode must restart with a clean first-shot pose in the new profile.
    state={};
    const auto normalFirst=advanceWeaponRecoil(state,settings,{7,true,false,true,5000});
    const auto normalKick=advanceWeaponRecoil(state,settings,
        {7,true,false,true,5000+kWeaponRecoilPostFireDelayMs});
    const auto turretFirst=advanceWeaponRecoil(state,settings,{9,true,false,true,5100});
    const auto turretKick=advanceWeaponRecoil(state,settings,
        {9,true,false,true,5100+kWeaponRecoilPostFireDelayMs});
    assert(normalFirst.pitchDegrees==0.f&&!normalFirst.shotEstimated);
    assert(turretFirst.pitchDegrees==0.f&&!turretFirst.shotEstimated);
    assert(normalKick.pitchDegrees>turretKick.pitchDegrees);
    assert(turretKick.pitchDegrees>0.f&&turretKick.shotEstimated);

    settings.recovery[7]=.25f; // new minimum must be valid and persisted
    assert(validWeaponRecoilSettings(settings));
    settings.recovery[7]=.24f;
    assert(!validWeaponRecoilSettings(settings));
    settings.recovery[7]=.25f;
    settings.strength[1]=9.1f;
    assert(!validWeaponRecoilSettings(settings));
    settings.strength[1]=9.f;
    const auto path=std::filesystem::temp_directory_path()/"kharvox_recoil_policy_test.cfg";
    std::filesystem::remove(path);
    assert(saveWeaponRecoilSettings(path.string(),settings));
    WeaponRecoilSettings loaded{};
    assert(loadWeaponRecoilSettings(path.string(),loaded));
    assert(loaded.enabled&&loaded.strength[1]==9.f
        &&loaded.recovery[7]==.25f);
    assert(loaded.strength[7]==3.f&&loaded.strength[9]==1.7f);
    // A pre-Turret save has nine keys. Loading must preserve all those
    // user-selected values while adding ONLY the new Turret preset.
    {
        std::ofstream legacy(path,std::ios::trunc);
        legacy<<"schema 1\nenabled 1\n";
        for(std::size_t i=0;i<9;++i)
            legacy<<kWeaponRecoilKinds[i]<<" 1.500 0.250\n";
    }
    WeaponRecoilSettings migrated{};
    assert(loadWeaponRecoilSettings(path.string(),migrated));
    for(std::size_t i=0;i<9;++i)
        assert(migrated.strength[i]==1.5f&&migrated.recovery[i]==.25f);
    assert(migrated.strength[9]==kWeaponRecoilDefaultStrength[9]);
    assert(migrated.recovery[9]==kWeaponRecoilDefaultRecovery[9]);
    assert(saveWeaponRecoilSettings(path.string(),migrated));
    assert(loadWeaponRecoilSettings(path.string(),loaded));
    assert(loaded.strength[7]==1.5f&&loaded.strength[9]==1.7f);
    {
        std::ofstream bad(path,std::ios::trunc);
        bad<<"schema 1\nenabled 1\nshotgun nan 1.0\n";
    }
    assert(!loadWeaponRecoilSettings(path.string(),loaded));
    std::filesystem::remove(path);
    return 0;
}
