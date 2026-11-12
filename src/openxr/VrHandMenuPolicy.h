#pragma once
#include "RightHandGripConfig.h"
#include "VrLiveModsConfig.h"
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
// Only LIVE weapon-pose calibration forwards the physically held PRIMARY
// weapon Grip to DOOM's native alternate-fire input (XInput left trigger).
// Never allow it from the visible settings menu, the wrong equipped weapon,
// a changed weapon hand, or outside live gameplay.
inline constexpr bool vrCalibrationNativeAltFireAllowed(
    VrHandMenuMode mode,bool gameplay,bool calibrationEnabled,
    bool selectedWeaponEquipped,bool weaponHandUnchanged,
    bool primaryWeaponGripDown) noexcept {
    return mode==VrHandMenuMode::Calibrating && gameplay
        && calibrationEnabled && selectedWeaponEquipped
        && weaponHandUnchanged && primaryWeaponGripDown;
}
// Single physical RIGHT thumbstick hold >= 1000ms opens/closes the menu.
// A short click is untouched, so Hand Focus retains its original input.
struct VrMenuHoldShortcut {
    bool held{},triggered{};
    std::uint64_t beganAt{};
};
enum class VrMenuShortcutAction {None,Toggle,ConsumedRelease};
inline VrMenuShortcutAction updateVrMenuHoldShortcut(
    VrMenuHoldShortcut& state,bool rightClick,std::uint64_t nowMs) noexcept {
    if(!rightClick){
        const bool consumed=state.triggered;
        state.held=false;state.triggered=false;state.beganAt=0;
        return consumed?VrMenuShortcutAction::ConsumedRelease
            :VrMenuShortcutAction::None;
    }
    if(!state.held){
        state.held=true;state.triggered=false;state.beganAt=nowMs;
    }
    if(!state.triggered&&nowMs>=state.beganAt
        &&nowMs-state.beganAt>=1000ULL){
        state.triggered=true;
        return VrMenuShortcutAction::Toggle;
    }
    return VrMenuShortcutAction::None;
}
enum class VrMenuPage {Mods,WeaponGrip};
struct VrHandMenuState {
    VrHandMenuMode mode{VrHandMenuMode::Closed};
    VrMenuPage page{VrMenuPage::Mods};
    VrLiveMod selectedMod{VrLiveMod::WeaponGrip};
    int modScroll{};
    int selectedRow{};
    VrLiveCapture liveCapture{VrLiveCapture::None};
    bool autoSelectingWeapon{};
    unsigned long long autoSelectionDeadlineMs{};
    int pendingNativeSelection{}; // 1=BFG (Y), 2=Chainsaw (X)
    long long nativeSelectionPulseUntil{};
    int selectedWeapon{1};
    bool rightHand{true};
    // Gauss Siege and Chaingun Turret have separate visual hand profiles.
    bool alternateProfile{};
    // Latch holster ownership at capture start; switching hands while
    // calibrating never writes a pose to the wrong physical hand.
    bool calibratingWeaponLeft{};
    VrMenuHoldShortcut shortcut{};
    bool rightTriggerPreviously{},leftTriggerPreviously{},leftStickCentered{true};
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
inline bool hasSavedVrPrimaryPose(const RightHandGripSettings& settings,
    const std::string&weapon,bool leftHand) {
    const auto& positions=leftHand?settings.leftVisualPositions:settings.visualPositions;
    const auto& rotations=leftHand?settings.leftVisualRotations:settings.visualRotations;
    return positions.find(weapon)!=positions.end()
        &&rotations.find(weapon)!=rotations.end();
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
                matches=key==weapon+"_hand_position"||key==weapon+"_rotation"
                    ||key==weapon+"_left_hand_position"||key==weapon+"_left_rotation"
                    ||(!alternate.empty()&&(
                        key==alternate+"_hand_position"||key==alternate+"_rotation"
                        ||key==alternate+"_left_hand_position"
                        ||key==alternate+"_left_rotation"
                        ||key==alternate+"_support_position"
                        ||key==alternate+"_left_primary_support_position"));
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
// Restore only one selected weapon from the user's PROVIDED factory preset.
// Unlike the legacy reset (which removes overrides), this reproduces the
// exact calibrated left/right primary and support values, including Siege /
// Turret, without touching other weapons or shared Gauss/Chaingun anchors.
// Validate the factory and stage everything BEFORE modifying any live file.
inline bool restoreVrWeaponCalibrationDefault(const std::string& path,
    const std::string& factoryPath,const std::string& weapon,
    bool primaryFile,std::string*why=nullptr) {
    namespace fs=std::filesystem;
    auto fail=[&](const char* message){
        if(why)*why=message;
        return false;
    };
    if(!validVrCalibrationWeapon(weapon))
        return fail("unsupported reset weapon");
    const std::string alternate=weapon=="gauss_cannon"?"gauss_cannon_siege"
        :weapon=="chaingun"?"chaingun_turret":"";
    const auto belongs=[&](const std::string& line)->bool {
        if(primaryFile){
            const auto first=line.find_first_not_of(" \t");
            const auto equal=line.find('=');
            if(first==std::string::npos||equal==std::string::npos
               ||line[first]=='#'||first>=equal)return false;
            auto key=line.substr(first,equal-first);
            const auto end=key.find_last_not_of(" \t");
            if(end!=std::string::npos)key.resize(end+1);
            const auto matches=[&](const std::string& name){
                return key==name+"_hand_position"||key==name+"_rotation"
                    ||key==name+"_left_hand_position"
                    ||key==name+"_left_rotation";
            };
            if(matches(weapon))return true;
            return !alternate.empty()
                &&(matches(alternate)||key==alternate+"_support_position"
                   ||key==alternate+"_left_primary_support_position");
        }
        std::istringstream row(line);
        std::string version,key;
        return (row>>version>>key)&&version=="2"&&key==weapon;
    };
    std::error_code ec;
    if(!fs::exists(factoryPath,ec)||ec)return fail("packaged factory calibration missing");
    if(fs::file_size(factoryPath,ec)>(primaryFile?16384u:65536u)||ec)
        return fail("factory calibration exceeds size limit");
    std::ifstream factory(factoryPath);
    if(!factory)return fail("cannot read factory calibration");
    std::vector<std::string> restore;
    std::string line;
    while(std::getline(factory,line))
        if(belongs(line))restore.push_back(line);
    if(factory.bad()||restore.empty())
        return fail("factory has no saved pose for this weapon");
    const bool exists=fs::exists(path,ec);
    if(ec)return fail("cannot access existing calibration");
    std::vector<std::string> outputLines;
    if(exists){
        if(fs::file_size(path,ec)>(primaryFile?16384u:65536u)||ec)
            return fail("existing calibration exceeds size limit");
        std::ifstream input(path);
        if(!input)return fail("cannot read existing calibration");
        while(std::getline(input,line))
            if(!belongs(line))outputLines.push_back(line);
        if(input.bad())return fail("failed reading existing calibration");
    }
    outputLines.insert(outputLines.end(),restore.begin(),restore.end());
    const fs::path dest(path),staging(path+".vr-default.tmp");
    // Backup is always left adjacent in portable UserData for recovery.
    if(exists){
        fs::copy_file(dest,path+".before-vr-reset.bak",
            fs::copy_options::overwrite_existing,ec);
        if(ec)return fail("cannot back up current weapon calibration");
    }
    {std::ofstream output(staging,std::ios::trunc);
     if(!output)return fail("cannot create reset staging file");
     for(const auto& record:outputLines)output<<record<<'\n';
     output.flush();
     if(!output){fs::remove(staging,ec);return fail("cannot save reset staging file");}}
#ifdef _WIN32
    if(!MoveFileExW(staging.wstring().c_str(),dest.wstring().c_str(),
        MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){
        fs::remove(staging,ec);
        return fail("cannot install factory calibration");
    }
#else
    fs::rename(staging,dest,ec);
    if(ec){fs::remove(staging);return fail("cannot install factory calibration");}
#endif
    return true;
}

}
