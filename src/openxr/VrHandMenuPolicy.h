#pragma once
#include "RightHandGripConfig.h"
#include <array>
#include <string>
#include <vector>
#include <sstream>
#include <fstream>
#include <filesystem>

// In-headset weapon-hand calibration. Left support hand saves TRANSLATION
// only. Its orientation is intentionally established anew on every Grip.
namespace kharvox {
inline constexpr std::array<const char*,10> kVrCalibrationWeapons{
    "pistol","shotgun","heavy_assault_rifle","plasma_rifle",
    "rocket_launcher","super_shotgun","gauss_cannon","chaingun",
    "bfg","chainsaw"
};
enum class VrHandMenuMode { Closed, Menu, Calibrating };
struct VrHandMenuState {
    VrHandMenuMode mode{VrHandMenuMode::Closed};
    int selectedWeapon{1};
    bool rightHand{true};
    bool bothClickedPreviously{},rightTriggerPreviously{},leftStickCentered{true};
    int hovered{-1};
    float cursorX{-1.f},cursorY{-1.f};
    bool previewValid{};
    RightHandGripOffset previewPosition{};
    RightHandGripRotation previewRotation{};
    std::array<bool,10> rightSaved{},leftSaved{};
    std::string message;
    const char* weapon() const noexcept {return kVrCalibrationWeapons[static_cast<size_t>(selectedWeapon)];}
};
inline bool isVrMenuInputOwner(const VrHandMenuState& m) noexcept {
    return m.mode!=VrHandMenuMode::Closed;
}
inline bool vrHandMenuBothStickEdge(VrHandMenuState& m,bool left,bool right) noexcept {
    const bool both=left&&right;
    const bool edge=both&&!m.bothClickedPreviously;
    m.bothClickedPreviously=both;
    return edge;
}
inline bool hasSavedVrSupport(const std::string&path,const std::string&weapon){
    std::ifstream input(path);std::string row;
    while(std::getline(input,row)){
        std::istringstream record(row);std::string version,key;
        if(record>>version>>key&&version=="2"&&key==weapon)return true;
    }
    return false;
}
// Write one per-weapon reset, retaining every unrelated line and an adjacent
// copy for recovery. The legacy heavy weapon GUN anchor is not a visual hand
// calibration and must never be deleted or rewritten by this operation.
inline bool resetVrWeaponCalibrationFile(const std::string&path,
    const std::string&weapon,bool rightHandFile,std::string*why=nullptr){
    if(!validVrCalibrationWeapon(weapon)){
        if(why)*why="unsupported weapon";return false;
    }
    namespace fs=std::filesystem;
    std::error_code ec;
    if(!fs::exists(path,ec))return true;
    if(ec||fs::file_size(path,ec)>(rightHandFile?16384u:65536u)){
        if(why)*why="calibration file too large or inaccessible";return false;
    }
    std::ifstream input(path);
    if(!input){if(why)*why="cannot open calibration file";return false;}
    std::vector<std::string> remaining;
    std::string line;bool removed=false;
    const std::string alternate=weapon=="gauss_cannon"?weapon+"_siege"
        :weapon=="chaingun"?weapon+"_turret":"";
    while(std::getline(input,line)){
        bool matches=false;
        if(rightHandFile){
            const auto eq=line.find('=');
            const auto first=line.find_first_not_of(" \t");
            if(eq!=std::string::npos&&first!=std::string::npos&&line[first]!='#'){
                auto key=line.substr(first,eq-first);
                const auto end=key.find_last_not_of(" \t");
                if(end!=std::string::npos)key.erase(end+1);
                matches=key==weapon+"_hand_position"||key==weapon+"_rotation"||
                    (!alternate.empty()&&(key==alternate+"_hand_position"||
                      key==alternate+"_rotation"||key==alternate+"_support_position"));
            }
        }else{
            std::istringstream record(line);std::string version,key;
            if((record>>version>>key)&&version=="2"&&key==weapon)matches=true;
        }
        if(matches)removed=true;
        else remaining.push_back(line);
    }
    if(input.bad()){if(why)*why="failed reading calibration";return false;}
    if(!removed)return true;
    fs::copy_file(path,path+".before-vr-reset.bak",
        fs::copy_options::overwrite_existing,ec);
    if(ec){if(why)*why="could not back up existing calibration";return false;}
    const fs::path tmp(path+".vr-reset.tmp"),dest(path);
    {std::ofstream output(tmp,std::ios::trunc);
     if(!output){if(why)*why="failed opening reset staging file";return false;}
     for(const auto&row:remaining)output<<row<<'\n';
     output.flush();if(!output){if(why)*why="failed saving reset staging file";return false;}}
#ifdef _WIN32
    if(!MoveFileExW(tmp.wstring().c_str(),dest.wstring().c_str(),
        MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){
        fs::remove(tmp,ec);if(why)*why="could not install reset config";return false;}
#else
    fs::rename(tmp,dest,ec);
    if(ec){fs::remove(tmp);if(why)*why="could not install reset config";return false;}
#endif
    return true;
}
}
