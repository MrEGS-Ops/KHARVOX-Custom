#include "../src/openxr/CinematicRefreshPolicy.h"
#include <cassert>
#include <initializer_list>

// Regression from Build #81: the October 5 native full-speed sync CVar
// was accidentally overwritten to 0 EVERY FRAME by immersive cinematic
// refresh. Native Glory and Chainsaw executions must preserve the opt-in 1.
int main() {
    using namespace kharvox;
    // Fast Glory and Fast Chainsaw use the SAME native sync flag.
    assert(nativeSyncAdaptiveTickDuringCinematic(true)==1);
    assert(nativeSyncAdaptiveTickAfterCinematic(true,0)==1);
    assert(nativeSyncAdaptiveTickAfterCinematic(true,1)==1);

    // With fast mods OFF, leave existing immersive refresh behavior and the
    // original/native saved value unchanged at release.
    assert(nativeSyncAdaptiveTickDuringCinematic(false)==0);
    assert(nativeSyncAdaptiveTickAfterCinematic(false,0)==0);
    assert(nativeSyncAdaptiveTickAfterCinematic(false,1)==1);

    // The present-path must honour either checkbox OR the legacy speed
    // selector; Build #426 overlooked the old slider and could write zero.
    for(bool glory : {false,true}) {
        for(bool chainsaw : {false,true}) {
            for(int legacySpeed : {0,5,9,10}) {
                const bool expected=glory||chainsaw||legacySpeed<10;
                const bool actual=nativeFastKillSyncRequested(
                    glory,chainsaw,legacySpeed);
                assert(actual==expected);
                assert(nativeSyncAdaptiveTickDuringCinematic(actual)
                    ==(expected?1:0));
                assert(nativeSyncAdaptiveTickAfterCinematic(actual,0)
                    ==(expected?1:0));
            }
        }
    }
    assert(!nativeFastKillSyncRequested(false,false,-1));

    // Real headset trace Oct 15: nominal display 120 Hz, achieved 39 Hz
    // (GK) and 36.8 Hz (chainsaw). Neither execution may force a 120 Hz
    // minimum game simulation rate or patch the adaptive tick runtime state.
    assert(!shouldForceCinematicAdaptiveTickPacing(true));
    assert(shouldForceCinematicAdaptiveTickPacing(false));
    assert(cinematicKillTargetTickHz(1,120)==120);
    assert(cinematicKillTargetTickHz(2,120)==90);
    assert(cinematicKillTargetTickHz(3,120)==70);
    assert(cinematicKillTargetTickHz(4,120)==50);
    assert(cinematicKillTargetTickHz(5,120)==30);
    for (int level=2; level<=5; ++level) {
        assert(!nativeFastKillSyncRequested(false,false,10,level));
        assert(shouldForceCinematicAdaptiveTickPacing(false,level));
    }
    assert(!nativeFastKillSyncRequested(false,false,10,1));
    assert(!nativeFastKillSyncRequested(false,false,10,5));
    assert(shouldForceCinematicAdaptiveTickPacing(false,5));
    assert(!shouldForceCinematicAdaptiveTickPacing(true,5));
    for(bool glory : {false,true}) {
        for(bool chainsaw : {false,true}) {
            const bool requested=nativeFastKillSyncRequested(
                glory,chainsaw,10);
            assert(shouldForceCinematicAdaptiveTickPacing(requested)
                ==!(glory||chainsaw));
        }
    }
    // The old Glory Kill Speed control is also native mode.
    assert(!shouldForceCinematicAdaptiveTickPacing(
        nativeFastKillSyncRequested(false,false,0)));
    // Original 90ms blend remains unchanged; now start during the first
    // portion of native sync instead of near its exit.
    assert(nativeSyncEntryHandoffDelayMs(0)==40);
    assert(nativeSyncEntryHandoffDelayMs(80)==80);
    assert(nativeSyncEntryHandoffDelayMs(999)==400);
    assert(!nativeSyncEarlyHandoffReady(80'000'000,1,80,true));
    assert(nativeSyncEarlyHandoffReady(80'000'001,1,80,true));
    assert(!nativeSyncEarlyHandoffReady(400'000'002,1,80,false));
    assert(!nativeSyncEarlyHandoffReady(50,0,40,true));
    assert(nativeSyncExitHandoffFraction(0,0)==0.f);
    assert(nativeSyncExitHandoffFraction(100,100)==0.f);
    assert(nativeSyncExitHandoffFraction(45'000'100,100)==.5f);
    assert(nativeSyncExitHandoffFraction(90'000'100,100)==1.f);
    assert(nativeSyncExitHandoffFraction(200'000'100,100)==1.f);
    return 0;
}
