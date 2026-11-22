// All configurable VR mods are applied from their current in-process values,
// NOT by restarting the game or rewriting the launcher JSON. Desktop defaults
// remain authoritative until a user explicitly changes a VR checkbox.
struct VrLiveMenuEntry {
    const char* title;
    kharvox::VrLiveMod mod{kharvox::VrLiveMod::Other};
    bool enabled{};
    kharvox::VrMenuDevOption developer{kharvox::VrMenuDevOption::None};
};
std::vector<VrLiveMenuEntry> activeVrMenuEntries(){
    using kharvox::VrLiveMod;
    std::vector<VrLiveMenuEntry> rows;
    // DEV is a dedicated second list, not a set of miscellaneous rows in
    // the public menu. Reuse the same selection, laser and navigation system.
    if(s.vrHandMenu.page==kharvox::VrMenuPage::Dev){
        using D=kharvox::VrMenuDevOption;
        rows.push_back({"Weapon Stability",VrLiveMod::WeaponRecoil,
            kharvox::anyWeaponRecoilEnabled(s.recoilSettings),D::WeaponStability});
        rows.push_back({"Weapon Alignment Debug",VrLiveMod::Other,
            s.vrHandMenu.devWeaponAlignment,D::WeaponAlignment});
        rows.push_back({"Hand Focus",VrLiveMod::HandFocus,
            s.customMods.handFocusedRs,D::HandFocus});
        rows.push_back({"Controller Input Monitor",VrLiveMod::Other,
            s.vrHandMenu.devControllerInputs,D::ControllerMonitor});
        rows.push_back({"Stats / Reset",VrLiveMod::Stats,
            true,D::StatsReset});
        rows.push_back({"Level Records / Reset",VrLiveMod::Stats,
            true,D::LevelRecordsReset});
#if KHARVOX_EXPERIMENTAL_WORLD_GRAB
        rows.push_back({"Experimental Features",VrLiveMod::Other,
            s.vrHandMenu.objectDiscovery.enabled,D::Experimental});
#endif
        return rows;
    }
    const auto add=[&](const char* title,VrLiveMod id,bool on,bool showEvenIfOff=false){
        if(on||showEvenIfOff)rows.push_back({title,id,on});
    };
    // Three core features stay on; only Crouch/PAINSAW can be toggled.
    // Keep Physical Glory Kills settings available without a live checkbox.
    add("Weapon Grip Calibration",VrLiveMod::WeaponGrip,
        s.customMods.offHandGripCapture,true);
    add("Gunslinger",VrLiveMod::Gunslinger,
        s.customMods.dynamicShoulderHolster||s.customMods.behindHeadWeaponWheel,true);
    add("KILLTIME",VrLiveMod::Killtime,
        s.vrLiveKilltimeEnabled,true);
    // The heading gesture reveals one DEV folder, never loose DEV mods.
    add("Physical Glory Kills",VrLiveMod::PhysicalGloryKills,
        s.physicalGlorykillEnabled,true);
    add("Smooth Turn Speed",VrLiveMod::TurnSpeed,true,true);
    // Group the two live ON/OFF features together at the bottom.
    add("Physical Crouch",VrLiveMod::PhysicalCrouch,
        s.customMods.physicalCrouch,true);
    add("PAINSAW",VrLiveMod::Painsaw,
        s.customMods.chainsawTriggerThrottle
            ||s.customMods.motionChainsawKillSpeed,true);
    add("Stats",VrLiveMod::Stats,true,true);
    if(s.vrHandMenu.devReveal.visible)
        rows.push_back({"DEV  >",VrLiveMod::Other,true,
            kharvox::VrMenuDevOption::Folder});
    return rows;
}
// This is the PLAYABILITY overview, not a calibration tutorial. It appears
// immediately as the player scrolls the mods list, before pressing A to
// open its controls. Keep the text short enough for the in-headset panel.
struct VrLiveModOverview {
    std::array<const char*,6> lines{};
};
VrLiveModOverview vrLiveModOverview(kharvox::VrLiveMod mod){
    using kharvox::VrLiveMod;
    switch(mod){
        case VrLiveMod::WeaponGrip:return {{{
            "Calibrate guns to your VR hand position.",
            "Equip the gun in the hand to adjust.",
            "Tune primary and support-hand grips.",
            "Left- and right-hand profiles",
            "are saved independently.",
            ""}}};
        case VrLiveMod::Gunslinger:return {{{
            "Open the wheel behind either side of head.",
            "Select by moving your ARM, not your wrist.",
            "Leave with Grip held to equip.",
            "Leave without Grip to cancel.",
            "Use hip holsters for instant quick swaps.",
            "Calibrate your wheel and hip zones."}}};
        case VrLiveMod::Killtime:return {{{
            "Control cinematic Glory Kill speed.",
            "Lower Hz means faster executions.",
            "Higher Hz means slower executions.",
            "Normal movement speed is unchanged.",
            "",
            ""}}};
        case VrLiveMod::TurnSpeed:return {{{
            "Tune SMOOTH right-stick turning in VR.",
            "Adjust in 25 degree/second steps.",
            "Changes apply instantly and are saved.",
            "Snap turn angle stays unchanged.",
            "Reset inherits the desktop launch speed.",
            ""}}};
        case VrLiveMod::WeaponRecoil:return {{{
            "One-handed fire adds physical recoil.",
            "Sustained shots cause muzzle climb.",
            "Lower your controller to stay on aim.",
            "Using both hands stabilises the weapon.",
            "Adjust recoil strength and recovery",
            "for each weapon."}}};
        case VrLiveMod::PhysicalGloryKills:return {{{
            "Perform Glory Kills with real punches.",
            "Strike a staggered demon in VR.",
            "Hand velocity determines the impact.",
            "Choose left, right or both hands.",
            "Adjust punch sensitivity to suit you.",
            ""}}};
        case VrLiveMod::PhysicalCrouch:return {{{
            "Crouch by physically lowering your head.",
            "Stand to rise back up in-game.",
            "Press A to toggle crouch at any time.",
            "A works even with Physical Crouch OFF.",
            "Calibrate standing and crouching height.",
            "B remains your jump button."}}};
        case VrLiveMod::Painsaw:return {{{
            "Take your time... or RIP & TEAR.",
            "Enjoy a slow, brutal chainsaw kill,",
            "or rev it up when you're in a hurry.",
            "Control speed with each trigger pull.",
            "Disabling PAINSAW restores native",
            "chainsaw execution."}}};
        case VrLiveMod::Stats:return {{{
            "Death counters, timers and level records.",
            "Each HUD item has its own ON/OFF switch.",
            "Place items using the fine HUD grid.",
            "Overlapping positions replace old items.",
            "Counters keep tracking when hidden.",
            "Select a mission in LEVEL RECORDS for PB."}}};
        default:return {{{"Select a mod for information."}}};
    }
}
VrLiveModOverview vrDevOptionOverview(kharvox::VrMenuDevOption option){
    using D=kharvox::VrMenuDevOption;
    switch(option){
        case D::Folder:return {{{"Open the developer tools directly.",
            "Select DEV to enter its own menu.",
            "No separate Configure screen.",
            "Normal mod settings remain unchanged.","",""}}};
        case D::WeaponStability:
            return vrLiveModOverview(kharvox::VrLiveMod::WeaponRecoil);
        case D::WeaponAlignment:return {{{"Display live gun and hand anchor data.",
            "Shown ONLY during Weapon Grip",
            "Calibration's live capture screen.",
            "No positions are changed.","",""}}};
        case D::HandFocus:return {{{"Switch world interaction focus between",
            "your tracked weapon hand and head.",
            "Native scripted actions stay native.",
            "Applies live for this game session.","",""}}};
        case D::ControllerMonitor:return {{{"Show live hand-controller inputs.",
            "Visible ONLY inside Gunslinger",
            "hip or wheel calibration.",
            "Does not change button bindings.","",""}}};
        case D::LevelRecordsReset:return {{{
            "Reset your saved level-time records.",
            "Current level or all levels.",
            "Requires a second confirmation press.",
            "Does NOT reset normal statistics.",
            "Records and HUD preferences are separate.",
            ""}}};
        case D::StatsReset:return {{{
            "Reset campaign and session deaths",
            "or campaign and session timers.",
            "Press the same reset button twice.",
            "No Stats display settings are changed.",
            "Counters keep tracking in background.",
            ""}}};
        case D::Experimental:return {{{
            "Enable read-only native object observation.",
            "Scan radius: 1-15 m (default 5 m).",
            "Grab reach: 1-5 m (default 2 m).",
            "Change sliders with pointer or A button.",
            "Nearby rings/calibration still being developed.",
            "Grip cannot move stationary props."}}};
        default:return {{{"Select a developer tool.","","","","",""}}};
    }
}
// Show the runtime's effective Glory Kill target, not the provenance of
// the setting. A saved in-headset override wins, then the launcher profile.
// Without an explicit Hz, a native full-speed bypass has NO fixed target;
// display that accurately rather than inventing a refresh rate.
std::string vrLiveGloryKillSpeedText(){
    // Native full-speed bypass takes precedence over any displayable Hz
    // profile; the game does not apply that target in this execution path.
    int legacySpeedLevel=10;
    char legacyText[16]{};
    const DWORD legacyLength=GetEnvironmentVariableA(
        "KHARVOX_GLORY_KILL_SLOWMO_LEVEL",legacyText,
        sizeof(legacyText));
    if(legacyLength>0&&legacyLength<sizeof(legacyText)){
        char* legacyEnd{};
        const long value=std::strtol(legacyText,&legacyEnd,10);
        if(legacyEnd!=legacyText&&*legacyEnd=='\0'
            &&value>=0&&value<=10)
            legacySpeedLevel=static_cast<int>(value);
    }
    if(kharvox::nativeFastKillSyncRequested(
        s.customMods.fastGloryKills,s.customMods.fastChainsawKills,
        legacySpeedLevel))
        return "FULL SPEED (NATIVE)";
    const int explicitHz=actionSpeedTargets().gloryKill;
    if(explicitHz>0)return std::to_string(explicitHz)+" Hz";
    char text[16]{};
    const DWORD length=GetEnvironmentVariableA(
        "KHARVOX_MOD_CINEMATIC_KILL_SPEED",text,sizeof(text));
    int cinematicLevel=1;
    if(length>0&&length<sizeof(text)){
        char* end{};
        const long value=std::strtol(text,&end,10);
        if(end!=text&&*end=='\0'&&value>=1&&value<=5)
            cinematicLevel=static_cast<int>(value);
    }
    const int headsetHz=kharvox::targetGameHzForDisplayPeriod(
        s.fastestDisplayPeriod);
    if(headsetHz>0)return std::to_string(
        kharvox::cinematicKillTargetTickHz(cinematicLevel,headsetHz))
        +" Hz TARGET";
    if(cinematicLevel>=2)
        return std::to_string(kharvox::cinematicKillTargetTickHz(
            cinematicLevel,120))+" Hz TARGET";
    return "NATIVE ADAPTIVE";
}
void applyVrLiveModRuntimeState(){
    using kharvox::VrLiveMod;
    const auto enabled=[&](VrLiveMod mod,bool desktop){
        return kharvox::vrLiveEffectiveEnabled(s.vrLiveSettings,mod,desktop);
    };
    const bool grip=enabled(VrLiveMod::WeaponGrip,s.vrDesktopMods.offHandGripCapture);
    if(grip!=s.customMods.offHandGripCapture){
        s.primaryVisualLocked=false;
        s.supportVisualLocked=false;
        s.twoHandLatched=false;
    }
    s.customMods.offHandGripCapture=grip;
    if(grip&&s.weapon6Dof)s.twoHandEnabled=true;
    const bool gunslinger=enabled(VrLiveMod::Gunslinger,
        s.vrDesktopMods.dynamicShoulderHolster
        ||s.vrDesktopMods.behindHeadWeaponWheel);
    if(!gunslinger&&(s.behindHeadWheelActive||s.dynamicShoulderHolstered)){
        s.behindHeadWheelActive=false;
        s.behindHeadWheelSide=-1;
        s.dynamicShoulderHolstered=false;
        s.wheelGripLatched=false;
        s.wheelCancelDrain.reset();
        kharvox::resetQuickHipGestures(s.quickHolsters);
        KharvoxWeaponSetHolstered(false);
    }
    s.customMods.dynamicShoulderHolster=gunslinger;
    s.customMods.behindHeadWeaponWheel=gunslinger;
    s.customMods.leftWristWeaponWheel=gunslinger;
    s.customMods.behindHeadWheelHandSelection=gunslinger;
    const bool physicalCrouch=enabled(VrLiveMod::PhysicalCrouch,
        s.vrDesktopMods.physicalCrouch);
    if(s.customMods.physicalCrouch&&!physicalCrouch){
        if(s.physicalCrouchActive)s.vrCrouchReleasePending=true;
        s.physicalCrouchActive=false;
        // Disabling HMD crouch must not cancel a separate A-button
        // manual crouch that the player intentionally toggled.
        KharvoxCameraSetCrouchState(s.crouchToggleActive);
    }
    s.customMods.physicalCrouch=physicalCrouch;
    // PAINSAW includes legacy motion-only mode as well as current trigger
    // throttle. OFF disables both, otherwise an existing desktop setup
    // retains its original implementation. New live ON defaults to the
    // modern trigger-throttle mode without enabling two speed controllers.
    const auto painsaw=kharvox::vrLivePainsawModes(
        s.vrLiveSettings,s.vrDesktopMods.chainsawTriggerThrottle,
        s.vrDesktopMods.motionChainsawKillSpeed);
    s.customMods.chainsawTriggerThrottle=painsaw.triggerThrottle;
    s.customMods.motionChainsawKillSpeed=painsaw.legacyMotion;
    s.vrLiveKilltimeEnabled=enabled(VrLiveMod::Killtime,s.vrDesktopKilltime);
    const bool physicalGK=enabled(VrLiveMod::PhysicalGloryKills,
        s.vrDesktopPhysicalGk);
    if(physicalGK!=s.physicalGlorykillEnabled){
        s.physicalPunchArmed={false,false};
        s.physicalPunchPulseUntil=0;
    }
    s.physicalGlorykillEnabled=physicalGK;
    s.physicalGlorykillSpeed=s.vrLiveSettings.punchSpeedMetersPerSec;
    s.physicalGlorykillHands=static_cast<PhysicalGlorykillHands>(
        s.vrLiveSettings.punchHands);
    s.smoothTurnDegreesPerSecond=s.vrLiveSettings.smoothTurnSpeedDegreesPerSec>0.f
        ?s.vrLiveSettings.smoothTurnSpeedDegreesPerSec
        :s.vrDesktopSmoothTurnSpeed;
}
bool saveVrRecoilSettings(){
    if(!kharvox::saveWeaponRecoilSettings(
        kharvox::runtimePathA("weapon_recoil_saved.cfg"),s.recoilSettings)){
        s.vrHandMenu.message="Recoil save failed. Existing profile unchanged.";
        return false;
    }
    s.recoilState={};
    return true;
}
bool handleVrRecoilCalibrationAction(int action){
    if(action<140||action>145)return false;
    auto& m=s.vrHandMenu;
    const int index=std::clamp(m.selectedRecoilWeapon,0,
        kharvox::kVrRecoilMenuEntryCount-1);
    auto old=s.recoilSettings;
    if(index==kharvox::kVrRecoilMenuProfileCount){
        // One shared formula for all finite-ammo guns except Gauss.
        // Buttons are reused by the extra GLOBAL DYNAMICS list row.
        switch(action){
            case 140:s.recoilSettings.dynamicEnabled=
                !s.recoilSettings.dynamicEnabled;break;
            case 141:s.recoilSettings.dynamicBuildUpPerShot=std::max(.1f,
                s.recoilSettings.dynamicBuildUpPerShot-.05f);break;
            case 142:s.recoilSettings.dynamicBuildUpPerShot=std::min(.5f,
                s.recoilSettings.dynamicBuildUpPerShot+.05f);break;
            case 143:s.recoilSettings.dynamicMaxMultiplier=std::max(2.f,
                s.recoilSettings.dynamicMaxMultiplier-.25f);break;
            case 144:s.recoilSettings.dynamicMaxMultiplier=std::min(4.f,
                s.recoilSettings.dynamicMaxMultiplier+.25f);break;
            case 145:s.recoilSettings.dynamicEnabled=false;
                s.recoilSettings.dynamicBuildUpPerShot=.20f;
                s.recoilSettings.dynamicMaxMultiplier=3.f;break;
        }
        if(!saveVrRecoilSettings()){
            s.recoilSettings=old;return true;
        }
        m.message=std::string("Dynamic recoil ")+
            (s.recoilSettings.dynamicEnabled?"ON":"OFF")+
            " | Gauss remains independently tuned.";
        return true;
    }
    const std::size_t at=static_cast<std::size_t>(index);
    switch(action){
        case 140:s.recoilSettings.enabled[at]=!s.recoilSettings.enabled[at];break;
        case 141:s.recoilSettings.strength[at]=std::max(.25f,
            s.recoilSettings.strength[at]-.1f);break;
        case 142:s.recoilSettings.strength[at]=std::min(9.f,
            s.recoilSettings.strength[at]+.1f);break;
        case 143:s.recoilSettings.recovery[at]=std::max(.25f,
            s.recoilSettings.recovery[at]-.1f);break;
        case 144:s.recoilSettings.recovery[at]=std::min(9.f,
            s.recoilSettings.recovery[at]+.1f);break;
        case 145:
            s.recoilSettings.strength[at]=kharvox::kWeaponRecoilDefaultStrength[at];
            s.recoilSettings.recovery[at]=kharvox::kWeaponRecoilDefaultRecovery[at];break;
    }
    if(!saveVrRecoilSettings()){
        s.recoilSettings=old;return true;
    }
    m.message=std::string(kharvox::kWeaponRecoilKinds[at])+" recoil "
        +(s.recoilSettings.enabled[at]?"ON":"OFF")
        +" - saved instantly. Close menu to test.";
    return true;
}
bool saveVrLiveSettings(){
    const auto path=kharvox::runtimePathA("vr_live_mods_saved.cfg");
    if(!kharvox::vrLiveSave(path,s.vrLiveSettings)){
        s.vrHandMenu.message="Save failed: UserData/Settings/vr_live_mods_saved.cfg";
        return false;
    }
    applyVrLiveModRuntimeState();
    return true;
}
void openVrLiveCapture(kharvox::VrLiveCapture capture){
    auto& m=s.vrHandMenu;
    m.liveCapture=capture;
    m.previewValid=false;
    m.gunslingerZoneDraftValid=kharvox::vrLiveIsGunslingerCapture(capture);
    m.gunslingerZoneDraft=m.gunslingerZoneDraftValid
        ?kharvox::vrLiveZoneCenter(s.vrLiveSettings,capture):kharvox::VrLivePoint{};
    m.gunslingerZoneMoving=false;
    m.mode=kharvox::VrHandMenuMode::Calibrating;
    const bool crouch=capture==kharvox::VrLiveCapture::CrouchStanding
        ||capture==kharvox::VrLiveCapture::CrouchLowered;
    m.message=crouch?"Move your HEAD to target height, then RIGHT TRIGGER."
        :"Hold the matching GRIP to drag the zone. Release to freeze; RIGHT TRIGGER saves.";
    log("[VR-MODS] live calibration armed; fire remains blocked");
}
// A Grip drag updates ONLY the unsaved visual zone. Releasing Grip keeps the
// last valid draft position; reopening placement starts at the saved location.
void updateVrLiveZoneDraft(){
    using kharvox::VrLiveCapture;
    auto& m=s.vrHandMenu;
    const auto target=m.liveCapture;
    m.gunslingerZoneMoving=false;
    if(!kharvox::vrLiveIsGunslingerCapture(target))return;
    const bool right=target==VrLiveCapture::HipRight
        ||target==VrLiveCapture::WheelRight;
    // Live OpenXR action state is read here, independently of the local
    // gameplay-frame input helper; this also works during menu calibration.
    XrActionStateGetInfo gripInfo{XR_TYPE_ACTION_STATE_GET_INFO};
    gripInfo.action=right?s.rightWeaponGrip:s.leftWeaponGrip;
    XrActionStateFloat gripState{XR_TYPE_ACTION_STATE_FLOAT};
    const bool gripHeld=XR_SUCCEEDED(
        s.getActionStateFloat(s.session,&gripInfo,&gripState))
        &&gripState.isActive&&gripState.currentState>.55f;
    m.gunslingerZoneMoving=gripHeld;
    if(!gripHeld||!s.trackingHeadPositionValid||!s.headZeroPositionValid)return;
    // No relative drag offset: the very first held tracked frame snaps the
    // marker centre to the controller grip pose, then follows it 1:1.
    const auto& controller=right?s.rightGripController:s.leftGripController;
    if(!controller.valid||!controller.positionTracked)return;
    const auto relative=controllerRelativeToBodyTrackingOrigin(controller).position;
    const auto hd=positionRelativeToBodyTrackingOrigin(s.trackingHeadPosition);
    const kharvox::VrLivePoint point{right?relative.x-hd.x:hd.x-relative.x,
        relative.y-hd.y,relative.z-hd.z};
    if(kharvox::vrLiveUpdateZoneDraft(m.gunslingerZoneDraft,
        gripHeld,true,point,target))m.gunslingerZoneDraftValid=true;
}
void captureVrLivePosition(){
    using kharvox::VrLiveCapture;
    auto& m=s.vrHandMenu;
    const auto capture=m.liveCapture;
    if(capture==VrLiveCapture::None)return;
    const bool head=s.trackingHeadPositionValid&&s.headZeroPositionValid;
    if(!head){m.message="Head tracking unavailable. Position not saved.";return;}
    auto before=s.vrLiveSettings;
    kharvox::VrLivePoint p{};
    const bool crouch=capture==VrLiveCapture::CrouchStanding
        ||capture==VrLiveCapture::CrouchLowered;
    if(!crouch){
        if(!m.gunslingerZoneDraftValid){
            m.message="No valid zone preview. Nothing saved.";return;
        }
        p=m.gunslingerZoneDraft;
    }
    const float headDelta=s.trackingHeadPosition.y-s.headZeroPosition.y;
    if(!kharvox::vrLiveCapturePoint(s.vrLiveSettings,capture,p,headDelta)){
        m.message="Invalid pose / too close to standing height. Retry.";
        s.vrLiveSettings=before;
        return;
    }
    if(!saveVrLiveSettings()){
        s.vrLiveSettings=before;return;
    }
    m.liveCapture=VrLiveCapture::None;
    m.gunslingerZoneDraftValid=false;
    m.gunslingerZoneMoving=false;
    m.mode=kharvox::VrHandMenuMode::Menu;
    m.message="Zone saved to UserData and applied instantly.";
    kharvox::queueControllerZoneCue(s.handZoneCues[1],GetTickCount64(),
        190,.80f,185.f);
}
// Until native DOOM map identity has been verified, the owner can select
// their campaign mission explicitly. This makes Level Best usable without
// silently attributing checkpoint reloads to incorrect missions.
bool handleVrLevelRecordTrack(int missionIndex){
    if(missionIndex<0||missionIndex>=static_cast<int>(
        kharvox::kCampaignMissions.size()))return false;
    auto& menu=s.vrHandMenu;
    const auto now=GetTickCount64();
    // While ANY mission is being recorded the action becomes STOP RECORD.
    // We never silently replace an unfinished run by choosing another row.
    if(s.levelProgress.armed&&!s.levelProgress.completed){
        if(!kharvox::vrConfirmLevelStop(menu,now)){
            menu.message="Press STOP RECORD again within 6 seconds to discard "
                "the unfinished attempt. Previous records are safe.";
            return true;
        }
        if(!kharvox::levelProgressStopRecord(s.levelProgress)){
            menu.message="No active record to stop.";
            return false;
        }
        s.observedSessionDeaths=KharvoxHudCampaignSessionDeaths();
        // The live HUD clock/death totals never stopped, and display settings
        // stay exactly as the user chose them. Only the ranked run is removed.
        menu.message="Record stopped. Unfinished attempt discarded; "
            "live Level Time and Level Deaths restored.";
        log("[LEVEL-STATS] manual STOP RECORD confirmed; no result saved");
        return true;
    }
    if(!kharvox::vrConfirmLevelStart(menu,missionIndex,now)){
        menu.message="This Mod Menu will close and the timer will start. "
            "Press TRACK THIS LEVEL again within 6 seconds to confirm.";
        return true; // First press only arms confirmation; time is unchanged.
    }
    const auto& mission=kharvox::kCampaignMissions[missionIndex];
    if(!kharvox::levelProgressStartAttempt(s.levelProgress,mission.key,now)){
        menu.message="Cannot start: invalid campaign mission.";
        return false;
    }
    // Beginning another attempt must not resurrect an earlier result toast.
    s.levelResultNoticeStartedAtMs.store(0,std::memory_order_release);
    // A replay of the SAME mission must also start a clean attempt. Preserve
    // all previously saved Level Best times and HUD preferences.
    s.observedSessionDeaths=KharvoxHudCampaignSessionDeaths();
    // A tracked run begins with LEVEL TIME 00:00 and LEVEL DEATHS 0.
    // Level Time's visual toggle automatically turns OFF on completion;
    // unrelated death/session/campaign counters are never reset here.
    const auto previousHud=s.vrLiveSettings;
    kharvox::vrStatsSetEnabled(s.vrLiveSettings,4,true);
    if(!saveVrLiveSettings()){
        s.vrLiveSettings=previousHud;
        log("[LEVEL-STATS] timed run started but Level Time HUD setting could not be saved");
    }
    // Exact same input suppression as DEV Reset All: no trigger-to-fire,
    // A-to-crouch or B-to-jump leaking into DOOM on automatic close.
    menu.mode=kharvox::VrHandMenuMode::Closed;
    menu.navigation={};
    menu.liveCapture=kharvox::VrLiveCapture::None;
    menu.consumeMenuFaceUntilRelease=true;
    menu.consumeMenuTriggerUntilRelease=true;
    menu.hovered=-1;
    menu.cursorX=menu.cursorY=-1.f;
    menu.message=std::string("Timed level attempt started: ")+mission.title;
    log(std::string("[LEVEL-STATS] confirmed timed attempt; Mod Menu closed, mission=")
        +mission.key+"; ranked timer counts all elapsed time except native DOOM Pause");
    return true;
}
// Record resets are separate from Stats RESET ALL. Neither reset
// modifies live counters, timer values or HUD display positions.
bool handleVrLevelRecordReset(int action) {
    auto& menu=s.vrHandMenu;
    if(action!=130)return false;
    const int choice=std::clamp(menu.recordSelected,0,
        static_cast<int>(kharvox::kCampaignMissions.size()));
    const bool all=choice==static_cast<int>(kharvox::kCampaignMissions.size());
    const auto now=GetTickCount64();
    const int key=100+choice; // separate from Stats and timed-start confirms
    if(menu.pendingStatsReset!=key||!menu.confirmDeathResetUntilMs
        ||now>menu.confirmDeathResetUntilMs){
        menu.pendingStatsReset=key;
        menu.confirmDeathResetUntilMs=now+6000ULL;
        menu.message=all
            ?"Press again to delete ALL level records."
            :std::string("Press again to delete records for ")
                +kharvox::kCampaignMissions[choice].title+".";
        return true;
    }
    menu.pendingStatsReset=-1;
    menu.confirmDeathResetUntilMs=0;
    auto previous=s.levelRecords;
    if(!all){
        const auto& keyName=kharvox::kCampaignMissions[choice].key;
        if(!kharvox::levelRecordsReset(s.levelRecords,keyName)){
            menu.message="Selected mission has no records to delete.";
            return true;
        }
    }else kharvox::levelRecordsResetAll(s.levelRecords);
    if(!kharvox::levelRecordsSave(
        kharvox::runtimePathA("level_best_records.cfg"),s.levelRecords)){
        s.levelRecords=std::move(previous);
        menu.message="Could not save record reset; records restored in memory.";
        return true;
    }
    menu.message=all?"All mission records reset.":"Selected mission records reset.";
    s.levelProgress.bestUpdated=false;
    return true;
}
// Hidden DEV panel exposes one guarded reset per persistent stat.
// Public Stats controls never reset any underlying measurement.
bool handleVrStatsMenuAction(int action,bool dev){
    auto& menu=s.vrHandMenu;
    const int item=menu.statsSelected;
    if(item<0||item>=(dev?4:kharvox::kVrStatsItemCount))return false;
    if(dev){
        // 130 resets the selected statistic. 131 is the explicit RESET ALL
        // button. Both require a fresh second press within six seconds.
        // The reset never calls vrLiveSave or modifies display switches,
        // saved corners, weapon profiles or any other user preferences.
        if(action!=130&&action!=131)return false;
        // The campaign/session source counters feed level-death attribution.
        // Any DEV stats reset during an armed run could falsify a record.
        if(!kharvox::levelStatsResetAllowed(s.levelProgress.armed)){
            menu.pendingStatsReset=-1;
            menu.confirmDeathResetUntilMs=0;
            menu.message="Stats resets locked during a tracked level. "
                "Finish the run or start a new attempt.";
            return true;
        }
        const int resetKey=action==131?4:item;
        const auto now=GetTickCount64();
        if(menu.pendingStatsReset!=resetKey||!menu.confirmDeathResetUntilMs
            ||now>menu.confirmDeathResetUntilMs){
            menu.pendingStatsReset=resetKey;
            menu.confirmDeathResetUntilMs=now+6000ULL;
            menu.message=action==131
                ?"Press RESET ALL again within 6 seconds to confirm."
                :"Press the same RESET again within 6 seconds to confirm.";
            return true;
        }
        menu.pendingStatsReset=-1;
        menu.confirmDeathResetUntilMs=0;
        if(action==131){
            // Each persistent counter writes its own file. If either save
            // fails, keep DEV open and visibly report the partial failure.
            // Only on success do we clear session deaths and close the menu.
            const bool deathsReset=KharvoxHudResetCampaignDeathCount();
            const bool timerReset=resetCampaignRunTimeSaved();
            if(!deathsReset||!timerReset){
                menu.message="RESET ALL failed to save "
                    +std::string(!deathsReset?"campaign deaths":"campaign timer")
                    +". DEV remains open; check UserData.";
                return true;
            }
            KharvoxHudResetCampaignSessionDeaths();
            kharvox::resetCampaignRunTime(s.sessionRunTime);
            // This is not an active ranked run (those resets are blocked).
            // Reset the independent live level HUD too, without touching PBs.
            kharvox::levelLiveResetCounters(s.levelLiveStats,
                GetTickCount64(),KharvoxHudCampaignSessionDeaths());
            kharvox::resetCampaignRunTime(s.levelProgress.time);
            s.levelProgress.deaths=0;
            s.observedSessionDeaths=KharvoxHudCampaignSessionDeaths();
            menu.message="Live statistics reset; display and records preserved.";
            if(kharvox::vrStatsMenuShouldClose(dev,action,true)){
                menu.mode=kharvox::VrHandMenuMode::Closed;
                menu.navigation={};
                menu.liveCapture=kharvox::VrLiveCapture::None;
                menu.consumeMenuFaceUntilRelease=true;
                menu.consumeMenuTriggerUntilRelease=true;
                menu.hovered=-1;
                menu.cursorX=menu.cursorY=-1.f;
                log("[STATS] RESET ALL confirmed, saved and closed MOD MENU; "
                    "all display toggles and corner positions preserved");
            }
        }else if(item==0){
            menu.message=KharvoxHudResetCampaignDeathCount()
                ?"Campaign deaths reset to 0."
                :"Failed to save campaign death reset.";
        }else if(item==1){
            KharvoxHudResetCampaignSessionDeaths();
            s.observedSessionDeaths=0;
            s.levelLiveStats.observedSessionDeaths=0;
            menu.message="This session's death count reset to 0.";
        }else if(item==2){
            menu.message=resetCampaignRunTimeSaved()
                ?"Campaign run time reset to 0."
                :"Failed to save campaign timer reset.";
        }else{
            kharvox::resetCampaignRunTime(s.sessionRunTime);
            menu.message="Session time reset to 0.";
        }
        return true;
    }
    auto old=s.vrLiveSettings;
    bool changed=false;
    if(action==110){
        changed=kharvox::vrStatsSetEnabled(s.vrLiveSettings,item,
            !kharvox::vrStatsEnabled(s.vrLiveSettings,item));
    }else if(action>=130&&action<130+kharvox::kStatsGridCells){
        changed=kharvox::vrStatsSetCorner(s.vrLiveSettings,item,
            kharvox::statsPositionFromGridCell(action-130));
    }else return false;
    if(changed){
        // Position changes and display replacement do NOT close the menu.
        // The only Stats action that closes it is confirmed DEV RESET ALL.
        if(!saveVrLiveSettings())s.vrLiveSettings=old;
        else menu.message=std::string(kharvox::vrStatsLabel(item))
            +" saved. Overlapping display switched OFF if occupied.";
    }
    return true;
}
bool handleVrLiveMenuAction(int action){
    using kharvox::VrLiveMod;
    using kharvox::VrLiveCapture;
    auto& m=s.vrHandMenu;
    if(action==110){
        if(!kharvox::vrLiveModToggleable(m.selectedMod)){
            m.message="This setting cannot be toggled in-game.";
            return true;
        }
        if(KharvoxCameraSyncAttackActive()||s.chainsawKillLatched){
            m.message="Finish the execution before changing live mods.";return true;
        }
        const bool was=kharvox::vrLiveEffectiveEnabled(
            s.vrLiveSettings,m.selectedMod,
            m.selectedMod==VrLiveMod::WeaponGrip?s.vrDesktopMods.offHandGripCapture
            :m.selectedMod==VrLiveMod::Gunslinger
                ?s.vrDesktopMods.dynamicShoulderHolster
                   ||s.vrDesktopMods.behindHeadWeaponWheel
            :m.selectedMod==VrLiveMod::PhysicalCrouch?s.vrDesktopMods.physicalCrouch
            :m.selectedMod==VrLiveMod::Killtime?s.vrDesktopKilltime
            :m.selectedMod==VrLiveMod::Painsaw
                ?s.vrDesktopMods.chainsawTriggerThrottle
                   ||s.vrDesktopMods.motionChainsawKillSpeed
            :s.vrDesktopPhysicalGk);
        auto previous=s.vrLiveSettings;
        kharvox::vrLiveSetEnabled(s.vrLiveSettings,m.selectedMod,!was);
        if(!saveVrLiveSettings()){s.vrLiveSettings=previous;return true;}
        m.message=std::string(kharvox::vrLiveModLabel(m.selectedMod))
            +(!was?" enabled LIVE.":" disabled LIVE.");
        return true;
    }
    if(action<130||action>135)return false;
    const int option=action-130;
    auto old=s.vrLiveSettings;
    bool changed=false;
    switch(m.selectedMod){
        case VrLiveMod::WeaponGrip:
            if(option==0){
                m.page=kharvox::VrMenuPage::WeaponGrip;
                m.message="Select weapon, hand and profile. Trigger calibrates.";
            }
            break;
        case VrLiveMod::WeaponRecoil:
            if(option==0){
                m.page=kharvox::VrMenuPage::WeaponRecoil;
                m.navigation={};
                m.message="Choose weapon; adjust kick and recovery; B returns.";
            }
            break;
        case VrLiveMod::Gunslinger:
            if(option==0)openVrLiveCapture(VrLiveCapture::HipRight);
            else if(option==1)openVrLiveCapture(VrLiveCapture::HipLeft);
            else if(option==2)openVrLiveCapture(VrLiveCapture::WheelRight);
            else if(option==3)openVrLiveCapture(VrLiveCapture::WheelLeft);
            else if(option==4){
                kharvox::vrLiveResetGunslingerPositions(s.vrLiveSettings);
                changed=true;
            }
            break;
        case VrLiveMod::PhysicalCrouch:
            if(option==0)openVrLiveCapture(VrLiveCapture::CrouchStanding);
            else if(option==1)openVrLiveCapture(VrLiveCapture::CrouchLowered);
            else if(option==2){
                s.vrLiveSettings.crouchStandDelta=0.f;
                s.vrLiveSettings.crouchLoweredDelta=-.56f;
                s.vrLiveSettings.crouchStandSaved=false;
                s.vrLiveSettings.crouchLoweredSaved=false;
                changed=true;
            }
            break;
        case VrLiveMod::Killtime:
            if(option==0){
                // Slower means more ticks: 240 Hz is slow, 30 Hz is fast.
                s.vrLiveSettings.gloryKillHz=kharvox::vrLiveStepKilltimeHz(
                    s.vrLiveSettings.gloryKillHz,true);
                changed=true;
            }else if(option==1){
                s.vrLiveSettings.gloryKillHz=kharvox::vrLiveStepKilltimeHz(
                    s.vrLiveSettings.gloryKillHz,false);
                changed=true;
            }else if(option==2){
                s.vrLiveSettings.gloryKillHz=40;changed=true; // Owner-approved factory default.
            }
            break;
        case VrLiveMod::TurnSpeed:
            if(option==0){
                s.vrLiveSettings.smoothTurnSpeedDegreesPerSec=kharvox::vrLiveStepTurnSpeed(
                    s.vrLiveSettings.smoothTurnSpeedDegreesPerSec,
                    s.vrDesktopSmoothTurnSpeed,-1);changed=true;
            }else if(option==1){
                s.vrLiveSettings.smoothTurnSpeedDegreesPerSec=kharvox::vrLiveStepTurnSpeed(
                    s.vrLiveSettings.smoothTurnSpeedDegreesPerSec,
                    s.vrDesktopSmoothTurnSpeed,1);changed=true;
            }else if(option==2){
                s.vrLiveSettings.smoothTurnSpeedDegreesPerSec=0.f;changed=true;
            }
            break;
        case VrLiveMod::DeathCounter:
            if(option>=0&&option<4)
                changed=kharvox::vrStatsSetCorner(s.vrLiveSettings,0,option);
            break;
        case VrLiveMod::PhysicalGloryKills:
            if(option==0){
                s.vrLiveSettings.punchSpeedMetersPerSec=std::max(.5f,
                    s.vrLiveSettings.punchSpeedMetersPerSec-.2f);
                changed=true;
            }else if(option==1){
                s.vrLiveSettings.punchSpeedMetersPerSec=std::min(5.f,
                    s.vrLiveSettings.punchSpeedMetersPerSec+.2f);
                changed=true;
            }else if(option==2){
                s.vrLiveSettings.punchHands=(s.vrLiveSettings.punchHands+1)%3;
                changed=true;
            }else if(option==3){
                s.vrLiveSettings.punchSpeedMetersPerSec=s.vrDesktopPunchSpeed;
                s.vrLiveSettings.punchHands=s.vrDesktopPunchHands;
                changed=true;
            }
            break;
        default:break;
    }
    if(changed){
        if(!saveVrLiveSettings())s.vrLiveSettings=old;
        else m.message="Applied LIVE and saved to UserData.";
    }
    return true;
}
