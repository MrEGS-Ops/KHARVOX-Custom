// All configurable VR mods are applied from their current in-process values,
// NOT by restarting the game or rewriting the launcher JSON. Desktop defaults
// remain authoritative until a user explicitly changes a VR checkbox.
struct VrLiveMenuEntry {
    const char* title;
    kharvox::VrLiveMod mod{kharvox::VrLiveMod::Other};
    bool enabled{};
    bool devSection{};  // not a toggle or a gameplay mod
};
struct VrDevMenuEntry {
    const char* title;
    const char* status; // LOCKED, ON, AUTO or DESKTOP, never an in-headset switch
};
std::vector<VrDevMenuEntry> devVrMenuEntries(){
    // This is a read-only inventory: changing a desktop-only mod in the
    // headset must never mutate its flags or claim it has been enabled.
    std::vector<VrDevMenuEntry> rows;
    const auto enabled=[&](const char* label,bool isOn){
        if(isOn)rows.push_back({label,"ON"});
    };
    if(s.customMods.handFocusedRs)rows.push_back({"Hand Focus","LOCKED"});
    enabled("PAINSAW",s.customMods.chainsawTriggerThrottle);
    enabled("Wheel Button Remap (legacy)",s.weaponWheelRemapEnabled);
    enabled("Hands Jump",s.handsJumpEnabled);
    enabled("Boxing Guard",s.customMods.boxingGuard);
    enabled("Back-of-Hand HUD",s.customMods.backOfHandHud);
    enabled("No HUD",s.customMods.disableHud);
    enabled("No Weapon Wheel",s.customMods.disableWeaponWheel);
    enabled("Directional Dash",s.customMods.directionalDash);
    enabled("Gauss Charge Slow",s.customMods.gaussChargeSlowMovement);
    enabled("Persistent SSG Reload",s.customMods.persistentSuperShotgunReload);
    enabled("Physical Grenade",s.customMods.physicalGrenadeThrow);
    enabled("Story Skip",s.customMods.storyCinematicSkip);
    enabled("Story Free Walk",s.customMods.storyFreeWalk);
    enabled("Story Camera Focus",s.customMods.storyCameraFocus);
    enabled("Uninterrupted Story",s.customMods.uninterruptedStory);
    enabled("Revenge Demon",s.customMods.revengeDemon);
    enabled("Haptic Overhaul",s.customMods.hapticOverhaul);
    enabled("Fast Glory Kills",s.customMods.fastGloryKills);
    enabled("Fast Chainsaw Kills",s.customMods.fastChainsawKills);
    enabled("Cinematic Exit Handoff",s.customMods.cinematicExitHandoff);
    // These launcher Developer overrides have no adjustable in-VR settings.
    // Their desktop selections are not exposed as live flags here; show
    // DESKTOP instead of inventing an enabled/disabled runtime status.
    rows.push_back({"DEV: Glory Kill Highlighting","DESKTOP"});
    rows.push_back({"DEV: Glory Kill Motion Blur","DESKTOP"});
    rows.push_back({"DEV: Camera Kicks & Shaking","DESKTOP"});
    rows.push_back({"DEV: Native Weapon Kick","DESKTOP"});
    rows.push_back({"DEV: Hand-Hit Reactions","DESKTOP"});
    rows.push_back({"DEV: Native Ledge Speed","DESKTOP"});
    rows.push_back({"DEV: Scripted Cinematic Speed","DESKTOP"});
    rows.push_back({"DEV: RS Melee / Animation",
        s.physicalGlorykillEnabled?"AUTO":"OFF"});
    return rows;
}
std::vector<VrLiveMenuEntry> activeVrMenuEntries(){
    using kharvox::VrLiveMod;
    std::vector<VrLiveMenuEntry> rows;
    const auto add=[&](const char* title,VrLiveMod id,bool on,bool showEvenIfOff=false){
        if(on||showEvenIfOff)rows.push_back({title,id,on});
    };
    // Always show the five editable features so OFF ones can be re-enabled.
    add("Weapon Grip Calibration",VrLiveMod::WeaponGrip,
        s.customMods.offHandGripCapture,true);
    add("Gunslinger",VrLiveMod::Gunslinger,
        s.customMods.dynamicShoulderHolster||s.customMods.behindHeadWeaponWheel,true);
    add("Physical Crouch",VrLiveMod::PhysicalCrouch,
        s.customMods.physicalCrouch,true);
    add("KILLTIME: RIP & TEAR",VrLiveMod::Killtime,
        s.vrLiveKilltimeEnabled,true);
    add("Physical Glory Kills",VrLiveMod::PhysicalGloryKills,
        s.physicalGlorykillEnabled,true);
    // Keep the left list strictly configurable. DEV consolidates every
    // currently active read-only mod and non-configurable developer override.
    rows.push_back({"DEV",VrLiveMod::Other,true,true});
    return rows;
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
        if(!kharvox::vrLiveModEditable(m.selectedMod)){
            m.message="This mod is read-only. Use the desktop launcher.";return true;
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
                s.vrLiveSettings.gloryKillHz=std::max(30,
                    (s.vrLiveSettings.gloryKillHz==0?200:
                        s.vrLiveSettings.gloryKillHz)-10);
                changed=true;
            }else if(option==1){
                s.vrLiveSettings.gloryKillHz=std::min(240,
                    (s.vrLiveSettings.gloryKillHz==0?200:
                        s.vrLiveSettings.gloryKillHz)+10);
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
