// LevelStatsPolicy uses MoveFileExW in its Windows persistence path.
#ifdef _WIN32
#include <windows.h>
#endif
#include "../src/hud/LevelStatsPolicy.h"
#include <cassert>
#include <cstdint>

int main(){
    using namespace kharvox;
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
    return 0;
}
