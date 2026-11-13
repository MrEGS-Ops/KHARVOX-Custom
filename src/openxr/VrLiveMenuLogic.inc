// All configurable VR mods are applied from their current in-process values,
// NOT by restarting the game or rewriting the launcher JSON. Desktop defaults
// remain authoritative until a user explicitly changes a VR checkbox.
struct VrLiveMenuEntry {
    const char* title;
    kharvox::VrLiveMod mod{kharvox::VrLiveMod::Other};
    bool enabled{};

};
std::vector<VrLiveMenuEntry> activeVrMenuEntries(){
    using kharvox::VrLiveMod;
    std::vector<VrLiveMenuEntry> rows;
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
    add("Physical Glory Kills",VrLiveMod::PhysicalGloryKills,
        s.physicalGlorykillEnabled,true);
    // Group the two live ON/OFF features together at the bottom.
    add("Physical Crouch",VrLiveMod::PhysicalCrouch,
        s.customMods.physicalCrouch,true);
    add("PAINSAW",VrLiveMod::Painsaw,
        s.customMods.chainsawTriggerThrottle
            ||s.customMods.motionChainsawKillSpeed,true);
    return rows;
}
// This is the PLAYABILITY overview, not a calibration tutorial. It appears
// immediately as the player scrolls the mods list, before pressing A to
// open its controls. Keep the text short enough for the in-headset panel.
struct VrLiveModOverview {
    std::array<const char*,6> lines{};
    const char* subtitle{"HOW IT WORKS"};
};
VrLiveModOverview vrLiveModOverview(kharvox::VrLiveMod mod){
    using kharvox::VrLiveMod;
    switch(mod){
        case VrLiveMod::WeaponGrip:return {{{
            "Physically attach each weapon to your VR hand.",
            "One-handed aim follows your weapon controller.",
            "Use your other hand to grab the weapon's grip.",
            "Both hands steer the weapon naturally together.",
            "Supports left/right weapon hands and heavy modes.",
            "A: open weapon-specific grip fitting."}}};
        case VrLiveMod::Gunslinger:return {{{
            "Fast weapon swapping with physical hip holsters.",
            "Place a hand in its side's hip zone and grip.",
            "A quick grab instantly equips that holstered gun.",
            "Reach behind your head for the weapon wheel.",
            "The wheel follows the hand that opened it.",
            "A: adjust hip zones, wheel zones and resets."}}};
        case VrLiveMod::Killtime:return {{{
            "Changes the speed of cinematic Glory Kills.",
            "Finisher animation speed uses a tick-Hz preset.",
            "Smaller Hz values finish executions faster.",
            "Larger Hz values make executions slower.",
            "Other gameplay remains on native timing.",
            "A: adjust and reset the Glory Kill speed."}}};
        case VrLiveMod::PhysicalGloryKills:return {{{
            "Perform Glory Kills with real forward punches.",
            "Face an executable demon and strike naturally.",
            "Tracks physical hand speed for punch detection.",
            "Adjust the punch threshold to fit your style.",
            "Choose the active punch hand or use both.",
            "A: change punch speed and active hand."}}};
        case VrLiveMod::PhysicalCrouch:return {{{
            "Crouch by physically lowering your headset.",
            "Your real head height controls your stance.",
            "Stand again to rise back up in-game.",
            "Standing and lowered heights can be fitted.",
            "Switch OFF to use the alternate crouch input.",
            "A: toggle and calibrate crouch heights."}}};
        case VrLiveMod::Painsaw:return {{{
            "Chainsaw revs controlled with trigger pulls.",
            "First pull starts a normal chainsaw execution.",
            "Release and pull for two more speed surges.",
            "The third pull accelerates the finish.",
            "Uses DOOM's native chainsaw sound and haptics.",
            "A: enable or disable PAINSAW."}}};
        default:return {{{"Select a mod for information."}}};
    }
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
        KharvoxCameraSetCrouchState(false);
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
}
bool saveVrLiveSettings(){
    const auto path=kharvox::runtimePathA("vr_live_mods_saved.cfg");
    if(!kharvox::vrLiveSave(path,s.vrLiveSettings)){
        s.vrHandMenu.message="Save failed: UserData/vr_live_mods_saved.cfg";
        return false;
    }
    applyVrLiveModRuntimeState();
    return true;
}
void openVrLiveCapture(kharvox::VrLiveCapture capture){
    auto& m=s.vrHandMenu;
    m.liveCapture=capture;
    m.previewValid=false;
    m.mode=kharvox::VrHandMenuMode::Calibrating;
    const bool crouch=capture==kharvox::VrLiveCapture::CrouchStanding
        ||capture==kharvox::VrLiveCapture::CrouchLowered;
    m.message=crouch?"Move your HEAD to target height, then RIGHT TRIGGER."
        :"Move the matching HAND to target zone, then RIGHT TRIGGER.";
    log("[VR-MODS] live calibration armed; fire remains blocked");
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
        const bool right=capture==VrLiveCapture::HipRight
            ||capture==VrLiveCapture::WheelRight;
        const auto& controller=right?s.rightGripController:s.leftGripController;
        if(!controller.valid||!controller.positionTracked){
            m.message="Chosen hand not tracked. Position not saved.";return;
        }
        const auto relative=controllerRelativeToBodyTrackingOrigin(controller).position;
        const auto hd=positionRelativeToBodyTrackingOrigin(s.trackingHeadPosition);
        p={right?relative.x-hd.x:hd.x-relative.x,
            relative.y-hd.y,relative.z-hd.z};
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
    m.mode=kharvox::VrHandMenuMode::Menu;
    m.message="Position saved to UserData and applied instantly.";
    kharvox::queueControllerZoneCue(s.handZoneCues[1],GetTickCount64(),
        190,.80f,185.f);
}
bool handleVrLiveMenuAction(int action){
    using kharvox::VrLiveMod;
    using kharvox::VrLiveCapture;
    auto& m=s.vrHandMenu;
    if(action==110){
        if(!kharvox::vrLiveModToggleable(m.selectedMod)){
            m.message="Core settings remain accessible; only Physical Crouch and PAINSAW toggle.";
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
        case VrLiveMod::Gunslinger:
            if(option==0)openVrLiveCapture(VrLiveCapture::HipRight);
            else if(option==1)openVrLiveCapture(VrLiveCapture::HipLeft);
            else if(option==2)openVrLiveCapture(VrLiveCapture::WheelRight);
            else if(option==3)openVrLiveCapture(VrLiveCapture::WheelLeft);
            else if(option==4){
                s.vrLiveSettings.hipShift={};
                s.vrLiveSettings.wheelShift={};
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
                s.vrLiveSettings.gloryKillHz=0;changed=true;
            }
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
