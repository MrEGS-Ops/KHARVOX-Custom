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
    std::uint32_t automaticIntervalMs{};
};
inline constexpr std::array<WeaponRecoilProfile,10> kWeaponRecoilProfiles{{
    {1.6f,.24f,6.f,0},   // pistol
    {3.7f,.45f,10.f,0},  // combat shotgun
    {.65f,.18f,11.f,95}, // heavy assault rifle
    {.43f,.15f,9.f,90},  // plasma rifle
    {4.4f,.45f,10.f,0},  // rocket launcher
    {5.4f,.65f,13.f,0},  // super shotgun
    {4.2f,.38f,11.f,0},  // gauss cannon
    {.75f,.24f,15.f,75}, // chaingun
    {5.0f,.3f,12.f,0},  // BFG
    {.55f,.15f,11.f,75}  // Minigun Turret (one-handed only)
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
};
inline bool anyWeaponRecoilEnabled(const WeaponRecoilSettings& settings) noexcept {
    return std::any_of(settings.enabled.begin(),settings.enabled.end(),
        [](bool enabled){return enabled;});
}
inline bool validWeaponRecoilSettings(const WeaponRecoilSettings& settings) noexcept {
    for(const float v:settings.strength)
        if(!std::isfinite(v)||v<.25f||v>9.f)return false;
    for(const float v:settings.recovery)
        if(!std::isfinite(v)||v<.25f||v>2.f)return false;
    return true;
}
struct WeaponRecoilState {
    float fastPitch{},climbPitch{},yaw{};
    int weapon{-1};
    bool fireHeld{},pendingShot{};
    std::uint64_t lastTick{},lastShotTick{},pendingShotTick{};
    std::uint32_t shotNumber{};
};
struct WeaponRecoilInput {
    int weapon{-1};
    bool fireHeld{},twoHands{},gameplay{};
    std::uint64_t nowMs{};
};
struct WeaponRecoilOutput {
    float pitchDegrees{},yawDegrees{};
    bool shotEstimated{};
};
// A trigger edge is NOT a projectile event. DOOM consumes its native Fire
// input separately from our OpenXR render update; previously adding kick on
// that same frame bent the very first shot. Keep the submitted muzzle/weapon
// pose untouched at first input, then commit each queued kick only AFTER a
// subsequent gameplay tick and a short native-fire grace period. This is an
// input-level estimate, not an asserted native shot confirmation.
inline constexpr std::uint64_t kWeaponRecoilPostFireDelayMs=55;
inline WeaponRecoilOutput advanceWeaponRecoil(
    WeaponRecoilState& state,const WeaponRecoilSettings& settings,
    WeaponRecoilInput input) noexcept {
    if(!input.gameplay||input.weapon<0
        ||input.weapon>=static_cast<int>(kWeaponRecoilProfiles.size())
        ||!validWeaponRecoilSettings(settings)
        ||!settings.enabled[static_cast<std::size_t>(input.weapon)]){
        state={};
        return {};
    }
    if(state.weapon!=input.weapon){
        state={};state.weapon=input.weapon;state.lastTick=input.nowMs;
    }
    // Every new trigger pull gets an uncompromised first-shot trajectory.
    // Recoil can accumulate only from continued hold on an automatic
    // weapon; a new tap doesn't inherit a stale kick from another tap.
    const bool triggerEdge=input.fireHeld&&!state.fireHeld;
    if(triggerEdge){
        state.fastPitch=0.f;
        state.climbPitch=0.f;
        state.yaw=0.f;
        state.pendingShot=false;
    }
    const auto rawDelta=input.nowMs>=state.lastTick
        ?input.nowMs-state.lastTick:0u;
    const float dt=static_cast<float>(std::min<std::uint64_t>(rawDelta,80u));
    state.lastTick=input.nowMs;
    const std::size_t index=static_cast<std::size_t>(input.weapon);
    const float recovery=settings.recovery[index];
    const float fastMs=input.twoHands?90.f:105.f/recovery;
    const float slowMs=input.twoHands?115.f:(input.fireHeld?1300.f:480.f)/recovery;
    const float yawMs=input.twoHands?95.f:(input.fireHeld?450.f:200.f)/recovery;
    state.fastPitch*=std::exp(-dt/fastMs);
    state.climbPitch*=std::exp(-dt/slowMs);
    state.yaw*=std::exp(-dt/yawMs);
    const auto& profile=kWeaponRecoilProfiles[index];
    bool kickApplied=false;
    if(input.twoHands){
        // A pending one-handed kick must never fire after the second hand
        // locks on, nor after holstering/weapon switching (handled above).
        state.pendingShot=false;
    }else if(state.pendingShot
        &&input.nowMs>state.pendingShotTick
        &&input.nowMs-state.pendingShotTick>=kWeaponRecoilPostFireDelayMs){
        const float scale=settings.strength[index];
        // 900% scales the actual impulse and kick ceiling, not only the UI.
        const float maxPitch=profile.maximumPitch*(std::max)(1.f,scale);
        state.fastPitch=(std::min)(maxPitch*.45f,
            state.fastPitch+profile.pitchPerShot*.38f*scale);
        state.climbPitch=(std::min)(maxPitch,
            state.climbPitch+profile.pitchPerShot*.62f*scale);
        const float side=(state.shotNumber++%4u<2u)?1.f:-1.f;
        state.yaw=std::clamp(state.yaw+side*profile.yawPerShot*scale,
            -maxPitch*.3f,maxPitch*.3f);
        state.pendingShot=false;
        kickApplied=true;
    }
    if(!input.twoHands&&input.fireHeld){
        const bool repeated=profile.automaticIntervalMs
            &&input.nowMs>=state.lastShotTick
            &&input.nowMs-state.lastShotTick>=profile.automaticIntervalMs;
        if((triggerEdge||repeated)&&!state.pendingShot){
            // Schedule, don't rotate the muzzle on the fire-input frame.
            state.pendingShot=true;
            state.pendingShotTick=input.nowMs;
            state.lastShotTick=input.nowMs;
        }
    }
    state.fireHeld=input.fireHeld;
    const float maxPitch=profile.maximumPitch*
        (std::max)(1.f,settings.strength[index]);
    return {(std::min)(maxPitch,state.fastPitch+state.climbPitch),
            state.yaw,kickApplied};
}
// schema 2 stores an independent toggle per weapon. Previous schema 1
// stored a global enable and two tuning columns; migrate the global value
// to ALL weapons (including a newly introduced Turret profile) on load.
// The original file is never rewritten until the user saves a setting.
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
