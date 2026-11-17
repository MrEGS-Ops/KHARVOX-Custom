#pragma once
#include "../hud/CampaignDeathCounterPolicy.h"
#include "../hud/CampaignStatsPolicy.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

// All values are scoped to the current OpenXR session and forwarded to
// gameplay on the NEXT frame; changes are never loaded by restarting DOOM.
// Only this file contains owner-edited, separately persisted overrides.
// No shipped calibration/default cfg file is mutated.
namespace kharvox {
struct VrLivePoint { float x{},y{},z{}; };
// Owner-calibrated right/left Gunslinger hip zone SHIFT values captured
// in-headset on 2026-11-14, not controller positions or wheel offsets.
// Keep existing UserData overrides as-is; these apply on fresh installs
// and when Gunslinger's in-game Reset is explicitly selected.
inline constexpr std::array<VrLivePoint,2> kVrLiveFactoryHipShifts{{
    {-0.0397179f,0.0424916f,-0.0611160f}, // RIGHT hip
    { 0.0193224f,0.0286265f,-0.1343255f}  // LEFT hip
}};
enum class VrLiveMod : int {
    WeaponGrip, Gunslinger, PhysicalCrouch, Killtime,
    PhysicalGloryKills, HandFocus, Painsaw,
    HandsJump, BoxingGuard, Other, WeaponRecoil, TurnSpeed, DeathCounter, Stats
};
// "Has live settings" is NOT the same as "can be enabled/disabled".
// The three core mods stay available; only Physical Crouch and PAINSAW
// have ON/OFF checkboxes. Physical Glory Kills retains its desktop state
// but may still expose punch thresholds/hand controls in this menu.
inline constexpr bool vrLiveModEditable(VrLiveMod id) noexcept {
    return id==VrLiveMod::WeaponGrip||id==VrLiveMod::Gunslinger
        ||id==VrLiveMod::PhysicalCrouch||id==VrLiveMod::Killtime
        ||id==VrLiveMod::PhysicalGloryKills||id==VrLiveMod::Painsaw
        ||id==VrLiveMod::WeaponRecoil||id==VrLiveMod::TurnSpeed
        ||id==VrLiveMod::DeathCounter;
}
inline constexpr bool vrLiveModToggleable(VrLiveMod id) noexcept {
    return id==VrLiveMod::PhysicalCrouch||id==VrLiveMod::Painsaw
        ||id==VrLiveMod::DeathCounter;
}
inline constexpr bool vrLiveModAlwaysEnabled(VrLiveMod id) noexcept {
    return id==VrLiveMod::WeaponGrip||id==VrLiveMod::Gunslinger
        ||id==VrLiveMod::Killtime||id==VrLiveMod::TurnSpeed;
}
inline const char* vrLiveModLabel(VrLiveMod id) noexcept {
    switch(id){
        case VrLiveMod::WeaponGrip:return "Weapon Grip Calibration";
        case VrLiveMod::Gunslinger:return "Gunslinger";
        case VrLiveMod::PhysicalCrouch:return "Physical Crouch";
        case VrLiveMod::Killtime:return "KILLTIME";
        case VrLiveMod::PhysicalGloryKills:return "Physical Glory Kills";
        case VrLiveMod::HandFocus:return "Hand Focus";
        case VrLiveMod::Painsaw:return "PAINSAW";
        case VrLiveMod::WeaponRecoil:return "Weapon Stability";
case VrLiveMod::TurnSpeed:return "Smooth Turn Speed";
        case VrLiveMod::HandsJump:return "Hands Jump";
        case VrLiveMod::BoxingGuard:return "Boxing Guard";
        case VrLiveMod::DeathCounter:return "Campaign Deaths";
        case VrLiveMod::Stats:return "Stats";
        default:return "Other active mod";
    }
}
enum class VrLiveCapture : int {
    None,HipRight,HipLeft,WheelRight,WheelLeft,CrouchStanding,CrouchLowered
};
inline constexpr int vrLiveToggleIndex(VrLiveMod mod) {
    switch(mod){
        case VrLiveMod::WeaponGrip:return 0;
        case VrLiveMod::Gunslinger:return 1;
        case VrLiveMod::PhysicalCrouch:return 2;
        case VrLiveMod::Killtime:return 3;
        case VrLiveMod::PhysicalGloryKills:return 4;
        case VrLiveMod::Painsaw:return 5;
        default:return -1;
    }
}
struct VrLiveSettings {
    // -1 follows desktop startup status (and preserves existing defaults);
    // 0/1 is an explicit live menu override saved across sessions.
    // Slot 5 appended for PAINSAW; old five-slot saves still load.
    std::array<int,6> enabledOverrides{{-1,-1,1,-1,-1,1}}; // crouch + PAINSAW ON on fresh installs
    // Offset from the existing tracking-relative zones. On a fresh
    // installation inherit the owner's measured right/left hip centres;
    // an already-saved UserData file (including old zero shifts) wins.
    std::array<VrLivePoint,2> hipShift{kVrLiveFactoryHipShifts};
    std::array<VrLivePoint,2> wheelShift{};
    float crouchStandDelta{0.f};
    float crouchLoweredDelta{-0.56f};
    bool crouchStandSaved{},crouchLoweredSaved{};
    bool gaussRequireTwoHandFire{true}; // Gauss trigger needs BOTH physical Grips and an acquired support point
    int gloryKillHz{40}; // KILLTIME factory default; 0 in legacy saves still inherits launcher Hz
    float smoothTurnSpeedDegreesPerSec{0.f}; // 0 inherits the desktop launcher speed
float punchSpeedMetersPerSec{2.8f};
    int punchHands{2}; // 0 right, 1 left, 2 both
    bool deathCounterEnabled{}; // optional: never clutter the headset by default
    int deathCounterCorner{1}; // 0..3 legacy corners; 100..219 fine grid positions
    bool deathStatisticsEnabled{};
    int deathStatisticsCorner{0}; // fresh install defaults opposite Campaign Deaths
    bool campaignTimerEnabled{};
    int campaignTimerCorner{3}; // fresh install bottom right; existing saved values take priority
    bool sessionTimerEnabled{}; // session elapsed is NOT persisted, only this display preference
    int sessionTimerCorner{2};
    // Level readouts remain opt-in; records are stored separately.
    std::array<bool,3> levelDisplayEnabled{{false,false,false}};
    std::array<int,3> levelDisplayPosition{{117,129,141}};
};
// Native Sync Action pacing is inverse to the displayed speed: lower Hz
// gives a faster kill, higher Hz a slower one (30 = fast, 240 = slow).
// Legacy unset (0) starts at the new 40-Hz factory default.
inline int vrLiveStepKilltimeHz(int currentHz,bool slower) noexcept {
    const int current=currentHz==0?40:currentHz;
    return std::clamp(current+(slower?10:-10),30,240);
}
// Smooth turning is an OpenXR setting and changes immediately in VR.
// Zero means inherit the desktop launch value. Native snap angle is unaffected.
inline float vrLiveStepTurnSpeed(float saved,float desktop,int direction) noexcept {
    const float current=saved>0.f?saved:std::clamp(desktop,150.f,400.f);
    return std::clamp(current+(direction>0?25.f:-25.f),150.f,400.f);
}
inline constexpr VrLivePoint vrLiveHipDefaultCenter() { return {.315f,-.76f,.225f}; }
inline constexpr VrLivePoint vrLiveWheelDefaultCenter() { return {.20f,.09f,.3075f}; }
inline bool vrLiveValidPoint(VrLivePoint p) {
    return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z)
        &&p.x>=-.45f&&p.x<=.8f&&p.y>=-1.6f&&p.y<=.7f
        &&p.z>=-.9f&&p.z<=1.2f;
}
inline bool vrLiveValidShift(VrLivePoint p) {
    return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z)
        &&std::abs(p.x)<=.7f&&std::abs(p.y)<=.9f&&std::abs(p.z)<=1.f;
}
// Campaign and Session Deaths are independent counters and HUD displays.
// A new HUD position replaces any existing physically overlapping display.
// Old 0..3 positions load unchanged; fine placement uses a 12 x 10 grid.
// Hidden counters continue collecting deaths unconditionally.
inline bool vrStatsEnabled(const VrLiveSettings& s,int item) noexcept {
    if(item==0)return s.deathCounterEnabled;
    if(item==1)return s.deathStatisticsEnabled;
    if(item==2)return s.campaignTimerEnabled;
    if(item==3)return s.sessionTimerEnabled;
    return item>=4&&item<7?s.levelDisplayEnabled[item-4]:false;
}
inline int vrStatsCorner(const VrLiveSettings& s,int item) noexcept {
    if(item==0)return s.deathCounterCorner;
    if(item==1)return s.deathStatisticsCorner;
    if(item==2)return s.campaignTimerCorner;
    if(item==3)return s.sessionTimerCorner;
    return item>=4&&item<7?s.levelDisplayPosition[item-4]:-1;
}
inline constexpr const char* vrStatsLabel(int item) noexcept {
    switch(item){
        case 0:return "Campaign Deaths";
        case 1:return "Session Deaths";
        case 2:return "Campaign Run Timer";
        case 3:return "Session Timer";
        case 4:return "Level Time";
        case 5:return "Level Best";
        case 6:return "Level Deaths";
        default:return "Unknown Stat";
    }
}
// Use the actual HUD panel rectangles, not only a cell ID, to prevent
// closely-spaced positions from accidentally drawing over each other.
inline constexpr int kVrStatsItemCount=7;
inline constexpr bool vrStatsPositionsOverlap(int first,int firstPosition,
    int second,int secondPosition) noexcept {
    if(first<0||first>=kVrStatsItemCount
        ||second<0||second>=kVrStatsItemCount
        ||!statsPositionValid(firstPosition)
        ||!statsPositionValid(secondPosition))return false;
    const int firstWidth=campaignStatsPanelWidth(first);
    const int secondWidth=campaignStatsPanelWidth(second);
    const int x1=statsPanelX(firstPosition,960,firstWidth);
    const int y1=statsPanelY(firstPosition,720,60);
    const int x2=statsPanelX(secondPosition,960,secondWidth);
    const int y2=statsPanelY(secondPosition,720,60);
    return x1<x2+secondWidth&&x2<x1+firstWidth
        &&y1<y2+60&&y2<y1+60;
}
inline bool vrStatsSetEnabled(VrLiveSettings& s,int item,bool on) noexcept {
    if(item<0||item>=kVrStatsItemCount)return false;
    if(item==0)s.deathCounterEnabled=on;
    else if(item==1)s.deathStatisticsEnabled=on;
    else if(item==2)s.campaignTimerEnabled=on;
    else if(item==3)s.sessionTimerEnabled=on;
    else s.levelDisplayEnabled[item-4]=on;
    if(on){
        const int target=vrStatsCorner(s,item);
        for(int other=0;other<kVrStatsItemCount;++other)if(other!=item
            &&vrStatsEnabled(s,other)
            &&vrStatsPositionsOverlap(item,target,other,vrStatsCorner(s,other))){
            if(other==0)s.deathCounterEnabled=false;
            else if(other==1)s.deathStatisticsEnabled=false;
            else if(other==2)s.campaignTimerEnabled=false;
            else if(other==3)s.sessionTimerEnabled=false;
            else s.levelDisplayEnabled[other-4]=false;
        }
    }
    return true;
}
// Ordinary display changes never close the Mod Menu. Only a successful
// confirmed DEV Reset All does; failed writes and per-item resets stay open.
inline constexpr bool vrStatsMenuShouldClose(bool dev,int action,
    bool persistedSuccessfully) noexcept {
    return dev&&action==131&&persistedSuccessfully;
}
inline bool vrStatsSetCorner(VrLiveSettings& s,int item,int corner) noexcept {
    if(item<0||item>=kVrStatsItemCount||!statsPositionValid(corner))return false;
    if(item==0)s.deathCounterCorner=corner;
    else if(item==1)s.deathStatisticsCorner=corner;
    else if(item==2)s.campaignTimerCorner=corner;
    else if(item==3)s.sessionTimerCorner=corner;
    else s.levelDisplayPosition[item-4]=corner;
    // Assigning a position to a hidden widget must not evict anything.
    if(vrStatsEnabled(s,item))vrStatsSetEnabled(s,item,true);
    return true;
}
inline bool vrLiveEffectiveEnabled(const VrLiveSettings& s,
    VrLiveMod mod,bool desktopStartupEnabled) {
    if(mod==VrLiveMod::DeathCounter)return s.deathCounterEnabled;
    // Ignore legacy OFF entries for core features. Keep the old six-field
    // save format readable instead of silently rewriting UserData.
    if(vrLiveModAlwaysEnabled(mod))return true;
    if(!vrLiveModToggleable(mod))return desktopStartupEnabled;
    const int index=vrLiveToggleIndex(mod);
    return index<0 ? desktopStartupEnabled
       :s.enabledOverrides[static_cast<size_t>(index)]<0
        ?desktopStartupEnabled
        :s.enabledOverrides[static_cast<size_t>(index)]==1;
}
inline bool vrLiveSetEnabled(VrLiveSettings& s,VrLiveMod mod,bool on) {
    if(mod==VrLiveMod::DeathCounter)return vrStatsSetEnabled(s,0,on);
    if(!vrLiveModToggleable(mod))return false;
    const int index=vrLiveToggleIndex(mod);
    if(index<0)return false;
    s.enabledOverrides[static_cast<size_t>(index)]=on?1:0;
    return true;
}
struct VrPainsawModes {
    bool triggerThrottle{},legacyMotion{};
};
// OFF must return to native chainsaw timing: neither the new trigger-based
// PAINSAW nor the older motion-only modifier may remain active. Respect
// explicit live OFF even when the desktop setup has either/both enabled.
inline VrPainsawModes vrLivePainsawModes(
    const VrLiveSettings& settings,bool desktopTrigger,bool desktopMotion) {
    const bool enabled=vrLiveEffectiveEnabled(settings,VrLiveMod::Painsaw,
        desktopTrigger||desktopMotion);
    return {enabled&&(desktopTrigger||!desktopMotion),
            enabled&&desktopMotion};
}
// Restore ONLY Gunslinger hip/wheel placement, not KILLTIME, crouch,
// enabled overrides, weapon grip positions or any other saved settings.
inline void vrLiveResetGunslingerPositions(VrLiveSettings& s) noexcept {
    s.hipShift=kVrLiveFactoryHipShifts;
    s.wheelShift={};
}
inline bool vrLiveValidSettings(const VrLiveSettings& s) {
    for(int value:s.enabledOverrides)if(value < -1||value > 1)return false;
    if(!statsPositionValid(s.deathCounterCorner))return false;
    if(!statsPositionValid(s.deathStatisticsCorner))return false;
    if(!statsPositionValid(s.campaignTimerCorner))return false;
    if(!statsPositionValid(s.sessionTimerCorner))return false;
    for(const int position:s.levelDisplayPosition)
        if(!statsPositionValid(position))return false;
    for(int i=0;i<kVrStatsItemCount;++i)
        for(int j=i+1;j<kVrStatsItemCount;++j)
        if(vrStatsEnabled(s,i)&&vrStatsEnabled(s,j)
            &&vrStatsPositionsOverlap(i,vrStatsCorner(s,i),
                j,vrStatsCorner(s,j)))return false;
    for(const auto& p:s.hipShift)if(!vrLiveValidShift(p))return false;
    for(const auto& p:s.wheelShift)if(!vrLiveValidShift(p))return false;
    return std::isfinite(s.crouchStandDelta)
        &&std::isfinite(s.crouchLoweredDelta)
        &&s.crouchStandDelta>=-1.5f&&s.crouchStandDelta<=1.5f
        &&s.crouchLoweredDelta>=-1.5f&&s.crouchLoweredDelta<=1.5f
        &&(!s.crouchStandSaved||!s.crouchLoweredSaved
            ||s.crouchStandDelta-s.crouchLoweredDelta>=.18f)
        &&(s.gloryKillHz==0||(s.gloryKillHz>=30&&s.gloryKillHz<=240))
        &&std::isfinite(s.punchSpeedMetersPerSec)
        &&s.punchSpeedMetersPerSec>=.5f&&s.punchSpeedMetersPerSec<=5.f
        &&s.punchHands>=0&&s.punchHands<=2
        &&std::isfinite(s.smoothTurnSpeedDegreesPerSec)
        &&(s.smoothTurnSpeedDegreesPerSec==0.f
            ||(s.smoothTurnSpeedDegreesPerSec>=150.f&&s.smoothTurnSpeedDegreesPerSec<=400.f));
}
inline VrLivePoint vrLiveShiftedPoint(VrLivePoint p,VrLivePoint shift) {
    return {p.x+shift.x,p.y+shift.y,p.z+shift.z};
}
inline bool vrLiveInsideBounds(VrLivePoint p,VrLivePoint lo,VrLivePoint hi,
                               VrLivePoint shift) {
    return p.x>=lo.x+shift.x&&p.x<=hi.x+shift.x
        &&p.y>=lo.y+shift.y&&p.y<=hi.y+shift.y
        &&p.z>=lo.z+shift.z&&p.z<=hi.z+shift.z;
}
inline bool vrLiveHipInside(VrLivePoint p,bool expanded,VrLivePoint shift){
    return vrLiveInsideBounds(p,
        expanded?VrLivePoint{.07f,-1.14f,-.05f}:VrLivePoint{.12f,-1.04f,.02f},
        expanded?VrLivePoint{.60f,-.38f,.53f}:VrLivePoint{.51f,-.48f,.43f},
        shift);
}
inline bool vrLiveWheelInside(VrLivePoint p,bool expanded,VrLivePoint shift){
    return vrLiveInsideBounds(p,
        expanded?VrLivePoint{-.16f,-.35f,-.13f}:VrLivePoint{-.04f,-.18f,.015f},
        expanded?VrLivePoint{.66f,.54f,.86f}:VrLivePoint{.44f,.36f,.60f},
        shift);
}
// Gunslinger calibration previews use the same head-relative zone coordinates
// as live detection. During a Grip drag only the temporary draft changes.
inline constexpr bool vrLiveIsGunslingerCapture(VrLiveCapture target) noexcept {
    return target==VrLiveCapture::HipRight||target==VrLiveCapture::HipLeft
        ||target==VrLiveCapture::WheelRight||target==VrLiveCapture::WheelLeft;
}
inline VrLivePoint vrLiveZoneCenter(const VrLiveSettings& s,VrLiveCapture target) noexcept {
    if(!vrLiveIsGunslingerCapture(target))return {};
    const bool hip=target==VrLiveCapture::HipRight||target==VrLiveCapture::HipLeft;
    const int side=(target==VrLiveCapture::HipLeft||target==VrLiveCapture::WheelLeft)?1:0;
    return vrLiveShiftedPoint(hip?vrLiveHipDefaultCenter():vrLiveWheelDefaultCenter(),
        hip?s.hipShift[side]:s.wheelShift[side]);
}
inline bool vrLiveUpdateZoneDraft(VrLivePoint& draft,bool gripHeld,bool tracked,
                                  VrLivePoint controllerRelative,VrLiveCapture target) noexcept {
    if(!vrLiveIsGunslingerCapture(target)||!gripHeld||!tracked
        ||!vrLiveValidPoint(controllerRelative))return false;
    const bool hip=target==VrLiveCapture::HipRight||target==VrLiveCapture::HipLeft;
    const auto base=hip?vrLiveHipDefaultCenter():vrLiveWheelDefaultCenter();
    if(!vrLiveValidShift({controllerRelative.x-base.x,
                          controllerRelative.y-base.y,
                          controllerRelative.z-base.z}))return false;
    draft=controllerRelative;
    return true;
}
inline bool vrLiveCapturePoint(VrLiveSettings& s,VrLiveCapture target,
                               VrLivePoint handRelative,float headDelta){
    if(target==VrLiveCapture::CrouchStanding){
        if(!std::isfinite(headDelta)||headDelta < -1.5f ||headDelta>1.5f
           ||(s.crouchLoweredSaved
                &&headDelta-s.crouchLoweredDelta<.18f))return false;
        s.crouchStandDelta=headDelta;s.crouchStandSaved=true;return true;
    }
    if(target==VrLiveCapture::CrouchLowered){
        if(!std::isfinite(headDelta)||headDelta < -1.5f ||headDelta>1.5f
           ||headDelta>s.crouchStandDelta-.18f)return false;
        s.crouchLoweredDelta=headDelta;s.crouchLoweredSaved=true;return true;
    }
    if(!vrLiveValidPoint(handRelative))return false;
    const bool hip=target==VrLiveCapture::HipRight||target==VrLiveCapture::HipLeft;
    const bool wheel=target==VrLiveCapture::WheelRight||target==VrLiveCapture::WheelLeft;
    if(!hip&&!wheel)return false;
    const int side=(target==VrLiveCapture::HipLeft
        ||target==VrLiveCapture::WheelLeft)?1:0;
    const auto center=hip?vrLiveHipDefaultCenter():vrLiveWheelDefaultCenter();
    const VrLivePoint shift{handRelative.x-center.x,
        handRelative.y-center.y,handRelative.z-center.z};
    if(!vrLiveValidShift(shift))return false;
    (hip?s.hipShift:s.wheelShift)[side]=shift;
    return true;
}
inline float vrLiveCrouchEntry(const VrLiveSettings& s){
    if(!s.crouchLoweredSaved)return -.28f; // preserve native default
    return .5f*(s.crouchStandDelta+s.crouchLoweredDelta);
}
inline float vrLiveCrouchExit(const VrLiveSettings& s){
    if(!s.crouchLoweredSaved)return -.20f;
    return vrLiveCrouchEntry(s)+.08f;
}
inline bool vrLiveLoad(const std::string& path,VrLiveSettings& out){
    namespace fs=std::filesystem;
    std::error_code ec;
    if(!fs::exists(path,ec)||ec)return !ec;
    if(fs::file_size(path,ec)>8192u||ec)return false;
    std::ifstream f(path);
    if(!f)return false;
    VrLiveSettings next{};
    std::string name,line;
    int schema=0;
    while(std::getline(f,line)){
        if(line.empty()||line[0]=='#')continue;
        std::istringstream row(line);
        if(!(row>>name))continue;
        if(name=="schema"){if(!(row>>schema)||schema!=1)return false;}
        else if(name=="hip_right"||name=="hip_left"||name=="wheel_right"||name=="wheel_left"){
            VrLivePoint p{};std::string trailing;
            if(!(row>>p.x>>p.y>>p.z)||(row>>trailing))return false;
            const int side=name.find("left")!=std::string::npos?1:0;
            (name.find("hip_")==0?next.hipShift:next.wheelShift)[side]=p;
        }else if(name=="standing"||name=="lowered"){
            float value{};int valid{};std::string trailing;
            if(!(row>>valid>>value)||(row>>trailing)||(valid!=0&&valid!=1))return false;
            (name=="standing"?next.crouchStandSaved:next.crouchLoweredSaved)=valid!=0;
            (name=="standing"?next.crouchStandDelta:next.crouchLoweredDelta)=value;
        }else if(name=="enabled"){
            // Backward compatible: existing UserData files have 5 toggles;
            // PAINSAW is appended as a sixth and defaults to desktop state.
            int value{};size_t count=0;
            while(row>>value){
                if(count>=next.enabledOverrides.size())return false;
                next.enabledOverrides[count++]=value;
            }
            if(!row.eof()||(count!=5&&count!=6))return false;
            if(count==5)next.enabledOverrides[5]=-1; // preserve legacy desktop PAINSAW selection
        }else if(name=="gauss_two_hand_fire"){
            int required{};std::string extra;
            if(!(row>>required)||(required!=0&&required!=1)||(row>>extra))return false;
            next.gaussRequireTwoHandFire=required!=0;
        }else if(name=="killtime_hz"){if(!(row>>next.gloryKillHz))return false;}
        else if(name=="punch_speed"){if(!(row>>next.punchSpeedMetersPerSec))return false;}
        else if(name=="punch_hands"){if(!(row>>next.punchHands))return false;}
        else if(name=="smooth_turn_speed"){if(!(row>>next.smoothTurnSpeedDegreesPerSec))return false;}
        else if(name=="death_counter_enabled"){
            int enabled{};std::string extra;
            if(!(row>>enabled)||(enabled!=0&&enabled!=1)||(row>>extra))return false;
            next.deathCounterEnabled=enabled!=0;
        }else if(name=="death_counter_corner"){
            std::string extra;
            if(!(row>>next.deathCounterCorner)||(row>>extra))return false;
        }else if(name=="death_statistics_enabled"||name=="campaign_timer_enabled"
            ||name=="session_timer_enabled"){
            int enabled{};std::string extra;
            if(!(row>>enabled)||(enabled!=0&&enabled!=1)||(row>>extra))return false;
            (name=="death_statistics_enabled"?next.deathStatisticsEnabled
                :name=="campaign_timer_enabled"?next.campaignTimerEnabled
                :next.sessionTimerEnabled)=enabled!=0;
        }else if(name=="death_statistics_corner"||name=="campaign_timer_corner"
            ||name=="session_timer_corner"){
            int corner{};std::string extra;
            if(!(row>>corner)||(row>>extra))return false;
            (name=="death_statistics_corner"?next.deathStatisticsCorner
                :name=="campaign_timer_corner"?next.campaignTimerCorner
                :next.sessionTimerCorner)=corner;
        }else if(name=="level_time_enabled"||name=="level_best_enabled"
            ||name=="level_deaths_enabled"){
            int value{};std::string extra;
            if(!(row>>value)||(value!=0&&value!=1)||(row>>extra))return false;
            next.levelDisplayEnabled[name=="level_time_enabled"?0:
                name=="level_best_enabled"?1:2]=value!=0;
        }else if(name=="level_time_position"||name=="level_best_position"
            ||name=="level_deaths_position"){
            int value{};std::string extra;
            if(!(row>>value)||(row>>extra))return false;
            next.levelDisplayPosition[name=="level_time_position"?0:
                name=="level_best_position"?1:2]=value;
        }else return false;
    }
    if(f.bad()||schema!=1)return false;
    // Preserve both counters when they occupy separate areas. Resolve any
    // overlapping settings (including legacy saved corners) deterministically.
    if(next.deathCounterEnabled)vrStatsSetEnabled(next,0,true);
    if(next.deathStatisticsEnabled)vrStatsSetEnabled(next,1,true);
    if(next.campaignTimerEnabled)vrStatsSetEnabled(next,2,true);
    if(next.sessionTimerEnabled)vrStatsSetEnabled(next,3,true);
    for(int i=4;i<kVrStatsItemCount;++i)
        if(next.levelDisplayEnabled[i-4])vrStatsSetEnabled(next,i,true);
    if(!vrLiveValidSettings(next))return false;
    out=next;
    return true;
}
inline bool vrLiveSave(const std::string& path,const VrLiveSettings& s){
    if(!vrLiveValidSettings(s))return false;
    namespace fs=std::filesystem;
    const fs::path dest(path),tmp(path+".tmp");
    std::error_code ec;
    if(!dest.parent_path().empty()){
        fs::create_directories(dest.parent_path(),ec);
        if(ec)return false;
    }
    std::ofstream out(tmp,std::ios::trunc);
    if(!out)return false;
    out<<std::fixed<<std::setprecision(7)<<"schema 1\n";
    out<<"enabled";
    for(int value:s.enabledOverrides)out<<' '<<value;
    out<<'\n';
    auto point=[&](const char* name,VrLivePoint p){
        out<<name<<' '<<p.x<<' '<<p.y<<' '<<p.z<<'\n';
    };
    point("hip_right",s.hipShift[0]);point("hip_left",s.hipShift[1]);
    point("wheel_right",s.wheelShift[0]);point("wheel_left",s.wheelShift[1]);
    out<<"gauss_two_hand_fire "<<(s.gaussRequireTwoHandFire?1:0)<<'\n';
    out<<"standing "<<s.crouchStandSaved<<' '<<s.crouchStandDelta<<'\n'
       <<"lowered "<<s.crouchLoweredSaved<<' '<<s.crouchLoweredDelta<<'\n'
       <<"killtime_hz "<<s.gloryKillHz<<'\n'
       <<"punch_speed "<<s.punchSpeedMetersPerSec<<'\n'
       <<"punch_hands "<<s.punchHands<<'\n'
       <<"smooth_turn_speed "<<s.smoothTurnSpeedDegreesPerSec<<'\n'
       <<"death_counter_enabled "<<(s.deathCounterEnabled?1:0)<<'\n'
       <<"death_counter_corner "<<s.deathCounterCorner<<'\n'
       <<"death_statistics_enabled "<<(s.deathStatisticsEnabled?1:0)<<'\n'
       <<"death_statistics_corner "<<s.deathStatisticsCorner<<'\n'
       <<"campaign_timer_enabled "<<(s.campaignTimerEnabled?1:0)<<'\n'
       <<"campaign_timer_corner "<<s.campaignTimerCorner<<'\n'
       <<"session_timer_enabled "<<(s.sessionTimerEnabled?1:0)<<'\n'
       <<"session_timer_corner "<<s.sessionTimerCorner<<'\n'
       <<"level_time_enabled "<<(s.levelDisplayEnabled[0]?1:0)<<'\n'
       <<"level_time_position "<<s.levelDisplayPosition[0]<<'\n'
       <<"level_best_enabled "<<(s.levelDisplayEnabled[1]?1:0)<<'\n'
       <<"level_best_position "<<s.levelDisplayPosition[1]<<'\n'
       <<"level_deaths_enabled "<<(s.levelDisplayEnabled[2]?1:0)<<'\n'
       <<"level_deaths_position "<<s.levelDisplayPosition[2]<<'\n';
    out.flush();
    if(!out){out.close();fs::remove(tmp,ec);return false;}
    out.close();
#ifdef _WIN32
    if(!MoveFileExW(tmp.wstring().c_str(),dest.wstring().c_str(),
        MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){
        fs::remove(tmp,ec);return false;
    }
#else
    fs::rename(tmp,dest,ec);
    if(ec){fs::remove(tmp);return false;}
#endif
    return true;
}
} // namespace kharvox
