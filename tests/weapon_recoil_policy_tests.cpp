#include "../src/openxr/WeaponRecoilPolicy.h"
#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>

int main(){
    using namespace kharvox;
    WeaponRecoilSettings settings{};
    WeaponRecoilState state{};
    assert(!anyWeaponRecoilEnabled(settings));
    for(const bool on:settings.enabled)assert(!on); // no changed default gameplay
    settings.enabled[7]=true;
    assert(anyWeaponRecoilEnabled(settings));
    assert(!settings.enabled[9]); // Normal never enables Turret implicitly
    settings.enabled[7]=false;
    assert(!anyWeaponRecoilEnabled(settings));
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
    settings.enabled.fill(true); // individual flags used by existing recoil tests
    assert(anyWeaponRecoilEnabled(settings));

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

    // Heavy Assault Rifle alternate mode must be exempt from recoil.
    assert(recoilAllowedForWeaponMode(2,false));
    assert(!recoilAllowedForWeaponMode(2,true));
    assert(recoilAllowedForWeaponMode(6,true));
    assert(recoilAllowedForWeaponMode(9,true));

    // Gauss firing is NOT a trigger-input estimate. Native ammo must
    // decrease before even a huge 900% kick can be scheduled.
    state={};
    settings.strength[6]=9.f;
    settings.recovery[6]=.25f;
    auto gauss=[&](std::uint64_t time,bool trigger,int ammo,bool siege=false,
                   bool twoHands=false){
        return advanceWeaponRecoil(state,settings,
            {6,trigger,twoHands,true,time,siege,ammo});
    };
    auto idle=gauss(6000,false,180);
    assert(idle.pitchDegrees==0.f);
    assert(gauss(6010,true,180).pitchDegrees==0.f);
    assert(gauss(6200,true,180).pitchDegrees==0.f);
    assert(gauss(6210,false,180).pitchDegrees==0.f);
    // Five rapid pulls during cooldown and zero native ammo spent.
    for(std::uint64_t ms=6240;ms<=6440;ms+=50){
        assert(gauss(ms,true,180).pitchDegrees==0.f);
        assert(gauss(ms+20,false,180).pitchDegrees==0.f);
    }
    assert(gauss(6450,true,180).pitchDegrees==0.f);
    const auto spent=gauss(6540,true,165);
    assert(spent.pitchDegrees==0.f&&!spent.shotEstimated);
    const auto confirmedKick=gauss(6540+kGaussConfirmedAmmoGraceMs,true,165);
    assert(confirmedKick.shotEstimated&&confirmedKick.pitchDegrees>30.f);
    // Even with 25% recovery the gun must completely reset, never leave
    // an exponential tail that contaminates the following shot.
    const auto backToNeutral=gauss(6540+kGaussConfirmedAmmoGraceMs+421,true,165);
    assert(backToNeutral.pitchDegrees==0.f&&backToNeutral.yawDegrees==0.f);

    // 900% recovery is substantially faster, affecting the REAL Gauss
    // return curve rather than just the value shown in the VR menu.
    assert(gaussKickCycleMilliseconds(9.f)<100.f);
    assert(gaussKickCycleMilliseconds(2.f)>170.f);
    assert(gaussKickCycleMilliseconds(.25f)<=420.f);
    settings.recovery[6]=9.f;
    state={};
    gauss(7500,false,165);
    gauss(7510,true,165);
    gauss(7550,true,150);
    const auto fastKick=gauss(7550+kGaussConfirmedAmmoGraceMs,true,150);
    assert(fastKick.shotEstimated&&fastKick.pitchDegrees>0.f);
    const auto fastReset=gauss(7550+kGaussConfirmedAmmoGraceMs+110,true,150);
    assert(fastReset.pitchDegrees==0.f&&fastReset.yawDegrees==0.f);
    // No additional kick from trigger spam without another ammo debit.
    assert(gauss(7700,false,150).pitchDegrees==0.f);
    assert(gauss(7710,true,150).pitchDegrees==0.f);
    assert(gauss(7800,true,150).pitchDegrees==0.f);
    assert(gauss(7900,false,150).pitchDegrees==0.f);

    // Siege charge can stay held for seconds. Even an early ammo spend
    // must not kick until the shot is released, then ammo-confirmed.
    state={};
    settings.recovery[6]=9.f;
    gauss(9000,false,150,true);
    gauss(9010,true,150,true);
    assert(gauss(10700,true,150,true).pitchDegrees==0.f);
    assert(gauss(10710,true,135,true).pitchDegrees==0.f);
    assert(gauss(10800,true,135,true).pitchDegrees==0.f);
    const auto siegeReleased=gauss(10820,false,135,true);
    assert(siegeReleased.pitchDegrees==0.f&&!siegeReleased.shotEstimated);
    const auto siegeKick=gauss(10820+kGaussConfirmedAmmoGraceMs,false,135,true);
    assert(siegeKick.shotEstimated&&siegeKick.pitchDegrees>0.f);
    assert(gauss(10940,false,135,true).pitchDegrees==0.f);
    // Also accept an ammo decrease observed only AFTER Siege release.
    state={};
    gauss(11000,false,135,true);
    gauss(11010,true,135,true);
    gauss(11200,false,135,true);
    assert(gauss(11220,false,120,true).pitchDegrees==0.f);
    assert(gauss(11220+kGaussConfirmedAmmoGraceMs,false,120,true).shotEstimated);

    // If native ammo is unavailable, FAIL CLOSED instead of inventing a
    // Gauss shot. Restart baseline when samples become valid again.
    state={};
    gauss(12000,false,120);
    gauss(12010,true,-1);
    assert(gauss(12200,true,-1).pitchDegrees==0.f);
    assert(gauss(12210,true,105).pitchDegrees==0.f);
    assert(gauss(12250,false,105).pitchDegrees==0.f);
    settings.recovery[6]=9.1f;
    assert(!validWeaponRecoilSettings(settings));
    settings.recovery[6]=9.f;
    assert(validWeaponRecoilSettings(settings));

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
    assert(loaded.enabled[1]&&loaded.strength[1]==9.f
        &&loaded.recovery[7]==.25f);
    assert(loaded.strength[7]==3.f&&loaded.strength[9]==1.7f);
    // Per-weapon switches survive save/reload and do not leak to siblings.
    settings.enabled.fill(false);
    settings.enabled[1]=true; // shotgun
    settings.enabled[9]=true; // Minigun Turret but NOT Normal
    assert(saveWeaponRecoilSettings(path.string(),settings));
    WeaponRecoilSettings independent{};
    assert(loadWeaponRecoilSettings(path.string(),independent));
    for(std::size_t i=0;i<kWeaponRecoilKinds.size();++i)
        assert(independent.enabled[i]==(i==1||i==9));
    assert(independent.strength[7]==settings.strength[7]);
    assert(independent.strength[9]==settings.strength[9]);
    state={};
    const auto disabledNormal=advanceWeaponRecoil(state,independent,
        {7,true,false,true,5500});
    assert(disabledNormal.pitchDegrees==0.f&&!disabledNormal.shotEstimated);
    const auto enabledTurret=advanceWeaponRecoil(state,independent,
        {9,true,false,true,5600});
    const auto turretShot=advanceWeaponRecoil(state,independent,
        {9,true,false,true,5600+kWeaponRecoilPostFireDelayMs});
    assert(enabledTurret.pitchDegrees==0.f&&turretShot.shotEstimated
        &&turretShot.pitchDegrees>0.f);
    independent.enabled[9]=false;
    const auto disabledImmediately=advanceWeaponRecoil(state,independent,
        {9,true,false,true,5700});
    assert(disabledImmediately.pitchDegrees==0.f&&!disabledImmediately.shotEstimated);
    independent.enabled[7]=true;
    const auto freshlyEnabled=advanceWeaponRecoil(state,independent,
        {7,true,false,true,5800});
    assert(freshlyEnabled.pitchDegrees==0.f&&!freshlyEnabled.shotEstimated);
    // A partially written schema-2 file fails closed: don't discard
    // the caller's existing settings or clear other weapon ON flags.
    {
        std::ofstream truncated(path,std::ios::trunc);
        truncated<<"schema 2\nshotgun 1 2.500 0.600\n";
    }
    assert(!loadWeaponRecoilSettings(path.string(),independent));
    assert(independent.enabled[7]&&!independent.enabled[9]);
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
    for(bool on:migrated.enabled)assert(on); // legacy global 1 -> all guns
    // The legacy global OFF flag must remain OFF for every weapon.
    {
        std::ofstream legacyOff(path,std::ios::trunc);
        legacyOff<<"schema 1\nenabled 0\n";
        for(std::size_t i=0;i<9;++i)
            legacyOff<<kWeaponRecoilKinds[i]<<" 1.750 0.300\n";
    }
    WeaponRecoilSettings legacyDisabled{};
    assert(loadWeaponRecoilSettings(path.string(),legacyDisabled));
    assert(!anyWeaponRecoilEnabled(legacyDisabled));
    assert(legacyDisabled.strength[7]==1.75f);
    // Continue with the already-loaded legacy enabled migration below.
    assert(saveWeaponRecoilSettings(path.string(),migrated));
    assert(loadWeaponRecoilSettings(path.string(),loaded));
    assert(loaded.strength[7]==1.5f&&loaded.strength[9]==1.7f);
    for(bool on:loaded.enabled)assert(on);
    {
        std::ofstream bad(path,std::ios::trunc);
        bad<<"schema 1\nenabled 1\nshotgun nan 1.0\n";
    }
    assert(!loadWeaponRecoilSettings(path.string(),loaded));
    std::filesystem::remove(path);
    return 0;
}
