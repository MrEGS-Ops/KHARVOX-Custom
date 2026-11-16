#include "../src/openxr/VrHandMenuPolicy.h"
#include "../src/openxr/ManualCrouchPolicy.h"
#include "../src/hud/CampaignDeathCounterPolicy.h"
#include "../src/hud/CampaignStatsPolicy.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
int main(){
    using namespace kharvox;
    VrMenuNavigation nav{};
    // Campaign death detection never depends on whether the overlay is
    // visible. Each native death-menu opening is counted exactly once.
    assert(campaignDeathOpeningIsNew(false,0,0));
    assert(!campaignDeathOpeningIsNew(true,4,4));
    assert(campaignDeathOpeningIsNew(true,4,5));
    assert(campaignDeathOpeningIsNew(false,4,4));
    VrLiveSettings counterSettings{};
    assert(!counterSettings.deathCounterEnabled);
    assert(counterSettings.deathCounterCorner==1);
    assert(vrLiveModEditable(VrLiveMod::DeathCounter));
    assert(vrLiveModToggleable(VrLiveMod::DeathCounter));
    assert(!vrLiveEffectiveEnabled(counterSettings,VrLiveMod::DeathCounter,true));
    assert(vrLiveSetEnabled(counterSettings,VrLiveMod::DeathCounter,true));
    assert(vrLiveEffectiveEnabled(counterSettings,VrLiveMod::DeathCounter,false));
    assert(counterSettings.deathCounterCorner==1);
    assert(deathCounterPanelX(0,960,226)==32);
    assert(deathCounterPanelX(1,960,226)==702);
    assert(deathCounterPanelX(2,960,226)==32);
    assert(deathCounterPanelX(3,960,226)==702);
    assert(deathCounterPanelY(0,720,60)==28);
    assert(deathCounterPanelY(1,720,60)==28);
    assert(deathCounterPanelY(2,720,60)==632);
    assert(deathCounterPanelY(3,720,60)==632);
    assert(deathCounterShouldDraw(true,true,true,false,false));
    assert(!deathCounterShouldDraw(false,true,true,false,false));
    assert(!deathCounterShouldDraw(true,false,true,false,false));
    assert(!deathCounterShouldDraw(true,true,false,false,false));
    assert(!deathCounterShouldDraw(true,true,true,true,false));
    assert(!deathCounterShouldDraw(true,true,true,false,true));
    assert(vrLiveValidSettings(counterSettings));
    // New stats take ownership of their corner by disabling the previous
    // screen item. No automatic repositioning or stacking is allowed.
    VrLiveSettings stats{};
    assert(!stats.deathCounterEnabled&&!stats.deathStatisticsEnabled
        &&!stats.campaignTimerEnabled);
    assert(vrStatsSetEnabled(stats,0,true));
    assert(stats.deathCounterEnabled);
    assert(vrStatsSetEnabled(stats,1,true));
    assert(!stats.deathCounterEnabled&&stats.deathStatisticsEnabled);
    // Replacement and position changes NEVER close the Stats menu.
    assert(!vrStatsMenuShouldClose(false,110,true));
    assert(!vrStatsMenuShouldClose(false,130,true));
    assert(!vrStatsMenuShouldClose(false,131,true));
    assert(!vrStatsMenuShouldClose(false,132,true));
    assert(!vrStatsMenuShouldClose(false,133,true));
    // Successful individual DEV resets remain open; failed RESET ALL
    // remains open too. Only successfully persisted RESET ALL closes.
    assert(!vrStatsMenuShouldClose(true,130,true));
    assert(!vrStatsMenuShouldClose(true,131,false));
    assert(vrStatsMenuShouldClose(true,131,true));
    // Timer top-left and statistics top-right can coexist.
    assert(vrStatsSetEnabled(stats,2,true));
    assert(stats.campaignTimerEnabled&&stats.deathStatisticsEnabled);
    // Assigning the timer to the stat's occupied corner disables statistics.
    assert(vrStatsSetCorner(stats,2,1));
    assert(stats.campaignTimerEnabled&&!stats.deathStatisticsEnabled);
    assert(!vrStatsMenuShouldClose(false,131,true));
    assert(vrStatsCorner(stats,2)==1);
    // A hidden item's position preference does not displace visible items.
    assert(vrStatsSetCorner(stats,0,1));
    assert(!stats.deathCounterEnabled&&stats.campaignTimerEnabled);
    // Enabling the Death Counter in that corner now disables the timer.
    assert(vrStatsSetEnabled(stats,0,true));
    assert(stats.deathCounterEnabled&&!stats.campaignTimerEnabled);
    assert(vrStatsSetEnabled(stats,1,true));
    assert(!stats.deathCounterEnabled&&stats.deathStatisticsEnabled);
    assert(vrLiveValidSettings(stats));
    assert(!vrStatsSetCorner(stats,2,4));
    assert(!vrStatsSetEnabled(stats,-1,true));
    CampaignRunTime run{};
    advanceCampaignRunTime(run,true,1000);
    advanceCampaignRunTime(run,true,1016);
    assert(run.elapsedMs==16);
    advanceCampaignRunTime(run,false,2000);
    advanceCampaignRunTime(run,true,6000);
    advanceCampaignRunTime(run,true,6020);
    assert(run.elapsedMs==36); // no pause gap counted
    assert(campaignRunTimeText(3723000)=="01:02:03");
    assert(campaignStatsPanelWidth(0)==226);
    assert(campaignStatsPanelWidth(1)==316);
    resetCampaignRunTime(run);
    assert(run.elapsedMs==0&&!run.activeLastSample);
    assert(campaignStatsCanDraw(true,true,false,false));
    assert(!campaignStatsCanDraw(true,true,false,true));
    assert(!campaignStatsCanDraw(true,false,false,false));
    counterSettings.deathCounterCorner=4;
    assert(!vrLiveValidSettings(counterSettings));
    // Fine HUD placement: 120 logical slots, with four central cells
    // deliberately unavailable to keep the centre of view clear.
    assert(kStatsGridColumns==12&&kStatsGridRows==10);
    assert(statsGridCellFromPosition(0)==0);
    assert(statsGridCellFromPosition(1)==11);
    assert(statsGridCellFromPosition(2)==108);
    assert(statsGridCellFromPosition(3)==119);
    assert(statsGridCellFromPosition(statsPositionFromGridCell(107))==107);
    assert(statsPositionFromGridCell(53)==-1);
    assert(statsPositionFromGridCell(66)==-1);
    assert(statsMenuGridCellAt(kStatsMenuGridX+4,kStatsMenuGridY+4)==0);
    assert(statsMenuGridCellAt(kStatsMenuGridX+5*kStatsMenuGridCellWidth+4,
        kStatsMenuGridY+4*kStatsMenuGridCellHeight+4)==-1);
    assert(statsPanelX(1,960,226)==702); // legacy untouched
    assert(statsPanelY(1,720,60)==28);
    assert(statsPanelX(statsPositionFromGridCell(11),960,316)==612);
    assert(statsPanelY(statsPositionFromGridCell(12),720,60)>88);
    VrLiveSettings fineGrid{};
    assert(vrStatsSetCorner(fineGrid,1,statsPositionFromGridCell(0)));
    assert(vrStatsSetEnabled(fineGrid,1,true));
    assert(vrStatsSetCorner(fineGrid,2,statsPositionFromGridCell(12)));
    assert(vrStatsSetEnabled(fineGrid,2,true));
    // Separate vertical cells allow exact left-edge alignment and stacking.
    assert(fineGrid.deathStatisticsEnabled&&fineGrid.campaignTimerEnabled);
    assert(statsPanelX(fineGrid.deathStatisticsCorner,960,316)
        ==statsPanelX(fineGrid.campaignTimerCorner,960,226));
    assert(!vrStatsPositionsOverlap(1,fineGrid.deathStatisticsCorner,
        2,fineGrid.campaignTimerCorner));
    // A nearby horizontal cell still overlaps physically, so the newly
    // selected display replaces the old one rather than drawing over it.
    assert(vrStatsSetCorner(fineGrid,2,statsPositionFromGridCell(1)));
    assert(fineGrid.campaignTimerEnabled&&!fineGrid.deathStatisticsEnabled);
    assert(!vrStatsSetCorner(fineGrid,2,statsPositionFromGridCell(53)));
    assert(vrLiveValidSettings(fineGrid));
    VrMenuNavigation fineNav{};
    int fineSelected=0;
    vrStatsNavigateStick(fineNav,.95f,0.f,fineSelected,0);
    assert(fineNav.settingsColumn&&fineNav.actionIndex==0);
    vrStatsNavigateStick(fineNav,0.f,0.f,fineSelected,0);
    vrStatsNavigateStick(fineNav,0.f,-.95f,fineSelected,0);
    assert(fineNav.actionIndex==1);
    vrStatsNavigateStick(fineNav,0.f,0.f,fineSelected,0);
    vrStatsNavigateStick(fineNav,0.f,-.95f,fineSelected,0);
    assert(fineNav.actionIndex==1+kStatsGridColumns);
    fineNav.actionIndex=1+52; // row 4, column 4, next two cells blocked
    fineNav.horizontalCentered=true;
    vrStatsNavigateStick(fineNav,.95f,0.f,fineSelected,0);
    assert(fineNav.actionIndex==1+55);
    // Saved fine-grid cells round-trip using the same schema 1 UserData
    // format, while legacy integer corner preferences still remain readable.
    const auto fineFile=std::filesystem::temp_directory_path()
        /"ripshift_stats_grid_roundtrip.cfg";
    assert(vrLiveSave(fineFile.string(),fineGrid));
    VrLiveSettings loadedFine{};
    assert(vrLiveLoad(fineFile.string(),loadedFine));
    assert(loadedFine.campaignTimerCorner==fineGrid.campaignTimerCorner);
    assert(loadedFine.campaignTimerEnabled==fineGrid.campaignTimerEnabled);
    std::error_code fineFileError;
    std::filesystem::remove(fineFile,fineFileError);
    
    int selected=0;
    // The hidden DEV submenu has a distinct page and only diagnostic
    // toggles; none of its developer entries becomes a normal mod checkbox.
    assert(kharvox::vrMenuDevOptionToggleable(VrMenuDevOption::WeaponAlignment));
    assert(kharvox::vrMenuDevOptionToggleable(VrMenuDevOption::HandFocus));
    assert(kharvox::vrMenuDevOptionToggleable(VrMenuDevOption::HolsterZones));
    assert(kharvox::vrMenuDevOptionToggleable(VrMenuDevOption::ControllerMonitor));
    assert(!kharvox::vrMenuDevOptionToggleable(VrMenuDevOption::Folder));
    assert(!kharvox::vrMenuDevOptionToggleable(VrMenuDevOption::Experimental));
    assert(!kharvox::vrMenuDevOptionToggleable(VrMenuDevOption::WeaponStability));
    // DEV overlays are calibration-only. A checkbox being enabled is not
    // enough to draw anything in ordinary gameplay or the settings list.
    assert(vrDevCalibrationPanelVisible(VrHandMenuMode::Calibrating,
        VrLiveCapture::None,true,false,false));
    assert(!vrDevCalibrationPanelVisible(VrHandMenuMode::Menu,
        VrLiveCapture::None,true,true,true));
    assert(!vrDevCalibrationPanelVisible(VrHandMenuMode::Calibrating,
        VrLiveCapture::None,false,true,true));
    assert(vrDevCalibrationPanelVisible(VrHandMenuMode::Calibrating,
        VrLiveCapture::HipRight,false,true,false));
    assert(vrDevCalibrationPanelVisible(VrHandMenuMode::Calibrating,
        VrLiveCapture::WheelLeft,false,false,true));
    assert(!vrDevCalibrationPanelVisible(VrHandMenuMode::Calibrating,
        VrLiveCapture::CrouchStanding,true,true,true));
    assert(!vrDevCalibrationPanelVisible(VrHandMenuMode::Calibrating,
        VrLiveCapture::HipRight,true,false,false));
    // First-install setup must be a true modal prompt on the front menu,
    // not an R3 hint during loading or a controller reconnect request.
    VrFirstTimeSetupNotice setup{};
    for(int i=0;i<220;++i)
        assert(!updateVrFirstTimeSetupNotice(setup,false,false,false,true));
    assert(!setup.visible&&!setup.acknowledged);
    for(int i=0;i<179;++i)
        assert(!updateVrFirstTimeSetupNotice(setup,true,false,false,true));
    assert(!setup.visible);
    assert(!updateVrFirstTimeSetupNotice(setup,true,false,false,true));
    assert(setup.visible&&setup.bothSticksReleased);
    assert(!updateVrFirstTimeSetupNotice(setup,true,true,false,true));
    assert(!updateVrFirstTimeSetupNotice(setup,true,false,false,true));
    assert(!updateVrFirstTimeSetupNotice(setup,true,true,true,false));
    assert(updateVrFirstTimeSetupNotice(setup,true,true,true,true));
    assert(!updateVrFirstTimeSetupNotice(setup,true,true,true,true)); // edge only
    setup.acknowledged=true; // file write was successful
    assert(!updateVrFirstTimeSetupNotice(setup,true,false,false,true));
    assert(!setup.visible);
    VrFirstTimeSetupNotice preHeld{};
    for(int i=0;i<180;++i)
        assert(!updateVrFirstTimeSetupNotice(preHeld,true,true,true,true));
    assert(preHeld.visible&&!preHeld.bothSticksReleased);
    assert(!updateVrFirstTimeSetupNotice(preHeld,true,false,false,true));
    assert(updateVrFirstTimeSetupNotice(preHeld,true,true,true,true));
    VrFirstTimeSetupNotice interrupted{};
    for(int i=0;i<160;++i)
        updateVrFirstTimeSetupNotice(interrupted,true,false,false,true);
    assert(!updateVrFirstTimeSetupNotice(interrupted,false,false,false,true));
    for(int i=0;i<20;++i)
        updateVrFirstTimeSetupNotice(interrupted,true,false,false,true);
    assert(!interrupted.visible); // loading transition reset the counter
    // Gameplay's READY TO PLAY notice is independent of first-install
    // main-menu setup, persists across restarts and never accepts a lone R3.
    VrGameplayWelcomeNotice welcome{};
    for(int i=0;i<80;++i)
        assert(!updateVrGameplayWelcomeNotice(welcome,false,false,false,true));
    assert(!welcome.visible&&!welcome.acknowledged);
    for(int i=0;i<19;++i)
        assert(!updateVrGameplayWelcomeNotice(welcome,true,false,false,true));
    assert(!welcome.visible);
    assert(!updateVrGameplayWelcomeNotice(welcome,true,false,false,true));
    assert(welcome.visible&&welcome.bothSticksReleased);
    assert(!updateVrGameplayWelcomeNotice(welcome,true,false,true,true));
    assert(!updateVrGameplayWelcomeNotice(welcome,true,true,false,true));
    assert(!updateVrGameplayWelcomeNotice(welcome,true,false,false,true));
    assert(!updateVrGameplayWelcomeNotice(welcome,true,true,true,false));
    assert(updateVrGameplayWelcomeNotice(welcome,true,true,true,true));
    assert(!updateVrGameplayWelcomeNotice(welcome,true,true,true,true));
    welcome.acknowledged=true; // persisted to Config/gameplay_welcome.ack
    assert(!updateVrGameplayWelcomeNotice(welcome,true,false,false,true));
    assert(!welcome.visible);
    VrGameplayWelcomeNotice preHeldWelcome{};
    for(int i=0;i<20;++i)
        assert(!updateVrGameplayWelcomeNotice(preHeldWelcome,true,true,true,true));
    assert(preHeldWelcome.visible&&!preHeldWelcome.bothSticksReleased);
    assert(!updateVrGameplayWelcomeNotice(preHeldWelcome,true,false,false,true));
    assert(updateVrGameplayWelcomeNotice(preHeldWelcome,true,true,true,true));
    VrGameplayWelcomeNotice interruptedWelcome{};
    for(int i=0;i<20;++i)
        updateVrGameplayWelcomeNotice(interruptedWelcome,true,false,false,true);
    assert(interruptedWelcome.visible);
    assert(!updateVrGameplayWelcomeNotice(interruptedWelcome,false,false,false,true));
    assert(!interruptedWelcome.visible&&!interruptedWelcome.acknowledged);
    for(int i=0;i<20;++i)
        updateVrGameplayWelcomeNotice(interruptedWelcome,true,false,false,true);
    assert(interruptedWelcome.visible); // unacknowledged => retry later
    // Main-menu and gameplay statuses must never automatically acknowledge
    // one another; only explicit successful file writes update each state.
    VrFirstTimeSetupNotice alreadySetup{};
    alreadySetup.acknowledged=true;
    VrGameplayWelcomeNotice freshWelcome{};
    for(int i=0;i<20;++i)
        updateVrGameplayWelcomeNotice(freshWelcome,true,false,false,true);
    assert(freshWelcome.visible&&!freshWelcome.acknowledged);
    assert(alreadySetup.acknowledged);
    // Seven fast taps reveal, seven further taps rehide, timing resets.
    VrMenuDeveloperReveal reveal{};
    assert(!reveal.visible);
    for(int i=0;i<6;++i)
        assert(!vrMenuDeveloperTap(reveal,1000+i*150));
    assert(!reveal.visible);
    assert(vrMenuDeveloperTap(reveal,1900));
    assert(reveal.visible&&reveal.taps==0);
    for(int i=0;i<6;++i)
        assert(!vrMenuDeveloperTap(reveal,3000+i*150));
    assert(vrMenuDeveloperTap(reveal,3900));
    assert(!reveal.visible);
    for(int i=0;i<6;++i)
        assert(!vrMenuDeveloperTap(reveal,6000+i*150));
    assert(!vrMenuDeveloperTap(reveal,12000)); // expired
    assert(!reveal.visible&&reveal.taps==1);
    for(int i=1;i<7;++i){
        const bool changed=vrMenuDeveloperTap(reveal,12000+i*150);
        assert(changed==(i==6));
    }
    assert(reveal.visible);
    // Both minigun recoil profiles must be reachable by controller navigation.
    assert(kVrRecoilMenuProfileCount==10);
    VrMenuNavigation recoilNav{};
    assert(vrMenuRecoilGamepadTarget(recoilNav,7)==207); // normal
    assert(vrMenuRecoilGamepadTarget(recoilNav,9)==209); // turret
    assert(vrMenuRecoilGamepadTarget(recoilNav,10)==210); // shared dynamic
    assert(vrMenuRecoilGamepadTarget(recoilNav,11)==-1);
    recoilNav.settingsColumn=true;
    recoilNav.actionIndex=5;
    assert(vrMenuRecoilGamepadTarget(recoilNav,9)==145); // reset turret only
    // The reconnect acknowledgement itself, not just its banner,
    // is gameplay-scoped. Other native screens must remain interactive.
    assert(vrControllerLiveGameplay(true,true,false,false,false,false,false));
    assert(!vrControllerLiveGameplay(false,true,false,false,false,false,false));
    assert(!vrControllerLiveGameplay(true,false,false,false,false,false,false));
    assert(!vrControllerLiveGameplay(true,true,true,false,false,false,false));
    assert(!vrControllerLiveGameplay(true,true,false,true,false,false,false));
    assert(!vrControllerLiveGameplay(true,true,false,false,true,false,false));
    assert(!vrControllerLiveGameplay(true,true,false,false,false,true,false));
    assert(!vrControllerLiveGameplay(true,true,false,false,false,false,true));
    assert(!vrControllerLiveGameplay(true,true,false,false,false,false,false,true));
    // Initial controller lock must NOT cover DOOM's main menu or loading.
    // The reconnect banner only appears in live gameplay, with safety
    // gating unaffected for every other context.
    assert(vrControllerReconnectBannerVisible(
        true,true,true,false,false,false,false,false));
    assert(!vrControllerReconnectBannerVisible(
        false,true,true,false,false,false,false,false)); // unlocked
    assert(!vrControllerReconnectBannerVisible(
        true,false,true,false,false,false,false,false)); // loading/stale gameplay
    assert(!vrControllerReconnectBannerVisible(
        true,true,false,false,false,false,false,false)); // main menu
    assert(!vrControllerReconnectBannerVisible(
        true,true,true,true,false,false,false,false)); // paused
    assert(!vrControllerReconnectBannerVisible(
        true,true,true,false,true,false,false,false)); // fullscreen/menu
    assert(!vrControllerReconnectBannerVisible(
        true,true,true,false,false,true,false,false)); // death screen
    assert(!vrControllerReconnectBannerVisible(
        true,true,true,false,false,false,true,false)); // cinematic
    assert(!vrControllerReconnectBannerVisible(
        true,true,true,false,false,false,false,true)); // flat/quad transition
    assert(!vrControllerReconnectBannerVisible(
        true,true,true,false,false,false,false,false,true)); // special menu
    // LEFT stick moves through rows only on a deliberate edge.
    vrMenuNavigateStick(nav,0.f,-.90f,5,selected,2);
    assert(selected==1&&nav.usingStick&&!nav.settingsColumn);
    vrMenuNavigateStick(nav,0.f,-.90f,5,selected,2);
    assert(selected==1); // no runaway scrolling from a held stick
    vrMenuNavigateStick(nav,0.f,0.f,5,selected,2);
    vrMenuNavigateStick(nav,0.f,-.90f,5,selected,2);
    assert(selected==2);
    // RIGHT and LEFT move between mod list and settings column.
    vrMenuNavigateStick(nav,0.f,0.f,5,selected,2);
    vrMenuNavigateStick(nav,.90f,0.f,5,selected,2);
    assert(nav.settingsColumn&&nav.actionIndex==0);
    assert(vrMenuModsGamepadTarget(nav,selected,5,2)==110);
    vrMenuNavigateStick(nav,0.f,-.90f,5,selected,2);
    assert(nav.actionIndex==1);
    assert(vrMenuModsGamepadTarget(nav,selected,5,2)==130);
    vrMenuNavigateStick(nav,0.f,0.f,5,selected,2);
    vrMenuNavigateStick(nav,-.90f,0.f,5,selected,2);
    assert(!nav.settingsColumn);
    assert(vrMenuModsGamepadTarget(nav,selected,5,2)==1002);
    // GRIP RELEASED -> live free controller, every frame.
    // GRIP HELD -> freeze the valid pose for 1cm fine tune only. If
    // no preview yet exists, a first held frame can capture the pose.
    assert(vrWeaponManualCalibrationLive(
        VrHandMenuMode::Calibrating,false,true));
    assert(vrWeaponManualCalibrationLive(
        VrHandMenuMode::Calibrating,false,false));
    assert(!vrWeaponManualCalibrationLive(
        VrHandMenuMode::Calibrating,true,true));
    assert(vrWeaponManualCalibrationLive(
        VrHandMenuMode::Calibrating,true,false));
    assert(!vrWeaponManualCalibrationLive(
        VrHandMenuMode::Menu,true,true));
    assert(!vrWeaponManualCalibrationLive(
        VrHandMenuMode::Closed,false,false));
    // Invert PRIMARY editing: NO Grip moves the native gun in 1 cm steps;
    // HOLD weapon-hand Grip freezes the gun and rotates the visual glove.
    // Only the equipped weapon hand supplies the rotation probe.
    const RightHandGripRotation id{0,0,0,1,true};
    const RightHandGripRotation yaw90{0,0.707106781f,0,0.707106781f,true};
    const RightHandGripRotation yaw180{0,1,0,0,true};
    const auto rotationClose=[](RightHandGripRotation a,
        RightHandGripRotation b){
        const float dot=a.x*b.x+a.y*b.y+a.z*b.z+a.w*b.w;
        return std::abs(std::abs(dot)-1.f)<.0002f;
    };
    VrPrimaryRotationEdit edit{};
    RightHandGripRotation wrist=id;
    RightHandGripOffset gunDraft{.02f,-.03f,-.04f};
    const auto originalDraft=gunDraft;
    // No Grip: regardless of controller orientation, glove angle is locked.
    wrist=vrCalibrationRelativeRotation(edit,false,yaw90,wrist);
    assert(rotationClose(wrist,id)&&!edit.referenceValid);
    gunDraft.y+=.01f; // free 1 cm Y nudge
    // Weapon hand Grip starts rotation: zero jump on the first frame,
    // even when the weapon hand was already turned on entry.
    wrist=vrCalibrationRelativeRotation(edit,true,yaw90,wrist);
    assert(rotationClose(wrist,id)&&edit.referenceValid);
    wrist=vrCalibrationRelativeRotation(edit,true,yaw180,wrist);
    assert(rotationClose(wrist,yaw90)); // 90 degree weapon-hand movement
    assert(gunDraft.x==originalDraft.x
        &&std::abs(gunDraft.y+.02f)<1e-6f
        &&gunDraft.z==originalDraft.z); // gun fine-tune untouched
    // Release locks the last visual glove orientation.
    wrist=vrCalibrationRelativeRotation(edit,false,id,wrist);
    assert(rotationClose(wrist,yaw90)&&!edit.referenceValid);
    // Regrip at unrelated orientation: again NO glove jump.
    wrist=vrCalibrationRelativeRotation(edit,true,id,wrist);
    assert(rotationClose(wrist,yaw90)&&edit.referenceValid);
    wrist=vrCalibrationRelativeRotation(edit,true,yaw90,wrist);
    assert(rotationClose(wrist,yaw180));
    // Edit cancellation leaves the saved profile unchanged.
    const auto storedGun=originalDraft;
    const auto storedWrist=id;
    edit={};
    assert(!edit.referenceValid&&rotationClose(storedWrist,id));
    assert(std::abs(storedGun.x-.02f)<1e-6f);

    // If the Quest runtime provides a valid but non-TRACKED GRIP pose,
    // use a tracked AIM laser pose. If neither is tracked, POSITION_VALID
    // still suffices; don't leave the glove stuck at the old saved pose.
    struct FakeTrackedPose {bool valid,positionTracked;int id;};
    const FakeTrackedPose trackedGrip{true,true,1};
    const FakeTrackedPose trackedAim{true,true,2};
    const FakeTrackedPose validPredictedGrip{true,false,3};
    const FakeTrackedPose validPredictedAim{true,false,4};
    const FakeTrackedPose unavailable{false,false,5};
    assert(&vrCalibrationFreeControllerPose(trackedGrip,trackedAim)==&trackedGrip);
    assert(&vrCalibrationFreeControllerPose(validPredictedGrip,trackedAim)==&trackedAim);
    assert(&vrCalibrationFreeControllerPose(validPredictedGrip,validPredictedAim)
        ==&validPredictedGrip);
    assert(&vrCalibrationFreeControllerPose(unavailable,validPredictedAim)
        ==&validPredictedAim);
    assert(&vrCalibrationFreeControllerPose(unavailable,trackedAim)==&trackedAim);
    // SUPPORT CALIBRATION is NOT the primary glove's free-controller
    // rotation probe. It must use the actual support-hand GRIP position,
    // even if an AIM laser is tracked farther forward.
    const auto& supportTracked=kharvox::vrSupportCalibrationPhysicalGrip(
        trackedGrip,trackedAim);
    assert(&supportTracked==&trackedGrip);
    const auto& supportPredicted=kharvox::vrSupportCalibrationPhysicalGrip(
        validPredictedGrip,trackedAim);
    assert(&supportPredicted==&validPredictedGrip);
    // No AIM fallback: do not save wrong support position if GRIP is lost.
    const auto& supportUnavailable=kharvox::vrSupportCalibrationPhysicalGrip(
        unavailable,trackedAim);
    assert(&supportUnavailable==&unavailable);
    assert(!supportUnavailable.valid);
    // Quest can report a valid GRIP rotation with an unchanged/predicted
    // position while its tracked AIM laser translates freely.
    struct FakeController {
        bool valid,positionTracked;
        RightHandGripOffset position;
        RightHandGripRotation orientation;
    };
    const FakeController staticGrip{
        true,true,{0.f,0.f,0.f},{0.f,.4f,0.f,.9165f,true}};
    const FakeController movingAim{
        true,true,{.12f,-.20f,-.32f},{0.f,0.f,0.f,1.f,true}};
    const auto live=vrCalibrationLivePose(staticGrip,movingAim);
    assert(std::abs(live.position.x-.12f)<.00001f);
    assert(std::abs(live.position.y+.20f)<.00001f);
    assert(std::abs(live.position.z+.32f)<.00001f);
    assert(std::abs(live.orientation.y-.4f)<.00001f);
    // Primary Grip: quick flick is a single 1cm step. Holding the same
    // direction then auto-repeats in 1cm increments after 340ms, every
    // 175ms. The speed is clock-driven, not headset frame-rate-driven.
    VrWeaponFineTuneEdges fine{};
    std::uint64_t fineClock=1000;
    auto nudge=[&](bool allowed,bool grip,float lx,float ly,float ry,
                   std::uint64_t ms=16){
        fineClock+=ms;
        return vrWeaponFineTuneCentimeterStep(
            fine,allowed,grip,lx,ly,ry,fineClock);
    };
    const auto zeroNudge=[&](RightHandGripOffset p){
        assert(p.x==0.f&&p.y==0.f&&p.z==0.f);
    };
    const auto sameCm=[](float got,float expected){
        return std::abs(got-expected)<1e-6f;
    };
    zeroNudge(nudge(true,false,0,0,0)); // Default fine-tune enters, arm neutral
    auto move=nudge(true,false,1.f,0.f,0.f);
    assert(sameCm(move.x,.01f)&&move.y==0&&move.z==0);
    const auto firstAt=fineClock;
    zeroNudge(nudge(true,false,1.f,0.f,0.f,
        kVrWeaponFineTuneRepeatDelayMs-1)); // not even 1ms early
    assert(fineClock==firstAt+kVrWeaponFineTuneRepeatDelayMs-1);
    move=nudge(true,false,1.f,0.f,0.f,1);
    assert(sameCm(move.x,.01f)); // first held auto-repeat
    zeroNudge(nudge(true,false,1.f,0.f,0.f,
        kVrWeaponFineTuneRepeatIntervalMs-1));
    move=nudge(true,false,1.f,0.f,0.f,1);
    assert(sameCm(move.x,.01f)); // steady 175ms repeat
    // A frame stall must emit only ONE step, never a 10cm catch-up jump.
    move=nudge(true,false,1.f,0.f,0.f,1000);
    assert(sameCm(move.x,.01f)&&move.y==0&&move.z==0);
    // Recenter resets the countdown and permits a new instant 1cm flick.
    zeroNudge(nudge(true,false,0,0,0));
    move=nudge(true,false,-1.f,0.f,0.f);
    assert(sameCm(move.x,-.01f)&&move.y==0&&move.z==0);
    zeroNudge(nudge(true,false,0,0,0));
    move=nudge(true,false,0.f,1.f,0.f); // left up -> local -Z
    assert(move.x==0&&move.y==0&&sameCm(move.z,-.01f));
    zeroNudge(nudge(true,false,0,0,0));
    move=nudge(true,false,0.f,-1.f,0.f); // left down -> +Z
    assert(move.x==0&&move.y==0&&sameCm(move.z,.01f));
    zeroNudge(nudge(true,false,0,0,0));
    move=nudge(true,false,0,0,1.f); // right up -> +Y
    assert(move.x==0&&sameCm(move.y,.01f)&&move.z==0);
    const auto rightAt=fineClock;
    zeroNudge(nudge(true,false,0,0,1.f,
        kVrWeaponFineTuneRepeatDelayMs-1));
    move=nudge(true,false,0,0,1.f,1);
    assert(sameCm(move.y,.01f));
    assert(fineClock==rightAt+kVrWeaponFineTuneRepeatDelayMs);
    zeroNudge(nudge(true,false,0,0,0));
    move=nudge(true,false,0,0,-1.f); // right down -> -Y
    assert(move.x==0&&sameCm(move.y,-.01f)&&move.z==0);
    zeroNudge(nudge(true,false,0,0,0));
    move=nudge(true,false,.9f,.8f,0); // diagonal: one axis
    assert(sameCm(move.x,.01f)&&move.y==0&&move.z==0);
    // No automatic direction reversal (X, Z or sign) while stick
    // remains displaced: users must recenter between opposing nudges.
    zeroNudge(nudge(true,false,-1.f,0,0,2000));
    zeroNudge(nudge(true,false,0,-1.f,0,2000));
    zeroNudge(nudge(true,false,0,0,0));
    move=nudge(true,false,0,-1.f,0);
    assert(sameCm(move.z,.01f));

    // Grip HELD switches to rotation mode and cancels repeat timers.
    // Release Grip while a stick is held: still inert until recentered.
    zeroNudge(nudge(true,true,0,0,-1.f,2000));
    zeroNudge(nudge(true,false,1.f,0,0)); // released at entry: inert
    zeroNudge(nudge(true,false,1.f,0,0,2000)); // stays inert until centered
    zeroNudge(nudge(true,false,0,0,0));
    move=nudge(true,false,1.f,0,0);
    assert(sameCm(move.x,.01f));
    zeroNudge(nudge(false,false,-1.f,0,0,2000)); // menu/support: no motion
    zeroNudge(nudge(true,false,-1.f,0,0)); // no inherited repeat timer
    zeroNudge(nudge(true,false,0,0,0));
    move=nudge(true,false,0,0,-1.f);
    assert(sameCm(move.y,-.01f));

    // Two sticks retain independent repeat clocks; left X and right Y can
    // both step once in the same frame without rotating the weapon.
    VrWeaponFineTuneEdges independent{};
    const auto twoSticks=[&](std::uint64_t at,float lx,float ly,float ry){
        return vrWeaponFineTuneCentimeterStep(
            independent,true,false,lx,ly,ry,at);
    };
    zeroNudge(twoSticks(100,0,0,0));
    move=twoSticks(110,1.f,0,1.f);
    assert(sameCm(move.x,.01f)&&sameCm(move.y,.01f)&&move.z==0);
    move=twoSticks(110+kVrWeaponFineTuneRepeatDelayMs,1.f,0,1.f);
    assert(sameCm(move.x,.01f)&&sameCm(move.y,.01f)&&move.z==0);

    // A selects exactly once per press, B same, no latent double events.
    bool a=false,b=false;
    assert(vrMenuButtonEdge(true,a));
    assert(!vrMenuButtonEdge(true,a));
    assert(!vrMenuButtonEdge(false,a));
    assert(vrMenuButtonEdge(true,a));
    assert(vrMenuButtonEdge(true,b));
    assert(!vrMenuButtonEdge(true,b));
    // Weapon calibration layout maps to the existing pointer actions.
    nav.settingsColumn=true;nav.actionIndex=0;
    assert(vrMenuGripGamepadTarget(nav,3,false)==20);
    nav.actionIndex=1;
    assert(vrMenuGripGamepadTarget(nav,3,false)==21);
    nav.actionIndex=2;
    assert(vrMenuGripGamepadTarget(nav,3,false)==30);
    nav.actionIndex=3;
    assert(vrMenuGripGamepadTarget(nav,3,true)==30);
    assert(vrMenuGripGamepadTarget(nav,6,true)==30);
    nav.actionIndex=2;
    assert(vrMenuGripGamepadTarget(nav,6,true)==32);
    nav.actionIndex=4;
    assert(vrMenuGripGamepadTarget(nav,6,false)==95);
    assert(vrMenuGripGamepadTarget(nav,6,true)==31);
    nav.actionIndex=5;
    assert(vrMenuGripGamepadTarget(nav,6,true)==95);
    assert(vrMenuGripGamepadTarget(nav,6,false)==-1);
    nav.actionIndex=6;
    assert(vrMenuGripGamepadTarget(nav,6,true)==-1);
    // Copy RIGHT -> LEFT has been removed: all left-side presets are
    // calibrated independently. The old button code 33 is unreachable.
    for(int i=0;i<7;++i){
        nav.actionIndex=i;
        assert(vrMenuGripGamepadTarget(nav,6,true)!=33);
        assert(vrMenuGripGamepadTarget(nav,6,false)!=33);
    }
    nav.settingsColumn=false;
    assert(vrMenuGripGamepadTarget(nav,8,true)==8);
    // Mods page starts in overview mode. Scrolling through the six
    // configurable entries must NOT enter settings or activate a toggle.
    VrMenuNavigation browse{};
    browse.settingsColumn=false;
    int browsingRow=0;
    vrMenuNavigateStick(browse,0.f,-.9f,6,browsingRow,5);
    assert(browsingRow==1&&!browse.settingsColumn);
    assert(vrMenuModsGamepadTarget(browse,browsingRow,6,5,false)==1001);
    vrMenuNavigateStick(browse,0.f,0.f,6,browsingRow,5); // re-arm
    vrMenuNavigateStick(browse,0.f,-.9f,6,browsingRow,5);
    assert(browsingRow==2&&!browse.settingsColumn);
    vrMenuNavigateStick(browse,0.f,0.f,6,browsingRow,5);
    for(int i=0;i<4;++i){
        vrMenuNavigateStick(browse,0.f,-.9f,6,browsingRow,5);
        vrMenuNavigateStick(browse,0.f,0.f,6,browsingRow,5);
    }
    assert(browsingRow==0&&!browse.settingsColumn);
    // Only explicit A / CONFIGURE moves focus into live options.
    browse.settingsColumn=true;
    browse.actionIndex=0;
    assert(vrMenuModsGamepadTarget(browse,0,6,5,false)==130);
    browse.settingsColumn=false; // B: back to overview
    assert(vrMenuModsGamepadTarget(browse,0,6,5,false)==1000);

    // Core mods have settings but no enable checkbox. First action is
    // OPEN CALIBRATION / SET HIP / SLOWER etc, not toggle.
    nav.settingsColumn=true;nav.actionIndex=0;
    assert(vrMenuModsGamepadTarget(nav,0,7,5,false)==130);
    nav.actionIndex=1;
    assert(vrMenuModsGamepadTarget(nav,0,7,5,false)==131);
    // Only the Physical Crouch and PAINSAW rows preserve toggle first.
    nav.actionIndex=0;
    assert(vrMenuModsGamepadTarget(nav,2,7,4,true)==110);
    assert(vrMenuModsGamepadTarget(nav,5,7,1,true)==110);
    nav.actionIndex=1;
    assert(vrMenuModsGamepadTarget(nav,2,7,4,true)==130);
    // Legacy wheel remap is retired, but Gunslinger's gesture wheel frees
    // right-stick DOWN for crouch when Physical Crouch is OFF.
    assert(manualStickCrouchAllowed(false,true,false));
    assert(manualStickCrouchAllowed(false,false,true));
    assert(!manualStickCrouchAllowed(true,true,true));
    assert(!manualStickCrouchAllowed(false,false,false));
    assert(manualStickCrouchDirection(0.f,-1.f));
    assert(manualStickCrouchDirection(.24f,-.95f));
    assert(!manualStickCrouchDirection(.7f,-1.f)); // turning diagonally
    assert(!manualStickCrouchDirection(0.f,-.5f)); // no accidental crouch
    assert(!manualStickCrouchDirection(0.f,.9f)); // chainsaw UP unchanged
    // No Weapon Wheel cannot resurrect the retired stick-down wheel route.
    assert(!vrScopedWeaponSelectDown(true,true,true,false,true));
    assert(!vrScopedWeaponSelectDown(true,false,true,false,true));
    assert(vrScopedWeaponSelectDown(true,false,true,true,true));
    assert(vrScopedWeaponSelectDown(true,false,false,false,true));
    assert(!vrScopedWeaponSelectDown(false,false,true,true,true));
    // Native 90 ms edge pulse, both on crouch and on stand; never leak a
    // manual pulse into Physical Crouch, menus or the old remap input path.
    assert(manualStickCrouchPulseActive(true,false,false,100,100+90000000));
    assert(!manualStickCrouchPulseActive(true,false,false,100+90000000,100+90000000));
    assert(!manualStickCrouchPulseActive(false,false,false,100,100+90000000));
    assert(!manualStickCrouchPulseActive(true,true,false,100,100+90000000));
    assert(!manualStickCrouchPulseActive(true,false,true,100,100+90000000));
    return 0;
}
