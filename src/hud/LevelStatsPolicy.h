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
#include <vector>
#include <numeric>
#include <limits>
#include <iomanip>
#include <utility>

// Native level-transition generations change on checkpoint reloads too.
// Record attribution therefore requires the player's explicit mission choice.
namespace kharvox {
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
inline constexpr std::size_t kLevelRecordsMax=64;
inline constexpr std::size_t kLevelRankLimit=5;
inline constexpr std::size_t kLevelStoredRunsPerMission=kLevelRankLimit*2;
inline constexpr std::uint64_t kLevelRecordMaxMs=100000000000ULL;
inline constexpr std::uint32_t kLevelDeathsUnknown=std::numeric_limits<std::uint32_t>::max();
inline bool levelRecordKeyValid(std::string_view key) noexcept {
    if(key.empty()||key.size()>95)return false;
    for(unsigned char c:key)
        if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')
            ||(c>='0'&&c<='9')||c=='_'||c=='-'||c=='/'
            ||c=='.'))return false;
    return key.find("..")==std::string_view::npos;
}
inline bool levelRecordDateValid(std::uint32_t ymd) noexcept {
    if(ymd==0)return true; // migrated schema 1 did not record dates
    const unsigned year=ymd/10000,month=(ymd/100)%100,day=ymd%100;
    if(year<2000||year>2100||month<1||month>12||day<1)return false;
    constexpr unsigned days[]{0,31,28,31,30,31,30,31,31,30,31,30,31};
    const bool leap=year%4==0&&(year%100!=0||year%400==0);
    return day<=days[month]+(month==2&&leap?1:0);
}
inline std::string levelRecordDateText(std::uint32_t ymd) {
    if(!ymd||!levelRecordDateValid(ymd))return "--/--/----";
    std::ostringstream o;
    o<<std::setfill('0')<<std::setw(2)<<ymd%100<<'/'
      <<std::setw(2)<<(ymd/100)%100<<'/'<<std::setw(4)<<ymd/10000;
    return o.str();
}
struct LevelRun {
    std::uint64_t timeMs{};
    std::uint32_t deaths{kLevelDeathsUnknown};
    std::uint32_t dateYmd{}; // local YYYYMMDD; 0=unknown migrated record
};
inline bool levelRunValid(const LevelRun& run) noexcept {
    return run.timeMs>0&&run.timeMs<=kLevelRecordMaxMs
        &&(run.deaths<=1000000000u||run.deaths==kLevelDeathsUnknown)
        &&levelRecordDateValid(run.dateYmd);
}
inline bool levelRunLess(const LevelRun& a,const LevelRun& b,bool deathsFirst) noexcept {
    if(deathsFirst&&a.deaths!=b.deaths)return a.deaths<b.deaths;
    if(a.timeMs!=b.timeMs)return a.timeMs<b.timeMs;
    if(a.deaths!=b.deaths)return a.deaths<b.deaths;
    return a.dateYmd>b.dateYmd; // same performance: more recent first
}
struct LevelRecords {
    // Union of the best 5 by time and best 5 by deaths. At most 10 entries
    // per mission: neither sort can evict records needed for the other sort.
    std::map<std::string,std::vector<LevelRun>> runs;
};
inline std::vector<LevelRun> levelRankedRuns(const LevelRecords& records,
    const std::string& key,bool byDeaths) {
    const auto it=records.runs.find(key);
    if(it==records.runs.end())return {};
    auto ranked=it->second;
    std::stable_sort(ranked.begin(),ranked.end(),
        [byDeaths](const LevelRun&a,const LevelRun&b){return levelRunLess(a,b,byDeaths);});
    if(ranked.size()>kLevelRankLimit)ranked.resize(kLevelRankLimit);
    return ranked;
}
inline std::uint64_t levelBestFor(const LevelRecords& records,
    const std::string& key) noexcept {
    if(!levelRecordKeyValid(key))return 0;
    const auto it=records.runs.find(key);
    if(it==records.runs.end()||it->second.empty())return 0;
    std::uint64_t best=it->second.front().timeMs;
    for(const auto& run:it->second)best=std::min(best,run.timeMs);
    return best;
}
inline bool levelRecordsAddRun(LevelRecords& records,const std::string& key,
    const LevelRun& result) {
    if(!levelRecordKeyValid(key)||!levelRunValid(result))return false;
    auto it=records.runs.find(key);
    if(it==records.runs.end()){
        if(records.runs.size()>=kLevelRecordsMax)return false;
        it=records.runs.emplace(key,std::vector<LevelRun>{}).first;
    }
    auto& entries=it->second;
    entries.push_back(result);
    const std::size_t newEntry=entries.size()-1;
    std::vector<std::size_t> ordered(entries.size());
    std::iota(ordered.begin(),ordered.end(),std::size_t{0});
    std::vector<bool> keep(entries.size(),false);
    for(const bool byDeaths:{false,true}){
        std::stable_sort(ordered.begin(),ordered.end(),
            [&](std::size_t a,std::size_t b){
                return levelRunLess(entries[a],entries[b],byDeaths);});
        for(std::size_t i=0;i<std::min(kLevelRankLimit,ordered.size());++i)
            keep[ordered[i]]=true;
    }
    const bool retained=keep[newEntry];
    std::vector<LevelRun> bounded;bounded.reserve(kLevelStoredRunsPerMission);
    for(std::size_t i=0;i<entries.size();++i)
        if(keep[i])bounded.push_back(entries[i]);
    entries=std::move(bounded);
    return retained;
}
struct LevelProgress {
    CampaignRunTime time{};
    std::uint64_t deaths{};
    std::string verifiedMapKey;
    bool armed{}; // only explicit double confirmation can arm a record run
    bool completed{};
    bool awaitingNextLevel{};
    bool bestUpdated{};
    std::uint64_t previousBestMs{};
    std::uint64_t bestImprovementMs{};
    std::uint64_t completedTimeMs{};
    bool endMenuWasVisible{};
    std::uint64_t generationAtCompletion{};
};
inline bool levelProgressStartAttempt(LevelProgress& progress,
    const std::string& verifiedKey,std::uint64_t nowMs=0) {
    if(!levelRecordKeyValid(verifiedKey))return false;
    progress=LevelProgress{};
    progress.verifiedMapKey=verifiedKey;
    progress.armed=true;
    // The competitive clock begins at the user's confirmation, not at the
    // next rendered frame or after the RIPSHIFT Mod Menu closes.
    progress.time.lastSampleMs=nowMs;
    progress.time.activeLastSample=true;
    return true;
}
inline bool levelProgressSetMap(LevelProgress& progress,
    const std::string& verifiedKey) {
    if(!levelRecordKeyValid(verifiedKey))return false;
    if(progress.verifiedMapKey==verifiedKey)return false;
    progress=LevelProgress{};
    progress.verifiedMapKey=verifiedKey;
    // Merely discovering/choosing a map never starts an attempt.
    return true;
}
// Competitive and live-level clocks use true monotonic elapsed time. Hayden
// dialogue, cutscenes, Mod Menu, deaths and checkpoint loads all count;
// ONLY native DOOM Pause suspends them. Regular Campaign/Session timers retain
// their original playable-gameplay and frame-cap policy.
inline void advanceLevelElapsedTime(CampaignRunTime& time,
    bool nativeGamePaused,std::uint64_t nowMs) noexcept {
    if(time.activeLastSample&&!nativeGamePaused&&time.lastSampleMs
        &&nowMs>time.lastSampleMs){
        time.elapsedMs+=nowMs-time.lastSampleMs;
    }
    time.lastSampleMs=nowMs;
    time.activeLastSample=!nativeGamePaused;
}
inline void levelProgressAdvance(LevelProgress& progress,
    bool nativeGamePaused,std::uint64_t nowMs) noexcept {
    if(!progress.armed||progress.completed)return;
    advanceLevelElapsedTime(progress.time,nativeGamePaused,nowMs);
}
// Separate live HUD measurements from the opt-in ranked record attempt.
// Showing LEVEL TIME/LEVEL DEATHS must never arm a record or save one.
struct LevelLiveStats {
    CampaignRunTime time{};
    std::uint64_t deaths{};
    std::uint64_t observedSessionDeaths{};
    std::uint64_t generationAtCompletion{};
    bool started{};
    bool awaitingNextLevel{};
    bool endMenuWasVisible{};
};
inline void levelLiveStart(LevelLiveStats& live,std::uint64_t nowMs,
    std::uint64_t sessionDeaths) noexcept {
    live=LevelLiveStats{};
    live.started=true;
    live.observedSessionDeaths=sessionDeaths;
    live.time.lastSampleMs=nowMs;
    live.time.activeLastSample=true;
}
inline void levelLiveResetCounters(LevelLiveStats& live,std::uint64_t nowMs,
    std::uint64_t sessionDeaths) noexcept {
    if(!live.started)return;
    resetCampaignRunTime(live.time);
    live.time.lastSampleMs=nowMs;
    live.time.activeLastSample=true;
    live.deaths=0;
    live.observedSessionDeaths=sessionDeaths;
}
inline void levelLiveObserveDeaths(LevelLiveStats& live,
    std::uint64_t sessionDeaths) noexcept {
    if(!live.started)return;
    if(sessionDeaths>live.observedSessionDeaths){
        const auto added=(std::min)(
            sessionDeaths-live.observedSessionDeaths,1000000000ULL-live.deaths);
        live.deaths+=added;
    }
    // An explicit DEV session-deaths reset can reduce the source counter.
    live.observedSessionDeaths=sessionDeaths;
}
inline void levelLiveAdvance(LevelLiveStats& live,
    bool nativeGamePaused,std::uint64_t nowMs) noexcept {
    if(live.started&&!live.awaitingNextLevel)
        advanceLevelElapsedTime(live.time,nativeGamePaused,nowMs);
}
inline void levelLiveObserveEndMenu(LevelLiveStats& live,
    bool endMenuVisible,std::uint64_t levelGeneration) noexcept {
    if(endMenuVisible&&!live.endMenuWasVisible&&live.started){
        live.awaitingNextLevel=true;
        live.generationAtCompletion=levelGeneration;
    }
    live.endMenuWasVisible=endMenuVisible;
}
inline bool levelLiveNextLevelReady(const LevelLiveStats& live,
    bool gameplay,bool endMenuVisible,std::uint64_t levelGeneration) noexcept {
    return live.started&&live.awaitingNextLevel&&gameplay&&!endMenuVisible
        &&levelGeneration!=live.generationAtCompletion;
}
// DOOM's current level generation also changes on checkpoint reloads. Do
// NOT treat generation alone as a new level, or a death erases live stats.
inline constexpr std::uint64_t kLevelFrontMenuCancelDelayMs=10000ULL;
inline bool levelFrontMenuConfirmed(bool frontMenuContext,
    std::uint64_t nowMs,std::uint64_t& candidateSinceMs) noexcept {
    if(!frontMenuContext){candidateSinceMs=0;return false;}
    if(!candidateSinceMs){candidateSinceMs=nowMs;return false;}
    return nowMs>=candidateSinceMs
        &&nowMs-candidateSinceMs>=kLevelFrontMenuCancelDelayMs;
}
inline constexpr bool levelStatsResetAllowed(bool recordArmed) noexcept {
    return !recordArmed;
}
// Exit to Desktop abandons an unfinished ranked run, never saving a PB.
// No disk I/O or allocations: invoked during DLL_PROCESS_DETACH.
inline void levelProgressAbandonOnDesktopExit(LevelProgress& progress) noexcept {
    if(progress.armed&&!progress.completed){
        progress.armed=false;
        progress.time.activeLastSample=false;
    }
}
inline void levelProgressDeath(LevelProgress& progress) noexcept {
    if(progress.armed&&!progress.completed&&progress.deaths<1000000000ULL)
        ++progress.deaths;
}
inline bool levelProgressComplete(LevelProgress& progress,
    LevelRecords& records,std::uint64_t /*nowMs*/,std::uint32_t localDateYmd=0) {
    if(!progress.armed||progress.completed)return false;
    progress.armed=false; // disarm IMMEDIATELY at end of level
    progress.completed=true;
    progress.awaitingNextLevel=true;
    progress.completedTimeMs=progress.time.elapsedMs;
    progress.time.activeLastSample=false;
    progress.bestUpdated=false;
    progress.previousBestMs=0;
    progress.bestImprovementMs=0;
    if(!levelRecordKeyValid(progress.verifiedMapKey)
        ||!progress.completedTimeMs)return false;
    const auto priorBest=levelBestFor(records,progress.verifiedMapKey);
    progress.previousBestMs=priorBest;
    const auto deaths=static_cast<std::uint32_t>(
        std::min<std::uint64_t>(progress.deaths,1000000000ULL));
    const bool kept=levelRecordsAddRun(records,progress.verifiedMapKey,
        LevelRun{progress.completedTimeMs,deaths,localDateYmd});
    if(kept&&(!priorBest||progress.completedTimeMs<priorBest)){
        progress.bestUpdated=true;
        if(priorBest)progress.bestImprovementMs=priorBest-progress.completedTimeMs;
    }
    return kept; // persist any newly ranked run, not only time PB
}
inline void levelProgressNextUnidentified(LevelProgress& progress) noexcept {
    // Keep no old mission identity in the new level. A new record is impossible
    // until the player explicitly chooses a mission and confirms START.
    progress=LevelProgress{};
}
inline bool levelRecordsReset(LevelRecords& records,const std::string& key) {
    if(!levelRecordKeyValid(key))return false;
    return records.runs.erase(key)>0;
}
inline void levelRecordsResetAll(LevelRecords& records) noexcept {
    records.runs.clear();
}
inline bool levelRecordsLoad(const std::string& path,LevelRecords& out) {
    namespace fs=std::filesystem;
    std::error_code ec;
    if(!fs::exists(path,ec)||ec)return !ec;
    if(fs::file_size(path,ec)>65536u||ec)return false;
    std::ifstream f(path);
    if(!f)return false;
    std::string tag;unsigned version{};
    if(!(f>>tag>>version)||tag!="schema"||(version!=1&&version!=2))
        return false;
    std::string rest;std::getline(f,rest);
    if(rest.find_first_not_of(" \t\r")!=std::string::npos)return false;
    LevelRecords next{};
    std::string line;std::size_t lines{};
    while(std::getline(f,line)){
        if(++lines>kLevelRecordsMax*kLevelStoredRunsPerMission)return false;
        std::istringstream row(line);
        std::string kind,key,extra;
        std::uint64_t ms{};
        if(!(row>>kind>>key>>ms)||!levelRecordKeyValid(key))return false;
        LevelRun run{};run.timeMs=ms;
        if(version==1){
            if(kind!="best"||(row>>extra))return false;
            // Legacy saves contained only a best time, not deaths/date.
            // Never pretend unknown historical deaths were zero.
        }else{
            std::uint64_t deaths{};std::uint32_t date{};
            if(kind!="run"||!(row>>deaths>>date)||(row>>extra)
                ||deaths>std::numeric_limits<std::uint32_t>::max())return false;
            run.deaths=static_cast<std::uint32_t>(deaths);
            run.dateYmd=date;
        }
        if(!levelRunValid(run))return false;
        if(!levelRecordsAddRun(next,key,run))return false;
    }
    if(!f.eof())return false;
    out=std::move(next);
    return true;
}
inline bool levelRecordsSave(const std::string& path,
    const LevelRecords& records) {
    if(records.runs.size()>kLevelRecordsMax)return false;
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
        out<<"schema 2\n";
        for(const auto& [key,runs]:records.runs){
            if(!levelRecordKeyValid(key)||runs.empty()
                ||runs.size()>kLevelStoredRunsPerMission)return false;
            for(const auto& entry:runs){
                if(!levelRunValid(entry))return false;
                out<<"run "<<key<<' '<<entry.timeMs<<' '<<entry.deaths
                   <<' '<<entry.dateYmd<<'\n';
            }
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
