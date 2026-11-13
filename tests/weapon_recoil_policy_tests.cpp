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
    auto first=advanceWeaponRecoil(state,settings,{1,true,false,true,1000});
    assert(first.shotEstimated&&first.pitchDegrees>1.f);
    const auto held=advanceWeaponRecoil(state,settings,{1,true,false,true,1020});
    assert(!held.shotEstimated&&held.pitchDegrees>0.f);
    const auto recovering=advanceWeaponRecoil(state,settings,{1,false,false,true,1220});
    assert(recovering.pitchDegrees<held.pitchDegrees);
    auto twoHands=advanceWeaponRecoil(state,settings,{1,false,true,true,1300});
    assert(twoHands.pitchDegrees<recovering.pitchDegrees);
    for(std::uint64_t ms=1350;ms<2250;ms+=50)
        twoHands=advanceWeaponRecoil(state,settings,{1,false,true,true,ms});
    assert(twoHands.pitchDegrees<.01f);
    state={};
    settings.strength[1]=2.f;
    auto high=advanceWeaponRecoil(state,settings,{1,true,false,true,2300});
    assert(high.pitchDegrees>first.pitchDegrees);
    state={};
    auto autoFirst=advanceWeaponRecoil(state,settings,{2,true,false,true,3000});
    auto autoAgain=advanceWeaponRecoil(state,settings,{2,true,false,true,3110});
    assert(autoFirst.shotEstimated&&autoAgain.shotEstimated);
    assert(autoAgain.pitchDegrees>autoFirst.pitchDegrees);
    const auto switched=advanceWeaponRecoil(state,settings,{7,false,false,true,3200});
    assert(switched.pitchDegrees==0.f);
    const auto menu=advanceWeaponRecoil(state,settings,{7,true,false,false,3300});
    assert(menu.pitchDegrees==0.f);
    settings.strength[1]=4.f;
    assert(!validWeaponRecoilSettings(settings));
    settings.strength[1]=1.f;
    const auto path=std::filesystem::temp_directory_path()/"kharvox_recoil_policy_test.cfg";
    std::filesystem::remove(path);
    assert(saveWeaponRecoilSettings(path.string(),settings));
    WeaponRecoilSettings loaded{};
    assert(loadWeaponRecoilSettings(path.string(),loaded));
    assert(loaded.enabled&&loaded.strength[1]==1.f);
    {
        std::ofstream bad(path,std::ios::trunc);
        bad<<"schema 1\nenabled 1\nshotgun nan 1.0\n";
    }
    assert(!loadWeaponRecoilSettings(path.string(),loaded));
    std::filesystem::remove(path);
    return 0;
}
