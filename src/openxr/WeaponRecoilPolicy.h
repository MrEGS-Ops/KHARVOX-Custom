#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#ifdef _WIN32
#include <windows.h>
#endif

// One-handed recoil is an additive, controller-local *weapon* rotation.
// Never move the HMD, physical controller/grip anchor, or saved hand calibration.
// All values are owned by this opt-in mod and cannot change factory profiles.
namespace kharvox {
inline constexpr std::array<const char*,10> kWeaponRecoilKinds{{
    "pistol","shotgun","heavy_assault_rifle","plasma_rifle",
    "rocket_launcher","super_shotgun","gauss_cannon","chaingun","bfg",
    "chaingun_turret" // appended: preserves the 9 legacy saved profile keys
}};
struct WeaponRecoilProfile {
    float pitchPerShot{},yawPerShot{},maximumPitch{};
};
inline constexpr std::array<WeaponRecoilProfile,10> kWeaponRecoilProfiles{{
    {1.6f,.24f,6.f},   // pistol
    {3.7f,.45f,10.f},  // combat shotgun
    {.65f,.18f,11.f}, // heavy assault rifle
    {.43f,.15f,9.f},  // plasma rifle
    {4.4f,.45f,10.f},  // rocket launcher
    {5.4f,.65f,13.f},  // super shotgun
    {4.2f,.38f,11.f},  // gauss cannon
    {.75f,.24f,15.f}, // chaingun
    {5.0f,.3f,12.f},  // BFG
    {.55f,.15f,11.f}  // Minigun Turret (one-handed only)
}};
// A separate native recoil identity for deployed Minigun Turret mode.
 // Normal Minigun retains its original array index and saved profile.
inline int weaponRecoilIndex(const char* weapon,bool turretMode=false) noexcept {
    if(!weapon)return -1;
    if(turretMode && std::strcmp(weapon,"chaingun")==0)
        return static_cast<int>(kWeaponRecoilKinds.size()-1);
    for(std::size_t i=0;i<kWeaponRecoilKinds.size();++i)
        if(std::strcmp(weapon,kWeaponRecoilKinds[i])==0)return static_cast<int>(i);
    return -1;
}
// Proposed aggressive-but-controllable initial balance: higher values
// reward two-handed tracking; none overwrite explicitly saved UserData.
inline constexpr std::array<float,10> kWeaponRecoilDefaultStrength{{
    2.00f,2.50f,2.40f,2.25f,2.70f,3.00f,2.60f,3.00f,3.20f,1.70f
}};
inline constexpr std::array<float,10> kWeaponRecoilDefaultRecovery{{
    .70f,.60f,.55f,.60f,.60f,.55f,.60f,.45f,.55f,.75f
}};
struct WeaponRecoilSettings {
    // Independent enable flags. All OFF on fresh installs to preserve
    // the accepted KHARVOX baseline; normal and Turret are separate slots.
    std::array<bool,10> enabled{};
    std::array<float,10> strength=kWeaponRecoilDefaultStrength;
    std::array<float,10> recovery=kWeaponRecoilDefaultRecovery;
    // Optional shared build-up formula; existing configurations remain
    // unchanged unless the player opts in from the VR calibration page.
    bool dynamicEnabled{};
    float dynamicBuildUpPerShot{.20f}; // +20% per confirmed ammo event
    float dynamicMaxMultiplier{3.f}; // 300% ceiling over weapon baseline
};
// Secondary-fire Grip drives the Heavy Assault Rifle's micro-missiles/scope.
// Never contaminate that alternate mode with the primary-fire recoil profile.
// Gauss and Minigun manage their own alternate behaviour separately.
inline bool recoilAllowedForWeaponMode(int recoilWeapon,
    bool secondaryFireForwarded) noexcept {
    return !(recoilWeapon==weaponRecoilIndex("heavy_assault_rifle")
        &&secondaryFireForwarded);
}
inline bool anyWeaponRecoilEnabled(const WeaponRecoilSettings& settings) noexcept {
    return std::any_of(settings.enabled.begin(),settings.enabled.end(),
        [](bool enabled){return enabled;});
}
inline bool validWeaponRecoilSettings(const WeaponRecoilSettings& settings) noexcept {
    if(!std::isfinite(settings.dynamicBuildUpPerShot)
        ||settings.dynamicBuildUpPerShot<.1f
        ||settings.dynamicBuildUpPerShot>.5f
        ||!std::isfinite(settings.dynamicMaxMultiplier)
        ||settings.dynamicMaxMultiplier<2.f
        ||settings.dynamicMaxMultiplier>4.f)
        return false;
    for(const float v:settings.strength)
        if(!std::isfinite(v)||v<.25f||v>9.f)return false;
    for(const float v:settings.recovery)
        if(!std::isfinite(v)||v<.25f||v>9.f)return false;
    return true;
}
struct WeaponRecoilState {
    float fastPitch{},climbPitch{},yaw{};
    int weapon{-1},lastAmmoTotal{-1};
    bool fireHeld{},confirmedShotQueued{};
    bool gaussCharging{},gaussAmmoSpentDuringCharge{};
    std::uint64_t lastTick{},lastTriggerTick{},gaussKickTick{};
    std::uint64_t lastConfirmedShotTick{};
    float gaussKickPitch{},gaussKickYaw{};
    float queuedMultiplier{1.f},activeMultiplier{1.f};
    std::uint32_t shotNumber{},confirmedStreak{};
};
struct WeaponRecoilInput {
    int weapon{-1};
    bool fireHeld{},twoHands{},gameplay{};
    std::uint64_t nowMs{};
    // Native Siege may charge until trigger release. Never kick on a
    // changing ammo total mid-charge, only after the release is complete.
    bool fireOnRelease{};
    // Read-only, validated DOOM ammo snapshot. -1 = missing/stale.
    int confirmedAmmoTotal{-1};
};
struct WeaponRecoilOutput {
    float pitchDegrees{},yawDegrees{};
    bool shotEstimated{}; // historical API name: now means native ammo-confirmed
};
// The recoil source is an OBSERVED DOOM AMMO DEBIT, never elapsed time after
// a trigger pull. The native ammo snapshot may coalesce several shots on a
// fast automatic into one debit; one kick per observed change is deliberate.
// Defer the recoil just ONE render update after confirming the ammo decrease
// so no firing frame can have its original muzzle trajectory modified.
inline float gaussKickCycleMilliseconds(float recovery) noexcept {
    // Full zero after 420ms at 25%; 260ms at 100%; 87ms at 900%.
    // High kick, short return; no exponential tail into the next shot.
    return (std::clamp)(260.f/std::sqrt(recovery),70.f,420.f);
}
// Shared build-up applies to finite-ammo guns EXCEPT Gauss (its single-shot
// charge, punch and fast-return remain independently tuned per weapon).
// No shot timer: 700ms only expires a streak; ammunition debit alone kicks.
inline constexpr std::uint64_t kRecoilComboResetMs=700;
inline float recoilDynamicMultiplier(const WeaponRecoilSettings& settings,
                                     std::uint32_t streak,
                                     int weapon) noexcept {
    if(!settings.dynamicEnabled||weapon==6||streak==0)return 1.f;
    return (std::min)(settings.dynamicMaxMultiplier,
        1.f+(static_cast<float>(streak-1u)*settings.dynamicBuildUpPerShot));
}
// Linked recovery grows more gently than kick, so every shot remains
// stronger but settles faster instead of creating an unbounded offset.
inline float recoilDynamicRecoveryScale(float multiplier) noexcept {
    return 1.f+.4f*(multiplier-1.f);
}
inline WeaponRecoilOutput advanceWeaponRecoil(
    WeaponRecoilState& state,const WeaponRecoilSettings& settings,
    WeaponRecoilInput input) noexcept {
    // The pistol is intentionally excluded: its infinite ammo cannot
    // provide a reliable shot confirmation, and its owner wants no recoil.
    if(!input.gameplay||input.weapon<=0
        ||input.weapon>=static_cast<int>(kWeaponRecoilProfiles.size())
        ||!validWeaponRecoilSettings(settings)
        ||!settings.enabled[static_cast<std::size_t>(input.weapon)]){
        state={};
        return {};
    }
    if(state.weapon!=input.weapon){
        state={};state.weapon=input.weapon;state.lastTick=input.nowMs;
    }
    const bool triggerEdge=input.fireHeld&&!state.fireHeld;
    const bool triggerRelease=!input.fireHeld&&state.fireHeld;
    if(triggerEdge){
        // Every NEW trigger press starts from original aim. No old
        // recovery or delayed ammo confirmation may bend a first shot.
        state.fastPitch=state.climbPitch=state.yaw=0.f;
        state.gaussKickTick=0;
        state.gaussKickPitch=state.gaussKickYaw=0.f;
        state.confirmedShotQueued=false;
        state.activeMultiplier=1.f;
        state.gaussCharging=input.weapon==6&&input.fireOnRelease;
        state.gaussAmmoSpentDuringCharge=false;
    }
    if(input.weapon==6&&input.fireHeld&&input.fireOnRelease)
        state.gaussCharging=true;
    if(input.fireHeld)
        state.lastTriggerTick=input.nowMs;
    const auto rawDelta=input.nowMs>=state.lastTick
        ?input.nowMs-state.lastTick:0u;
    const float dt=static_cast<float>((std::min<std::uint64_t>)(rawDelta,80u));
    state.lastTick=input.nowMs;
    const std::size_t index=static_cast<std::size_t>(input.weapon);
    const float recovery=(std::min)(9.f,
        settings.recovery[index]*
        recoilDynamicRecoveryScale(state.activeMultiplier));
    const float fastMs=input.twoHands?90.f:105.f/recovery;
    const float slowMs=input.twoHands?115.f:(input.fireHeld?1300.f:480.f)/recovery;
    const float yawMs=input.twoHands?95.f:(input.fireHeld?450.f:200.f)/recovery;
    state.fastPitch*=std::exp(-dt/fastMs);
    state.climbPitch*=std::exp(-dt/slowMs);
    state.yaw*=std::exp(-dt/yawMs);
    const auto& profile=kWeaponRecoilProfiles[index];
    bool kickApplied=false;
    // Process ONLY a shot confirmed on a previous render update.
    if(state.confirmedShotQueued&&!input.twoHands&&!triggerEdge){
        const float scale=settings.strength[index]*state.queuedMultiplier;
        state.activeMultiplier=state.queuedMultiplier;
        const float maxPitch=profile.maximumPitch*(std::max)(1.f,scale);
        const float side=(state.shotNumber++%4u<2u)?1.f:-1.f;
        if(input.weapon==6){
            state.gaussKickPitch=(std::min)(maxPitch,
                profile.pitchPerShot*scale*1.65f);
            state.gaussKickYaw=side*profile.yawPerShot*scale;
            state.gaussKickTick=input.nowMs;
            state.fastPitch=state.climbPitch=state.yaw=0.f;
        }else{
            state.fastPitch=(std::min)(maxPitch*.45f,
                state.fastPitch+profile.pitchPerShot*.38f*scale);
            state.climbPitch=(std::min)(maxPitch,
                state.climbPitch+profile.pitchPerShot*.62f*scale);
            state.yaw=(std::clamp)(state.yaw+side*profile.yawPerShot*scale,
                -maxPitch*.3f,maxPitch*.3f);
        }
        kickApplied=true;
    }
    state.confirmedShotQueued=false;
    if(input.twoHands){
        // Native two-hand aiming wins, including pending recoil.
        state.fastPitch=state.climbPitch=state.yaw=0.f;
        state.gaussKickTick=0;
        state.gaussKickPitch=state.gaussKickYaw=0.f;
        state.gaussAmmoSpentDuringCharge=false;
        state.confirmedStreak=0;
        state.lastConfirmedShotTick=0;
        state.activeMultiplier=1.f;
    }
    // Observe native AMMO, not trigger frequency. A missing/stale value
    // breaks the baseline, preventing phantom kicks when it reappears.
    const bool ammoDebit=input.confirmedAmmoTotal>=0
        &&state.lastAmmoTotal>=0
        &&input.confirmedAmmoTotal<state.lastAmmoTotal;
    state.lastAmmoTotal=input.confirmedAmmoTotal;
    // Trigger is an *authorization*, not a shot. The short trailing window
    // accommodates the inventory snapshot's 100ms polling interval AFTER
    // single-shot trigger release. Elapsed time alone never creates recoil.
    const bool recentlyFired=state.lastTriggerTick
        &&input.nowMs>=state.lastTriggerTick
        &&input.nowMs-state.lastTriggerTick<=500u;
    const auto confirmShot=[&](){
        // Count observed ammo-debit events rather than bullets, since
        // rapid automatic fire may batch multiple rounds per snapshot.
        const bool inCombo=state.lastConfirmedShotTick
            &&input.nowMs>=state.lastConfirmedShotTick
            &&input.nowMs-state.lastConfirmedShotTick<=kRecoilComboResetMs;
        state.confirmedStreak=inCombo
            ?(std::min)(state.confirmedStreak+1u,1000u):1u;
        state.lastConfirmedShotTick=input.nowMs;
        state.queuedMultiplier=recoilDynamicMultiplier(
            settings,state.confirmedStreak,input.weapon);
        state.confirmedShotQueued=true;
    };
    if(ammoDebit&&!input.twoHands&&recentlyFired){
        if(input.weapon==6&&state.gaussCharging&&input.fireHeld){
            // Siege may debit energy while charging, before the projectile.
            state.gaussAmmoSpentDuringCharge=true;
        }else{
            confirmShot();
            state.gaussAmmoSpentDuringCharge=false;
        }
    }
    if(input.weapon==6&&triggerRelease){
        if(state.gaussAmmoSpentDuringCharge&&!input.twoHands)
            confirmShot();
        state.gaussAmmoSpentDuringCharge=false;
        state.gaussCharging=false;
    }
    state.fireHeld=input.fireHeld;
    if(input.weapon==6){
        if(!state.gaussKickTick)return {0.f,0.f,kickApplied};
        const float elapsed=static_cast<float>(input.nowMs>=state.gaussKickTick
            ?input.nowMs-state.gaussKickTick:0u);
        const float gaussRecovery=(std::min)(9.f,settings.recovery[index]*
            recoilDynamicRecoveryScale(state.activeMultiplier));
        const float t=(std::clamp)(elapsed/gaussKickCycleMilliseconds(gaussRecovery),
            0.f,1.f);
        const float envelope=1.f-t*t*(3.f-2.f*t);
        if(t>=1.f){
            state.gaussKickTick=0;
            state.gaussKickPitch=state.gaussKickYaw=0.f;
        }
        return {state.gaussKickPitch*envelope,
                state.gaussKickYaw*envelope,kickApplied};
    }
    const float maxPitch=profile.maximumPitch*
        (std::max)(1.f,settings.strength[index]*
                   state.activeMultiplier);
    return {(std::min)(maxPitch,state.fastPitch+state.climbPitch),
            state.yaw,kickApplied};
}
inline bool loadWeaponRecoilSettings(const std::string& path,WeaponRecoilSettings& out){
    namespace fs=std::filesystem;
    std::error_code ec;
    if(!fs::exists(path,ec)||ec)return !ec;
    if(fs::file_size(path,ec)>4096u||ec)return false;
    std::ifstream file(path);
    if(!file)return false;
    WeaponRecoilSettings next{};
    std::string key,line;
    int schema{};
    bool schemaSeen=false,legacyEnabledSeen=false,readWeapon=false;
    bool legacyEnabled=false;
    std::array<bool,kWeaponRecoilKinds.size()> seen{};
    while(std::getline(file,line)){
        if(line.empty()||line[0]=='#')continue;
        std::istringstream row(line);
        if(!(row>>key))continue;
        std::string trailing;
        if(key=="schema"){
            if(schemaSeen||readWeapon||legacyEnabledSeen
                ||!(row>>schema)||(schema!=1&&schema!=2)||(row>>trailing))
                return false;
            schemaSeen=true;
        }else if(key=="enabled"){
            if(!schemaSeen||schema!=1)return false;
            int value=-1;
            if(legacyEnabledSeen||readWeapon||!(row>>value)
                ||(value!=0&&value!=1)||(row>>trailing))
                return false;
            legacyEnabled=value==1;legacyEnabledSeen=true;
        }else{
            if(!schemaSeen)return false;
            const int index=weaponRecoilIndex(key.c_str());
            if(index<0||seen[static_cast<std::size_t>(index)])return false;
            const auto at=static_cast<std::size_t>(index);
            float strength{},recovery{};
            if(schema==2){
                int on=-1;
                if(!(row>>on)||(on!=0&&on!=1)
                    ||!(row>>strength>>recovery)||(row>>trailing))
                    return false;
                next.enabled[at]=on==1;
            }else{
                if(!legacyEnabledSeen||!(row>>strength>>recovery)
                    ||(row>>trailing))return false;
            }
            next.strength[at]=strength;
            next.recovery[at]=recovery;
            seen[at]=true;readWeapon=true;
        }
    }
    if(file.bad()||!schemaSeen||!validWeaponRecoilSettings(next))
        return false;
    if(schema==1){
        if(!legacyEnabledSeen)return false;
        next.enabled.fill(legacyEnabled);
    }else if(!std::all_of(seen.begin(),seen.end(),
        [](bool present){return present;})){
        // No partial schema-2 files: a truncated write must never silently
        // turn off previously enabled guns or discard their tuning.
        return false;
    }
    out=next;
    return true;
}
inline bool saveWeaponRecoilSettings(const std::string& path,
                                     const WeaponRecoilSettings& settings){
    if(!validWeaponRecoilSettings(settings))return false;
    namespace fs=std::filesystem;
    const fs::path dest(path),tmp(path+".tmp");
    std::error_code ec;
    if(!dest.parent_path().empty()){
        fs::create_directories(dest.parent_path(),ec);
        if(ec)return false;
    }
    {
        std::ofstream file(tmp,std::ios::trunc);
        if(!file)return false;
        file<<std::fixed<<std::setprecision(3)
            <<"schema 2\n";
        for(std::size_t i=0;i<kWeaponRecoilKinds.size();++i)
            file<<kWeaponRecoilKinds[i]<<' '<<(settings.enabled[i]?1:0)
                <<' '<<settings.strength[i]<<' '<<settings.recovery[i]<<'\n';
        file.flush();
        if(!file){file.close();fs::remove(tmp,ec);return false;}
    }
#ifdef _WIN32
    if(!MoveFileExW(tmp.wstring().c_str(),dest.wstring().c_str(),
        MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){
        fs::remove(tmp,ec);return false;
    }
#else
    fs::rename(tmp,dest,ec);
    if(ec){fs::remove(tmp,ec);return false;}
#endif
    return true;
}
} // namespace kharvox
