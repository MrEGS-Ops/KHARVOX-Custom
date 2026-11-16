#pragma once
#include "CampaignStatsPolicy.h"
#include <array>
#include <algorithm>
#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <sstream>
#include <fstream>
#include <filesystem>

// Level timing never uses KharvoxCameraLevelTransitionGeneration as an
// identity: that counter ALSO advances on checkpoint reloads. A verified,
// stable native map key is required before a personal best can be saved.
namespace kharvox {
// Explicit campaign mission selection is a safe fallback until a verified
// DOOM v1.1.1 native map identity becomes available. Never derive a mission
// from levelTransitionGeneration: checkpoint reloads advance that too.
struct CampaignMission {const char* key;const char* title;};
inline constexpr std::array<CampaignMission,13> kCampaignMissions{{
    {"campaign/01","01  The UAC"},
    {"campaign/02","02  Resource Operations"},
    {"campaign/03","03  Foundry"},
    {"campaign/04","04  Argent Facility"},
    {"campaign/05","05  Argent Energy Tower"},
    {"campaign/06","06  Kadingir Sanctum"},
    {"campaign/07","07  Argent Facility (Destroyed)"},
    {"campaign/08","08  Advanced Research Complex"},
    {"campaign/09","09  Lazarus Labs"},
    {"campaign/10","10  Titan's Realm"},
    {"campaign/11","11  The Necropolis"},
    {"campaign/12","12  VEGA Central Processing"},
    {"campaign/13","13  Argent D'Nur"},
}};
inline constexpr std::uint64_t kLevelBestGoldMs=5000;
inline constexpr std::size_t kLevelRecordsMax=64;
inline constexpr std::uint64_t kLevelRecordMaxMs=100000000000ULL;
inline bool levelRecordKeyValid(std::string_view key) noexcept {
    if(key.empty()||key.size()>95)return false;
    for(unsigned char c:key)
        if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')
            ||(c>='0'&&c<='9')||c=='_'||c=='-'||c=='/'
            ||c=='.'))return false;
    return key.find("..")==std::string_view::npos;
}
struct LevelRecords {
    std::map<std::string,std::uint64_t> bestMs;
};
struct LevelProgress {
    CampaignRunTime time{};
    std::uint64_t deaths{};
    std::string verifiedMapKey;
    bool completed{};
    bool awaitingNextLevel{};
    bool bestUpdated{};
    std::uint64_t newBestGoldUntilMs{};
    std::uint64_t completedTimeMs{};
    bool endMenuWasVisible{};
    // A generation change alone is NOT a new level. Only the validated map
    // key or the completion-to-next-gameplay transition can change this scope.
    std::uint64_t generationAtCompletion{};
};
inline std::uint64_t levelBestFor(const LevelRecords& records,
    const std::string& key) noexcept {
    if(!levelRecordKeyValid(key))return 0;
    const auto it=records.bestMs.find(key);
    return it==records.bestMs.end()?0:it->second;
}
inline bool levelProgressSetMap(LevelProgress& progress,
    const std::string& verifiedKey) {
    if(!levelRecordKeyValid(verifiedKey))return false;
    if(progress.verifiedMapKey==verifiedKey)return false;
    progress.time={};
    progress.deaths=0;
    progress.verifiedMapKey=verifiedKey;
    progress.completed=false;
    progress.awaitingNextLevel=false;
    progress.bestUpdated=false;
    progress.newBestGoldUntilMs=0;
    progress.completedTimeMs=0;
    progress.endMenuWasVisible=false;
    progress.generationAtCompletion=0;
    return true;
}
inline void levelProgressAdvance(LevelProgress& progress,
    bool active,std::uint64_t nowMs) noexcept {
    advanceCampaignRunTime(progress.time,active&&!progress.completed,nowMs);
}
inline void levelProgressDeath(LevelProgress& progress) noexcept {
    if(!progress.completed&&progress.deaths<1000000000ULL)++progress.deaths;
}
inline bool levelProgressComplete(LevelProgress& progress,
    LevelRecords& records,std::uint64_t nowMs) {
    if(progress.completed)return false; // EndOfLevel nested screens repeat.
    progress.completed=true;
    progress.awaitingNextLevel=true;
    progress.completedTimeMs=progress.time.elapsedMs;
    progress.time.activeLastSample=false;
    progress.bestUpdated=false;
    if(!levelRecordKeyValid(progress.verifiedMapKey)
        ||!progress.completedTimeMs)return false;
    const auto best=levelBestFor(records,progress.verifiedMapKey);
    if(best&&progress.completedTimeMs>=best)return false;
    if(records.bestMs.size()>=kLevelRecordsMax && !best)return false;
    records.bestMs[progress.verifiedMapKey]=progress.completedTimeMs;
    progress.bestUpdated=true;
    progress.newBestGoldUntilMs=nowMs+kLevelBestGoldMs;
    return true;
}
inline void levelProgressNextUnidentified(LevelProgress& progress) noexcept {
    // Play time/deaths can resume in an unidentified level, but do not
    // recycle the last map's best into a different level.
    progress.time={};progress.deaths=0;progress.verifiedMapKey.clear();
    progress.completed=false;progress.awaitingNextLevel=false;
    progress.bestUpdated=false;progress.completedTimeMs=0;
    progress.endMenuWasVisible=false;progress.generationAtCompletion=0;
}
inline bool levelRecordsReset(LevelRecords& records,
    const std::string& key) {
    if(!levelRecordKeyValid(key))return false;
    return records.bestMs.erase(key)>0;
}
inline void levelRecordsResetAll(LevelRecords& records) noexcept {
    records.bestMs.clear();
}
inline bool levelRecordsLoad(const std::string& path,LevelRecords& out) {
    namespace fs=std::filesystem;
    std::error_code ec;
    if(!fs::exists(path,ec)||ec)return !ec;
    if(fs::file_size(path,ec)>16384u||ec)return false;
    std::ifstream f(path);
    if(!f)return false;
    std::string tag;unsigned version{};
    if(!(f>>tag>>version)||tag!="schema"||version!=1)return false;
    LevelRecords next{};
    std::string key;std::uint64_t ms{};
    while(f>>tag){
        if(tag!="best"||!(f>>key>>ms)||!levelRecordKeyValid(key)
            ||!ms||ms>kLevelRecordMaxMs
            ||next.bestMs.size()>=kLevelRecordsMax
            ||!next.bestMs.emplace(key,ms).second)return false;
        std::string extra;
        std::getline(f,extra);
        if(extra.find_first_not_of(" \t\r")!=std::string::npos)return false;
    }
    if(!f.eof())return false;
    out=std::move(next);
    return true;
}
inline bool levelRecordsSave(const std::string& path,
    const LevelRecords& records) {
    if(records.bestMs.size()>kLevelRecordsMax)return false;
    namespace fs=std::filesystem;
    const fs::path dest(path),tmp(path+".tmp");
    std::error_code ec;
    if(!dest.parent_path().empty()){
        fs::create_directories(dest.parent_path(),ec);
        if(ec)return false;
    }
    {
        std::ofstream out(tmp,std::ios::trunc);
        if(!out)return false;
        out<<"schema 1\n";
        for(const auto& [key,ms]:records.bestMs){
            if(!levelRecordKeyValid(key)||!ms||ms>kLevelRecordMaxMs)return false;
            out<<"best "<<key<<" "<<ms<<"\n";
        }
        out.flush();
        if(!out)return false;
    }
#ifdef _WIN32
    return MoveFileExW(tmp.wstring().c_str(),dest.wstring().c_str(),
        MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
#else
    fs::rename(tmp,dest,ec);
    return !ec;
#endif
}
} // namespace kharvox
