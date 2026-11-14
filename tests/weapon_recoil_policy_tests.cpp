#include "../src/openxr/WeaponRecoilPolicy.h"
#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>

int main(){
    using namespace kharvox;
    WeaponRecoilSettings settings{};
    WeaponRecoilState state{};
    assert(anyWeaponRecoilEnabled(settings));
    assert(!settings.enabled[0]&&!settings.enabled[4]&&settings.enabled[1]
        &&settings.enabled[2]&&settings.enabled[3]&&settings.enabled[5]
        &&settings.enabled[6]&&settings.enabled[7]&&settings.enabled[8]
        &&settings.enabled[9]);
    assert(settings.dynamicEnabled&&std::abs(settings.dynamicBuildUpPerShot-.35f)<.0001f);
    assert(kWeaponRecoilKinds.size()==10);
    assert(weaponRecoilIndex("shotgun")==1);
    assert(weaponRecoilIndex("chaingun")==7);
    assert(weaponRecoilIndex("chaingun",true)==9);
    assert(weaponRecoilIndex("chaingun_turret")==9);
    assert(weaponRecoilIndex("chainsaw")==-1);
    assert(settings.strength[7]==1.f&&settings.strength[9]==3.f);
    assert(settings.recovery[7]==.45f&&settings.recovery[9]==.75f);
    assert(recoilAllowedForWeaponMode(2,false)); // Rifle primary
    assert(!recoilAllowedForWeaponMode(2,true)); // Rifle alternate
    assert(recoilAllowedForWeaponMode(6,true)); // Gauss is not the HAR alt
    // Remaining legacy impulse tests use deterministic non-dynamic tuning;
    // factory dynamic mode is tested separately above and below.
    settings.dynamicEnabled=false;
    settings.enabled.fill(true);
    auto tick=[&](int weapon,std::uint64_t ms,bool fire,int ammo,
                  bool support=false,bool gameplay=true){
        return advanceWeaponRecoil(state,settings,
            {weapon,fire,support,gameplay,ms,ammo});
    };

    // A trigger is only authorization; not a shot. Observed native ammo
    // decreases are the SOLE source of recoil, with one update of grace.
    assert(tick(1,1000,false,50).pitchDegrees==0.f);
    assert(tick(1,1010,true,50).pitchDegrees==0.f);
    assert(tick(1,1020,true,49).pitchDegrees==0.f); // ammo confirmed
    const auto firstKick=tick(1,1030,true,49);
    assert(firstKick.shotEstimated&&firstKick.pitchDegrees>0.f);
    assert(!tick(1,1050,true,49).shotEstimated);
    const auto stillRecovering=tick(1,1100,false,49);
    assert(stillRecovering.pitchDegrees<firstKick.pitchDegrees);
    // Re-pressing before recoil has settled must NEVER snap the gun
    // to neutral. It continues recovering from its current offset.
    const auto cleanPress=tick(1,1110,true,49);
    assert(cleanPress.pitchDegrees>0.f
        &&cleanPress.pitchDegrees<stillRecovering.pitchDegrees
        &&!cleanPress.shotEstimated);

    // Rapid empty/dry presses during cooldown must neither produce new
    // recoil nor erase the leftover pitch. Even changing the player's
    // saved strength/recovery does not force an instant aim reset.
    settings.strength[1]=9.f;
    settings.recovery[1]=.25f;
    float previous=cleanPress.pitchDegrees;
    for(std::uint64_t ms=1120;ms<1420;ms+=50){
        const auto released=tick(1,ms,false,49);
        assert(released.pitchDegrees>0.f
            &&released.pitchDegrees<previous
            &&!released.shotEstimated);
        const auto dry=tick(1,ms+10,true,49);
        assert(dry.pitchDegrees>0.f
            &&dry.pitchDegrees<released.pitchDegrees
            &&!dry.shotEstimated);
        previous=dry.pitchDegrees;
    }
    const auto finalRelease=tick(1,1450,false,49);
    const auto nextPress=tick(1,1460,true,49);
    assert(nextPress.pitchDegrees>0.f
        &&nextPress.pitchDegrees<finalRelease.pitchDegrees
        &&!nextPress.shotEstimated);
    const auto confirmed=tick(1,1470,true,48);
    assert(confirmed.pitchDegrees>0.f&&!confirmed.shotEstimated);
    const auto highKick=tick(1,1480,true,48);
    assert(highKick.shotEstimated&&highKick.pitchDegrees>firstKick.pitchDegrees*3.f);

    // Multiple automatic bullets within a 100ms ammo poll are coalesced
    // into one recoil impulse. It's better to miss an impulse than invent
    // recoil every frame. Each NEW observed debit makes one more kick.
    state={};
    settings.strength[2]=2.4f;
    tick(2,2000,false,400);
    assert(tick(2,2010,true,400).pitchDegrees==0.f);
    const auto batch=tick(2,2120,true,397);
    assert(batch.pitchDegrees==0.f&&!batch.shotEstimated);
    const auto batchKick=tick(2,2130,true,397);
    assert(batchKick.shotEstimated&&batchKick.pitchDegrees>0.f);
    assert(!tick(2,2200,true,397).shotEstimated);
    assert(tick(2,2230,true,395).pitchDegrees>0.f);
    const auto secondKick=tick(2,2240,true,395);
    assert(secondKick.shotEstimated&&secondKick.pitchDegrees>batchKick.pitchDegrees);

    // Minigun: sustained fire raises pitch; releasing the trigger must
    // smoothly recover, and pressing it again halfway through MUST NOT
    // teleport to zero or shrink its dynamic recoil ceiling.
    {
        WeaponRecoilSettings rapid{};
        rapid.enabled[7]=true;
        rapid.enabled[9]=true;
        rapid.strength[7]=9.f;
        rapid.recovery[7]=.25f;
        rapid.dynamicEnabled=true;
        rapid.dynamicBuildUpPerShot=.5f;
        rapid.dynamicMaxMultiplier=4.f;
        WeaponRecoilState burst{};
        auto minigun=[&](std::uint64_t ms,bool down,int ammo,int mode=7){
            return advanceWeaponRecoil(burst,rapid,
                {mode,down,false,true,ms,ammo});
        };
        minigun(2400,false,600);
        assert(minigun(2410,true,600).pitchDegrees==0.f);
        for(int n=1;n<=7;++n){
            const auto changed=minigun(2410+100u*n,true,600-n);
            assert(!changed.shotEstimated);
            const auto applied=minigun(2420+100u*n,true,600-n);
            assert(applied.shotEstimated&&applied.pitchDegrees>0.f);
        }
        const float activeCapMultiplier=burst.activeMultiplier;
        const float heldPeak=minigun(3190,true,593).pitchDegrees;
        assert(activeCapMultiplier>1.f&&heldPeak>0.f);
        // At the 65-degree cap, the visible pitch may briefly plateau
        // while fastPitch+climbPitch still exceeds that ceiling. Do not
        // mistake the capped plateau for a recovery or trigger snap.
        const auto firstRelease=minigun(3270,false,593);
        assert(firstRelease.pitchDegrees>0.f
            &&firstRelease.pitchDegrees<=heldPeak);
        const auto halfRecovered=minigun(3350,false,593);
        assert(halfRecovered.pitchDegrees>0.f
            &&halfRecovered.pitchDegrees<heldPeak);
        const auto pressedAgain=minigun(3360,true,593);
        // No native ammo decrease: not a shot. No reset of pitch or
        // active dynamic cap. The only change is smooth recovery.
        assert(!pressedAgain.shotEstimated
            &&pressedAgain.pitchDegrees>0.f
            &&pressedAgain.pitchDegrees<halfRecovered.pitchDegrees);
        assert(burst.activeMultiplier==activeCapMultiplier);
        const auto stillAiming=minigun(3380,true,593);
        assert(!stillAiming.shotEstimated
            &&stillAiming.pitchDegrees>0.f
            &&stillAiming.pitchDegrees<pressedAgain.pitchDegrees);
        const auto newAmmo=minigun(3440,true,592);
        assert(!newAmmo.shotEstimated&&newAmmo.pitchDegrees>0.f);
        const auto nextRecoil=minigun(3450,true,592);
        assert(nextRecoil.shotEstimated&&nextRecoil.pitchDegrees>0.f);
        // A real weapon-mode switch still resets (separate recoil profile).
        assert(minigun(3460,true,592,9).pitchDegrees==0.f);
    }

    // The Heavy Assault Rifle's weapon-mod/alternate firing must not
    // generate any recoil, even if its ammo pool changes during the mod.
    const auto alternate=tick(2,2250,true,390,false,false);
    assert(alternate.pitchDegrees==0.f&&!alternate.shotEstimated);
    assert(tick(2,2260,true,390).pitchDegrees==0.f);
    assert(tick(2,2270,true,389).pitchDegrees==0.f);
    assert(tick(2,2280,true,389).shotEstimated);

    // Normal Minigun and Turret retain independent settings and state.
    state={};
    settings.strength[7]=3.f;
    settings.strength[9]=1.7f;
    tick(7,3000,false,700);
    tick(7,3010,true,700);
    assert(tick(7,3020,true,697).pitchDegrees==0.f);
    const auto normalKick=tick(7,3030,true,697);
    assert(normalKick.shotEstimated&&normalKick.pitchDegrees>0.f);
    assert(tick(9,3050,true,697).pitchDegrees==0.f); // mode switch resets
    assert(tick(9,3150,true,696).pitchDegrees==0.f);
    const auto turretKick=tick(9,3160,true,696);
    assert(turretKick.shotEstimated&&turretKick.pitchDegrees>0.f);

    // Two hands suppress recoil without letting pending ammo debits kick
    // AFTER release. No override to saved grip or controller transforms.
    state={};
    tick(3,4000,false,200);
    tick(3,4010,true,200);
    assert(tick(3,4020,true,199,true).pitchDegrees==0.f);
    assert(tick(3,4030,true,199,false).pitchDegrees==0.f);
    assert(!tick(3,4040,true,199).shotEstimated);

    // No valid native ammo means NO recoil. When snapshots resume, the
    // first value is merely a baseline (not a fabricated spend).
    state={};
    tick(4,5000,false,30);
    tick(4,5010,true,-1);
    tick(4,5100,true,-1);
    assert(tick(4,5200,true,20).pitchDegrees==0.f);
    assert(tick(4,5210,true,20).pitchDegrees==0.f);
    assert(!tick(4,5210,true,20).shotEstimated);

    // Pistol has infinite ammo and is intentionally excluded, regardless
    // of saved enabled bits. Its first shot and all repeated trigger pulls
    // are always original KHARVOX behaviour.
    state={};
    assert(tick(0,6000,true,-1).pitchDegrees==0.f);
    assert(tick(0,6010,true,0).pitchDegrees==0.f);
    assert(tick(0,6020,true,-1).pitchDegrees==0.f);

    // GAUSS: dry trigger spam during its native cooldown must have zero
    // recoil. The only source is a confirmed ammo debit.
    state={};
    settings.strength[6]=9.f;
    settings.recovery[6]=.25f;
    tick(6,7000,false,180);
    tick(6,7010,true,180);
    tick(6,7100,false,180);
    for(std::uint64_t ms=7120;ms<7420;ms+=50){
        assert(tick(6,ms,true,180).pitchDegrees==0.f);
        assert(tick(6,ms+20,false,180).pitchDegrees==0.f);
    }
    tick(6,7430,true,180);
    assert(tick(6,7500,true,165).pitchDegrees==0.f);
    const auto gaussPunch=tick(6,7510,true,165);
    assert(gaussPunch.shotEstimated&&gaussPunch.pitchDegrees>30.f);
    const auto gaussStillRecovering=tick(6,8000,true,165);
    assert(gaussStillRecovering.pitchDegrees>0.f);
    const auto gaussRecovered=tick(6,8300,true,165);
    assert(gaussRecovered.pitchDegrees==0.f&&gaussRecovered.yawDegrees==0.f);

    // Recovery has NO 420ms low-speed saturation: settings 25% through
    // 900% all give different durations. Gauss is not shared scaling.
    const float quarter=gaussKickCycleMilliseconds(.25f);
    const float half=gaussKickCycleMilliseconds(.5f);
    const float normal=gaussKickCycleMilliseconds(1.f);
    const float doubled=gaussKickCycleMilliseconds(2.f);
    const float quadrupled=gaussKickCycleMilliseconds(4.f);
    const float maximum=gaussKickCycleMilliseconds(9.f);
    assert(quarter>700.f&&quarter>half&&half>normal
        &&normal>doubled&&doubled>quadrupled&&quadrupled>maximum);
    assert(std::abs(normal-260.f)<.001f);
    assert(maximum>80.f&&maximum<100.f);
    state={};
    settings.recovery[6]=9.f;
    tick(6,8100,false,165);
    tick(6,8110,true,165);
    tick(6,8140,true,150);
    assert(tick(6,8150,true,150).shotEstimated);
    const auto quickRest=tick(6,8270,true,150);
    assert(quickRest.pitchDegrees==0.f&&quickRest.yawDegrees==0.f);
    assert(tick(6,8350,true,150).pitchDegrees==0.f); // no extra shot

    // Gauss MUST keep its existing recovery motion through trigger
    // pulls that spend NO ammo. Previously each press cleared the active
    // kick to zero, making 25% and 900% feel nearly indistinguishable.
    state={};
    settings.recovery[6]=.25f;
    settings.strength[6]=9.f;
    tick(6,16000,false,150);
    assert(tick(6,16010,true,150).pitchDegrees==0.f);
    assert(tick(6,16020,true,135).pitchDegrees==0.f);
    const auto heavyShot=tick(6,16030,true,135);
    assert(heavyShot.shotEstimated&&heavyShot.pitchDegrees>30.f);
    const auto partial=tick(6,16120,false,135);
    assert(partial.pitchDegrees>0.f
        &&partial.pitchDegrees<heavyShot.pitchDegrees);
    const auto dryPress=tick(6,16130,true,135);
    assert(dryPress.pitchDegrees>0.f
        &&dryPress.pitchDegrees<partial.pitchDegrees
        &&!dryPress.shotEstimated);
    const auto dryHold=tick(6,16180,true,135);
    assert(dryHold.pitchDegrees>0.f
        &&dryHold.pitchDegrees<dryPress.pitchDegrees
        &&!dryHold.shotEstimated);
    const auto dryReleased=tick(6,16200,false,135);
    assert(dryReleased.pitchDegrees>0.f
        &&dryReleased.pitchDegrees<dryHold.pitchDegrees);
    const auto dryAgain=tick(6,16210,true,135);
    assert(dryAgain.pitchDegrees>0.f
        &&dryAgain.pitchDegrees<dryReleased.pitchDegrees
        &&!dryAgain.shotEstimated);
    assert(tick(6,16800,false,135).pitchDegrees==0.f);

    // An ammo-confirmed shot queued on RELEASE must survive an immediate
    // next trigger press without bending that fresh firing frame.
    state={};
    tick(6,17000,false,135);
    tick(6,17010,true,135);
    const auto queuedOnRelease=tick(6,17020,false,120);
    assert(!queuedOnRelease.shotEstimated
        &&queuedOnRelease.pitchDegrees==0.f
        &&state.confirmedShotQueued);
    const auto quickRepress=tick(6,17030,true,120);
    assert(!quickRepress.shotEstimated
        &&quickRepress.pitchDegrees==0.f
        &&state.confirmedShotQueued);
    const auto delivered=tick(6,17040,true,120);
    assert(delivered.shotEstimated&&delivered.pitchDegrees>0.f
        &&!state.confirmedShotQueued);

    // Gauss recoil is defined by NORMAL single-shot firing ONLY, not an
    // attachment declaration, charge flag or trigger-release heuristic.
    // Holding the trigger through a real ammo debit must kick BEFORE release.
    settings.recovery[6]=9.f;
    state={};
    tick(6,9000,false,150);
    assert(tick(6,9010,true,150).pitchDegrees==0.f);
    const auto spentWhileHeld=tick(6,9020,true,135);
    assert(!spentWhileHeld.shotEstimated
        &&spentWhileHeld.pitchDegrees==0.f);
    const auto kickWhileHeld=tick(6,9030,true,135);
    assert(kickWhileHeld.shotEstimated&&kickWhileHeld.pitchDegrees>0.f);
    const auto releasedNoShot=tick(6,9040,false,135);
    assert(!releasedNoShot.shotEstimated
        &&releasedNoShot.pitchDegrees>0.f
        &&releasedNoShot.pitchDegrees<kickWhileHeld.pitchDegrees);
    // Releasing after an unsuccessful trigger pull must NEVER kick.
    const auto settled=tick(6,9160,false,135);
    assert(settled.pitchDegrees==0.f);
    assert(tick(6,9170,true,135).pitchDegrees==0.f);
    const auto dryRelease=tick(6,9180,false,135);
    assert(!dryRelease.shotEstimated&&dryRelease.pitchDegrees==0.f);
    assert(!tick(6,9190,false,135).shotEstimated);
    // If the native ammo cache reports a real debit only after trigger-up,
    // THAT AMMO EVENT (not the release) can still confirm an actual shot.
    state={};
    tick(6,9300,false,135);
    tick(6,9310,true,135);
    assert(!tick(6,9320,false,135).shotEstimated);
    const auto lateDebit=tick(6,9330,false,120);
    assert(!lateDebit.shotEstimated&&lateDebit.pitchDegrees==0.f);
    const auto lateConfirmed=tick(6,9340,false,120);
    assert(lateConfirmed.shotEstimated&&lateConfirmed.pitchDegrees>0.f);

    // Shared dynamics run on ammo-confirmed events, not trigger counts.
    WeaponRecoilSettings ds{};
    ds.dynamicEnabled=false; // explicit legacy/off mode for this regression
    ds.dynamicBuildUpPerShot=.20f; // independent test curve, not factory .35
    assert(!ds.dynamicEnabled);
    assert(recoilDynamicMultiplier(ds,4,3)==1.f);
    ds.dynamicEnabled=true;
    ds.enabled[3]=true; // Plasma
    ds.enabled[6]=true; // Gauss
    assert(recoilDynamicMultiplier(ds,1,3)==1.f);
    assert(std::abs(recoilDynamicMultiplier(ds,2,3)-1.2f)<.0001f);
    assert(std::abs(recoilDynamicMultiplier(ds,3,3)-1.4f)<.0001f);
    assert(recoilDynamicMultiplier(ds,999,3)==3.f);
    assert(recoilDynamicMultiplier(ds,999,6)==1.f); // Gauss independent
    assert(std::abs(recoilDynamicRecoveryScale(1.5f)-1.2f)<.0001f);
    WeaponRecoilState dyn{};
    auto shot=[&](std::uint64_t ms,int ammo,bool fire=true,
                  int weapon=3,bool twoHands=false){
        return advanceWeaponRecoil(dyn,ds,
            {weapon,fire,twoHands,true,ms,ammo});
    };
    shot(13000,100,false);
    shot(13010,100);
    assert(!shot(13020,99).shotEstimated);
    assert(shot(13030,99).shotEstimated);
    assert(dyn.confirmedStreak==1u&&dyn.activeMultiplier==1.f);
    assert(!shot(13120,98).shotEstimated);
    assert(shot(13130,98).shotEstimated);
    assert(dyn.confirmedStreak==2u);
    assert(std::abs(dyn.activeMultiplier-1.2f)<.0001f);
    shot(13220,97);
    assert(shot(13230,97).shotEstimated);
    assert(std::abs(dyn.activeMultiplier-1.4f)<.0001f);
    shot(13300,97,false);shot(13310,97);
    assert(!shot(13350,97).shotEstimated);
    assert(dyn.confirmedStreak==3u); // no phantom extra count
    assert(!shot(14200,97).shotEstimated);
    assert(!shot(14210,96).shotEstimated);
    assert(shot(14220,96).shotEstimated);
    assert(dyn.confirmedStreak==1u); // streak expiration only
    shot(14230,95,true,3,true);
    assert(dyn.confirmedStreak==0u); // two hands cancel streak
    assert(!shot(14240,95).shotEstimated);
    dyn={};
    shot(15000,180,false,6);
    shot(15010,180,true,6);
    shot(15020,165,true,6);
    assert(shot(15030,165,true,6).shotEstimated);
    assert(dyn.activeMultiplier==1.f); // Gauss still its own tune

    // Per-weapon enable state and 900% limits remain schema-2 compatible.
    settings.recovery[6]=9.f;
    settings.recovery[7]=.25f;
    assert(validWeaponRecoilSettings(settings));
    settings.recovery[6]=9.01f;
    assert(!validWeaponRecoilSettings(settings));
    settings.recovery[6]=9.f;
    settings.recovery[7]=.24f;
    assert(!validWeaponRecoilSettings(settings));
    settings.recovery[7]=.25f;
    settings.strength[1]=9.1f;
    assert(!validWeaponRecoilSettings(settings));
    settings.strength[1]=9.f;

    const auto path=std::filesystem::temp_directory_path()/
        "kharvox_recoil_policy_test.cfg";
    std::filesystem::remove(path);
    settings.dynamicEnabled=true;
    settings.dynamicBuildUpPerShot=.35f;
    settings.dynamicMaxMultiplier=3.5f;
    assert(saveWeaponRecoilSettings(path.string(),settings));
    WeaponRecoilSettings loaded{};
    assert(loadWeaponRecoilSettings(path.string(),loaded));
    assert(loaded.recovery[6]==9.f&&loaded.recovery[7]==.25f);
    assert(loaded.strength[6]==9.f&&loaded.strength[1]==9.f);
    assert(loaded.dynamicEnabled);
    assert(std::abs(loaded.dynamicBuildUpPerShot-.35f)<.0001f);
    assert(std::abs(loaded.dynamicMaxMultiplier-3.5f)<.0001f);

    // Independent ON/OFF switches, including Minigun's two modes.
    settings.enabled.fill(false);
    settings.enabled[1]=true;
    settings.enabled[9]=true;
    assert(saveWeaponRecoilSettings(path.string(),settings));
    WeaponRecoilSettings independent{};
    assert(loadWeaponRecoilSettings(path.string(),independent));
    for(std::size_t i=0;i<10;++i)
        assert(independent.enabled[i]==(i==1||i==9));
    assert(independent.strength[7]==settings.strength[7]);
    assert(independent.strength[9]==settings.strength[9]);
    state={};
    assert(tick(7,12000,true,20).pitchDegrees==0.f);

    // Schema 3 dynamic tuning must not silently survive malformed saves.
    {
        std::ofstream truncated(path,std::ios::trunc);
        truncated<<"schema 3\ndynamic 1 0.200 3.000\n"
                 <<"shotgun 1 2.500 0.600\n";
    }
    assert(!loadWeaponRecoilSettings(path.string(),independent));
    assert(independent.enabled[1]&&independent.enabled[9]
        &&independent.dynamicEnabled);
    // Old schema 2 profiles migrate with dynamics OFF, preserving gun tune.
    {
        std::ofstream old(path,std::ios::trunc);
        old<<"schema 2\n";
        for(std::size_t i=0;i<kWeaponRecoilKinds.size();++i)
            old<<kWeaponRecoilKinds[i]<<" "<<(i==9?1:0)
               <<" 1.500 0.250\n";
    }
    WeaponRecoilSettings oldProfile{};
    assert(loadWeaponRecoilSettings(path.string(),oldProfile));
    assert(!oldProfile.dynamicEnabled
        &&oldProfile.dynamicBuildUpPerShot==.20f
        &&oldProfile.dynamicMaxMultiplier==3.f);
    assert(oldProfile.enabled[9]&&!oldProfile.enabled[7]
        &&oldProfile.strength[9]==1.5f);

    // Legacy schema-1 global ON/OFF correctly migrates all ten flags.
    {
        std::ofstream legacy(path,std::ios::trunc);
        legacy<<"schema 1\nenabled 1\n";
        for(std::size_t i=0;i<9;++i)
            legacy<<kWeaponRecoilKinds[i]<<" 1.500 0.250\n";
    }
    WeaponRecoilSettings migrated{};
    assert(loadWeaponRecoilSettings(path.string(),migrated));
    for(std::size_t i=0;i<9;++i){
        assert(migrated.enabled[i]);
        assert(migrated.strength[i]==1.5f&&migrated.recovery[i]==.25f);
    }
    assert(migrated.enabled[9]);
    assert(!migrated.dynamicEnabled);
    assert(migrated.strength[9]==kWeaponRecoilDefaultStrength[9]);
    assert(migrated.recovery[9]==kWeaponRecoilDefaultRecovery[9]);
    assert(saveWeaponRecoilSettings(path.string(),migrated));
    assert(loadWeaponRecoilSettings(path.string(),loaded));
    assert(loaded.strength[7]==1.5f&&loaded.strength[9]==3.f);
    {
        std::ofstream legacyOff(path,std::ios::trunc);
        legacyOff<<"schema 1\nenabled 0\n";
        for(std::size_t i=0;i<9;++i)
            legacyOff<<kWeaponRecoilKinds[i]<<" 1.750 0.300\n";
    }
    WeaponRecoilSettings legacyDisabled{};
    assert(loadWeaponRecoilSettings(path.string(),legacyDisabled));
    assert(!anyWeaponRecoilEnabled(legacyDisabled));
    assert(!legacyDisabled.dynamicEnabled);
    assert(legacyDisabled.strength[7]==1.75f);
    {
        std::ofstream bad(path,std::ios::trunc);
        bad<<"schema 1\nenabled 1\nshotgun nan 1.0\n";
    }
    assert(!loadWeaponRecoilSettings(path.string(),loaded));
    std::filesystem::remove(path);
    return 0;
}
