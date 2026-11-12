#pragma once
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
enum class VrLiveMod : int {
    WeaponGrip, Gunslinger, PhysicalCrouch, Killtime,
    PhysicalGloryKills, HandFocus, Painsaw, WheelRemap,
    HandsJump, BoxingGuard, Other
};
inline constexpr bool vrLiveModEditable(VrLiveMod id) noexcept {
    return id==VrLiveMod::WeaponGrip||id==VrLiveMod::Gunslinger
        ||id==VrLiveMod::PhysicalCrouch||id==VrLiveMod::Killtime
        ||id==VrLiveMod::PhysicalGloryKills;
}
inline const char* vrLiveModLabel(VrLiveMod id) noexcept {
    switch(id){
        case VrLiveMod::WeaponGrip:return "Weapon Grip Calibration";
        case VrLiveMod::Gunslinger:return "Gunslinger";
        case VrLiveMod::PhysicalCrouch:return "Physical Crouch";
        case VrLiveMod::Killtime:return "KILLTIME: RIP & TEAR";
        case VrLiveMod::PhysicalGloryKills:return "Physical Glory Kills";
        case VrLiveMod::HandFocus:return "Hand Focus";
        case VrLiveMod::Painsaw:return "PAINSAW: RIPSHIFT";
        case VrLiveMod::WheelRemap:return "Weapon Wheel Remap";
        case VrLiveMod::HandsJump:return "Hands Jump";
        case VrLiveMod::BoxingGuard:return "Boxing Guard";
        default:return "Other active mod";
    }
}
enum class VrLiveCapture : int {
    None,HipRight,HipLeft,WheelRight,WheelLeft,CrouchStanding,CrouchLowered
};
struct VrLiveSettings {
    // Offset from the EXISTING tracking-relative zones. Zero exactly
    // reproduces existing Gunslinger behaviour on a fresh installation.
    std::array<VrLivePoint,2> hipShift{};
    std::array<VrLivePoint,2> wheelShift{};
    float crouchStandDelta{0.f};
    float crouchLoweredDelta{-0.56f};
    bool crouchStandSaved{},crouchLoweredSaved{};
    int gloryKillHz{}; // 0 = existing launcher preset and action-profile Hz
    float punchSpeedMetersPerSec{2.8f};
    int punchHands{2}; // 0 right, 1 left, 2 both
};
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
inline bool vrLiveValidSettings(const VrLiveSettings& s) {
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
        &&s.punchHands>=0&&s.punchHands<=2;
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
        }else if(name=="killtime_hz"){if(!(row>>next.gloryKillHz))return false;}
        else if(name=="punch_speed"){if(!(row>>next.punchSpeedMetersPerSec))return false;}
        else if(name=="punch_hands"){if(!(row>>next.punchHands))return false;}
        else return false;
    }
    if(f.bad()||schema!=1||!vrLiveValidSettings(next))return false;
    out=next;
    return true;
}
inline bool vrLiveSave(const std::string& path,const VrLiveSettings& s){
    if(!vrLiveValidSettings(s))return false;
    namespace fs=std::filesystem;
    const fs::path dest(path),tmp(path+".tmp");
    std::error_code ec;
    fs::create_directories(dest.parent_path(),ec);
    if(ec)return false;
    std::ofstream out(tmp,std::ios::trunc);
    if(!out)return false;
    out<<std::fixed<<std::setprecision(7)<<"schema 1\n";
    auto point=[&](const char* name,VrLivePoint p){
        out<<name<<' '<<p.x<<' '<<p.y<<' '<<p.z<<'\n';
    };
    point("hip_right",s.hipShift[0]);point("hip_left",s.hipShift[1]);
    point("wheel_right",s.wheelShift[0]);point("wheel_left",s.wheelShift[1]);
    out<<"standing "<<s.crouchStandSaved<<' '<<s.crouchStandDelta<<'\n'
       <<"lowered "<<s.crouchLoweredSaved<<' '<<s.crouchLoweredDelta<<'\n'
       <<"killtime_hz "<<s.gloryKillHz<<'\n'
       <<"punch_speed "<<s.punchSpeedMetersPerSec<<'\n'
       <<"punch_hands "<<s.punchHands<<'\n';
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
