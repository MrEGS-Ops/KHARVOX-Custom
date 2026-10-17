#include "../src/openxr/CinematicRefreshPolicy.h"
#include "../src/openxr/ActionSpeedPolicy.h"
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
    // Foundry regression: passive cutscene when "Regular cinematics in
    // CineWindow" is unchecked was inadvertently setting minHz=120 from
    // headset cadence even with syncAttack=0. Projection cannot imply speed.
    assert(!shouldApplyImmersiveActionTiming(true,false,false,false,false));
    assert(!shouldApplyImmersiveActionTiming(false,false,false,false,false));
    assert(shouldApplyImmersiveActionTiming(true,true,false,false,false));
    assert(shouldApplyImmersiveActionTiming(true,false,true,false,false));
    assert(shouldApplyImmersiveActionTiming(true,false,false,true,false));
    // Explicit action override is allowed to borrow the scoped timing hold.
    assert(shouldApplyImmersiveActionTiming(false,false,false,false,true));
    assert(!shouldApplyImmersiveActionTiming(false,true,false,false,false));
    // Diagnostic run regression: 60/61 Glory Kills wrongly handed their
    // exit frames to "Scripted cinematics", including 14 cases at 240 Hz
    // for about 0.5-0.8 s. The state gate MUST NOT add game/camera delay:
    // it only rejects false classifications until that cutscene is over.
    ScriptedCinematicBoundaryState gate{};
    assert(acceptIndependentScriptedCinematic(gate,true,false,false,false,1000));
    assert(!acceptIndependentScriptedCinematic(gate,true,true,false,false,1100));
    assert(gate.awaitingPreviousCutsceneEnd);
    assert(!acceptIndependentScriptedCinematic(gate,true,false,false,false,1200));
    assert(!acceptIndependentScriptedCinematic(gate,true,false,false,false,1750));
    assert(!acceptIndependentScriptedCinematic(gate,false,false,false,false,1800));
    assert(!gate.awaitingPreviousCutsceneEnd);
    assert(!acceptIndependentScriptedCinematic(gate,true,false,false,false,1920));
    assert(acceptIndependentScriptedCinematic(gate,true,false,false,false,2050));
    // Chainsaw and ledge exits must have the same protection.
    assert(!acceptIndependentScriptedCinematic(gate,true,false,true,false,2100));
    assert(!acceptIndependentScriptedCinematic(gate,true,false,false,false,3100));
    assert(!acceptIndependentScriptedCinematic(gate,false,false,false,false,3150));
    assert(!acceptIndependentScriptedCinematic(gate,true,false,false,false,3350));
    assert(acceptIndependentScriptedCinematic(gate,true,false,false,false,3400));
    assert(!acceptIndependentScriptedCinematic(gate,true,false,false,true,3500));
    assert(!acceptIndependentScriptedCinematic(gate,false,false,false,false,3600));
    assert(acceptIndependentScriptedCinematic(gate,true,false,false,false,3900));
    // Fresh standalone scripted interactions remain usable immediately.
    ScriptedCinematicBoundaryState independent{};
    assert(acceptIndependentScriptedCinematic(
        independent,true,false,false,false,0));
    // Saturate the grace deadline near ULONG_LONG_MAX without wrapping.
    assert(safeScriptedGateDeadline(~0ull-10ull,250)==~0ull);
    // Action targets never falsely claim independent enemy/reload clocks.
    assert(!validActionSpeedTarget(0));
    assert(!validActionSpeedTarget(29));
    assert(validActionSpeedTarget(30));
    assert(validActionSpeedTarget(240));
    assert(!validActionSpeedTarget(241));
    const ActionSpeedTargets target{50,60,80,170};
    assert(selectActionSpeed({true,true,false},target).kind
        ==ActionSpeedKind::Chainsaw);
    assert(selectActionSpeed({true,false,false},target).hz==60);
    assert(selectActionSpeed({false,false,true},target).hz==80);
    assert(selectActionSpeed({},target).kind==ActionSpeedKind::None);
    assert(selectActionSpeed({true,false,false},{}).hz==0);
    assert(selectActionSpeed({true,true,true},target).hz==50);
    assert(selectActionSpeed({false,false,false,true},target).kind
        ==ActionSpeedKind::ScriptedCinematic);
    assert(selectActionSpeed({false,false,false,true},target).hz==170);
    assert(selectActionSpeed({false,false,false,true},{}).hz==0);
    // A cutscene classifier may outlive the execution's native flag by a
    // frame; never allow scripted cinematic to steal a kill/ledge target.
    assert(selectActionSpeed({true,false,false,true},target).hz==60);
    assert(selectActionSpeed({false,true,false,true},target).hz==50);
    assert(selectActionSpeed({false,false,true,true},target).hz==80);
    assert(selectActionSpeed({true,false,false,true},{0,0,0,170}).hz==0);
    assert(selectActionSpeed({false,false,true,true},{0,0,0,170}).hz==0);
    assert(selectActionSpeed({false,false,false,true},{0,0,0,241}).hz==0);

    return 0;
}
