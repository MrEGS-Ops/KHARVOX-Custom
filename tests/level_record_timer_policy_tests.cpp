// LevelStatsPolicy uses MoveFileExW in its Windows persistence path.
#ifdef _WIN32
// Avoid Windows' min/max macros replacing std::min/std::max in LevelStatsPolicy.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include "../src/hud/LevelStatsPolicy.h"
#include <cassert>
#include <cstdint>

int main(){
    using namespace kharvox;
    // The completed record is permanent, its XR result toast is not.
    assert(!levelResultNoticeVisible(0,20000));
    assert(!levelResultNoticeVisible(10000,9999));
    assert(levelResultNoticeVisible(10000,10000));
    assert(levelResultNoticeVisible(10000,14999));
    assert(!levelResultNoticeVisible(10000,15000)); // exact 5s deadline
    assert(!levelResultNoticeVisible(10000,500000)); // long after completion
    // Native Pause, main menu and rendering do not extend the deadline:
    // the completed result still exists, but visibility is purely elapsed time.
    const std::uint64_t completedNoticeAt=556420;
    assert(levelResultNoticeVisible(completedNoticeAt,completedNoticeAt+4999));
    assert(!levelResultNoticeVisible(completedNoticeAt,completedNoticeAt+5000));
    LevelProgress attempt{};
    const std::uint64_t start=1000;
    assert(levelProgressStartAttempt(attempt,"campaign/01",start));
    assert(attempt.armed&&!attempt.completed);
    assert(attempt.time.lastSampleMs==start);
    assert(attempt.time.elapsedMs==0);
    // 100ms normal gameplay, then the in-headset RIPSHIFT Mod Menu.
    levelProgressAdvance(attempt,false,1100);
    levelProgressAdvance(attempt,false,3100); // 2s Mod Menu, STILL counting
    assert(attempt.time.elapsedMs==2100);
    // Hayden's lock/dialogue/cinematics may last far longer than a frame.
    // No 250ms frame clamp from the ordinary campaign time policy applies.
    levelProgressAdvance(attempt,false,130100);
    assert(attempt.time.elapsedMs==129100);
    // Death, loading and reloading are not grounds to stop a ranked timer.
    levelProgressDeath(attempt);
    levelProgressAdvance(attempt,false,155100);
    assert(attempt.deaths==1&&attempt.time.elapsedMs==154100);
    // ONLY the actual native DOOM Pause session stops elapsed time.
    levelProgressAdvance(attempt,true,155110);
    const auto beforePause=attempt.time.elapsedMs;
    levelProgressAdvance(attempt,true,555110); // 400s paused
    assert(attempt.time.elapsedMs==beforePause);
    levelProgressAdvance(attempt,false,555120); // resume edge
    assert(attempt.time.elapsedMs==beforePause);
    levelProgressAdvance(attempt,false,556120);
    assert(attempt.time.elapsedMs==beforePause+1000);
    // Add the last wall-clock interval BEFORE end-of-level disarms timing.
    levelProgressAdvance(attempt,false,556420);
    assert(attempt.time.elapsedMs==beforePause+1300);
    LevelRecords records{};
    assert(levelProgressComplete(attempt,records,556420,20261117));
    assert(!attempt.armed&&attempt.completed);
    assert(attempt.completedTimeMs==beforePause+1300);
    const auto ranked=levelRankedRuns(records,"campaign/01",false);
    assert(ranked.size()==1&&ranked[0].timeMs==beforePause+1300
        &&ranked[0].deaths==1);
    levelProgressAdvance(attempt,false,999999);
    assert(attempt.time.elapsedMs==beforePause+1300);
    assert(!levelProgressComplete(attempt,records,999999,20261117));
    // Restarting the SAME map discards only active progress, not past bests.
    assert(levelProgressStartAttempt(attempt,"campaign/01",1000000));
    assert(attempt.time.elapsedMs==0&&attempt.deaths==0
        &&levelBestFor(records,"campaign/01")==beforePause+1300);
    levelProgressAdvance(attempt,false,1000500);
    assert(attempt.time.elapsedMs==500);
    // The regular Campaign/Session timers still skip non-playable time and
    // limit single-frame gaps to 250ms: changing those would be a regression.
    CampaignRunTime normal{};
    advanceCampaignRunTime(normal,true,1000);
    advanceCampaignRunTime(normal,false,3000); // cutscene excluded
    advanceCampaignRunTime(normal,true,5000);  // transition excluded
    advanceCampaignRunTime(normal,true,7000);  // capped to 250
    assert(normal.elapsedMs==250);

    // Live HUD counters work without TRACK THIS LEVEL and never save a PB.
    LevelLiveStats live{};
    LevelProgress noRecord{};
    assert(!live.started&&!noRecord.armed);
    levelLiveStart(live,2000,12); // 12 prior session deaths not this level
    assert(live.started&&live.deaths==0&&live.time.elapsedMs==0);
    levelLiveAdvance(live,false,2500);
    levelLiveObserveDeaths(live,13);
    assert(live.time.elapsedMs==500&&live.deaths==1);
    // Checkpoint reloads and cinematic/menu delays do not restart the level.
    levelLiveAdvance(live,false,122500);
    levelLiveObserveDeaths(live,14);
    assert(live.time.elapsedMs==120500&&live.deaths==2);
    assert(!live.awaitingNextLevel);
    levelLiveObserveEndMenu(live,false,42); // generation changed on reload
    assert(!levelLiveNextLevelReady(live,true,false,42));
    levelLiveAdvance(live,true,122600); // native Pause enters
    levelLiveAdvance(live,true,222600);
    assert(live.time.elapsedMs==120500);
    levelLiveAdvance(live,false,222700); // resume edge
    levelLiveAdvance(live,false,223700);
    assert(live.time.elapsedMs==121500);
    assert(records.runs.size()==1); // only completed TRACK attempt from above
    // After actual end-of-level results, next level may start fresh.
    levelLiveObserveEndMenu(live,true,42);
    assert(live.awaitingNextLevel);
    assert(!levelLiveNextLevelReady(live,false,true,43));
    assert(levelLiveNextLevelReady(live,true,false,43));
    levelLiveStart(live,224000,14);
    assert(live.time.elapsedMs==0&&live.deaths==0&&!live.awaitingNextLevel);
    // A DEV session-death reset must not count as negative deaths.
    levelLiveObserveDeaths(live,0);
    levelLiveObserveDeaths(live,1);
    assert(live.deaths==1);
    levelLiveResetCounters(live,225000,1);
    assert(live.deaths==0&&live.time.elapsedMs==0);
    assert(!levelStatsResetAllowed(true)&&levelStatsResetAllowed(false));
    // Only a settled title-screen context cancels an unfinished run;
    // brief loading frames or checkpoint transitions must not.
    std::uint64_t frontMenuSince{};
    assert(!levelFrontMenuConfirmed(false,1000,frontMenuSince));
    assert(!levelFrontMenuConfirmed(true,2000,frontMenuSince));
    assert(!levelFrontMenuConfirmed(true,2000+kLevelFrontMenuCancelDelayMs-1,frontMenuSince));
    assert(!levelFrontMenuConfirmed(false,12001,frontMenuSince));
    assert(frontMenuSince==0);
    assert(!levelFrontMenuConfirmed(true,13000,frontMenuSince));
    assert(levelFrontMenuConfirmed(true,13000+kLevelFrontMenuCancelDelayMs,frontMenuSince));
    // Exit to Desktop must abandon an unfinished run without saving a result.
    LevelProgress quitRun{};
    LevelRecords completedResults{};
    assert(levelRecordsAddRun(completedResults,"campaign/02",
        LevelRun{720000,2,20261117}));
    assert(levelProgressStartAttempt(quitRun,"campaign/02",2000000));
    levelProgressAdvance(quitRun,false,2075000);
    levelProgressDeath(quitRun);
    assert(quitRun.armed&&quitRun.time.elapsedMs==75000&&quitRun.deaths==1);
    levelProgressAbandonOnDesktopExit(quitRun);
    assert(!quitRun.armed&&!quitRun.completed
        &&!quitRun.time.activeLastSample);
    assert(!levelProgressComplete(quitRun,completedResults,2080000,20261117));
    assert(levelBestFor(completedResults,"campaign/02")==720000);
    assert(completedResults.runs.at("campaign/02").size()==1);
    levelProgressAbandonOnDesktopExit(quitRun); // shutdown can repeat safely
    LevelProgress alreadyFinished{};
    assert(levelProgressStartAttempt(alreadyFinished,"campaign/03",3000000));
    levelProgressAdvance(alreadyFinished,false,3200000);
    assert(levelProgressComplete(alreadyFinished,completedResults,3200000,20261117));
    levelProgressAbandonOnDesktopExit(alreadyFinished);
    assert(alreadyFinished.completed
        &&levelBestFor(completedResults,"campaign/03")==200000);
    // Manual STOP RECORD abandons an active run and preserves completed PBs.
    LevelProgress manualStop{};
    assert(levelProgressStartAttempt(manualStop,"campaign/02",4000000));
    levelProgressAdvance(manualStop,false,4080000);
    levelProgressDeath(manualStop);
    assert(manualStop.time.elapsedMs==80000&&manualStop.deaths==1);
    const auto beforeStopBest=levelBestFor(completedResults,"campaign/02");
    assert(levelProgressStopRecord(manualStop));
    assert(!manualStop.armed&&!manualStop.completed
        &&manualStop.time.elapsedMs==0&&manualStop.deaths==0);
    assert(!levelProgressComplete(manualStop,completedResults,4090000,20261117));
    assert(levelBestFor(completedResults,"campaign/02")==beforeStopBest);
    assert(!levelProgressStopRecord(manualStop)); // no active run
    assert(!levelProgressStopRecord(alreadyFinished)); // completed PB safe
    // A new attempt may be started normally without inheriting old deaths.
    assert(levelProgressStartAttempt(manualStop,"campaign/02",4100000));
    assert(manualStop.armed&&manualStop.time.elapsedMs==0&&manualStop.deaths==0);
    // The HUD must pair the fastest-time record with THAT run's death
    // count, never the unrelated fewest-deaths leaderboard result.
    LevelRecords pairedResults{};
    assert(levelFastestRunFor(pairedResults,"campaign/03")==nullptr);
    assert(levelRecordsAddRun(pairedResults,"campaign/03",
        LevelRun{1328000,2,20261117})); // 00:22:08, 2 deaths
    assert(levelRecordsAddRun(pairedResults,"campaign/03",
        LevelRun{1380000,0,20261117})); // fewer deaths, slower
    const auto originalBest=levelFastestRunFor(pairedResults,"campaign/03");
    assert(originalBest&&originalBest->timeMs==1328000
        &&originalBest->deaths==2);
    assert(levelBestFor(pairedResults,"campaign/03")==originalBest->timeMs);
    assert(levelRecordsAddRun(pairedResults,"campaign/03",
        LevelRun{1328000,1,20261118})); // equal time, fewer deaths wins
    const auto tiedBest=levelFastestRunFor(pairedResults,"campaign/03");
    assert(tiedBest&&tiedBest->timeMs==1328000&&tiedBest->deaths==1);
    assert(levelRecordsAddRun(pairedResults,"campaign/04",
        LevelRun{1800000,kLevelDeathsUnknown,0})); // legacy time-only
    const auto legacyBest=levelFastestRunFor(pairedResults,"campaign/04");
    assert(legacyBest&&legacyBest->timeMs==1800000
        &&legacyBest->deaths==kLevelDeathsUnknown);
    assert(levelFastestRunFor(pairedResults,"campaign/09")==nullptr);
    return 0;
}
