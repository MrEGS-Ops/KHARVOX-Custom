#include "../src/common/RuntimePaths.h"

#include <string>

int main() {
    const std::wstring directory = kharvox::runtimeDirectory();
    if (directory.empty()) return 1;

    const std::wstring file = kharvox::runtimePath(L"marker.test");
    if (file.size() <= directory.size()) return 2;
    if (file.compare(0, directory.size(), directory) != 0) return 3;
    if (file.substr(file.size() - 11) != L"marker.test") return 4;

    const std::string narrow = kharvox::runtimePathA("marker.test");
    if (narrow.empty() || narrow.find("marker.test") == std::string::npos) return 5;

    wchar_t temp[MAX_PATH]{};
    if(!GetTempPathW(MAX_PATH,temp))return 6;
    if(kharvox::logPath(L"native_stereo.log")!=std::wstring(temp)+L"KHARVOX-NATIVE-STEREO.log")return 7;
    if(kharvox::logPath(L"vr_intro.log")!=std::wstring(temp)+L"KHARVOX-VR-INTRO.log")return 8;
    if(kharvox::logPath(L"native_capture.json")!=std::wstring(temp)+L"KHARVOX-Diagnostics\\native_capture.json")return 9;
    if(kharvox::logPathA("native_stereo.log").find("KHARVOX-NATIVE-STEREO.log")==std::string::npos)return 10;
    // Categorized personal files must stay separate from live launch markers,
    // immutable defaults and unrelated legacy configurations.
    struct CategoryTest { const wchar_t* file; const wchar_t* folder; };
    constexpr CategoryTest grouped[]{
        {L"vr_live_mods_saved.cfg",L"Settings"},
        {L"object_discovery.cfg",L"ObjectGrip"},
        {L"object_grip_profiles.cfg",L"ObjectGrip"},
        {L"offhand_right_hand_positions.cfg",L"Calibrations"},
        {L"hud_role_calibration_saved.cfg",L"Calibrations"},
        {L"weapon_recoil_saved.cfg",L"Calibrations"},
        {L"campaign_death_count.cfg",L"Statistics"},
        {L"campaign_run_timer.cfg",L"Statistics"},
        {L"level_best_records.cfg",L"Records"}
    };
    for(const auto& entry:grouped){
        const auto category=kharvox::userConfigFolder(entry.file);
        if(!category||_wcsicmp(category,entry.folder)!=0)return 11;
        if(!kharvox::isUserConfigName(entry.file))return 12;
    }
    // Object Grip development files use their folder even when old flat
    // disposable test data remains in UserData (no migration or fallback).
    for(const auto* name:{
        L"weapon_scale.cfg",L"render_scale.cfg",L"back_weapon.cfg",
        L"two_hand_alignment.cfg",L"hand_models.cfg",
        L"hand_models_calibration_default.cfg",L"unknown.cfg",
        L"CalibrationDefaults/offhand_right_hand_positions.cfg",
        L"../level_best_records.cfg"
    }){
        if(kharvox::userConfigFolder(name)!=nullptr)return 13;
    }
    if(kharvox::isUserConfigName(L"hand_models_calibration_default.cfg")
        ||kharvox::isUserConfigName(L"CalibrationDefaults/default.cfg"))
        return 14;
    return 0;
}
