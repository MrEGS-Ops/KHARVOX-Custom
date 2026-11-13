// All configurable VR mods are applied from their current in-process values,
// NOT by restarting the game or rewriting the launcher JSON. Desktop defaults
// remain authoritative until a user explicitly changes a VR checkbox.
struct VrLiveMenuEntry {
    const char* title;
    kharvox::VrLiveMod mod{kharvox::VrLiveMod::Other};
    bool enabled{};
    bool devSection{};  // not a toggle or a gameplay mod
};
// Browse first, configure second. These blurbs describe the actual
// gameplay mod (NOT the calibration screen) while the left list has focus.
// Keep lines short enough for the 384-pixel right-hand viewport.
struct VrLiveModOverview {
    std::array<const char*,7> lines{};
    int count{};
};
VrLiveModOverview vrLiveModOverview(kharvox::VrLiveMod mod){
    using kharvox::VrLiveMod;
    switch(mod){
        case VrLiveMod::WeaponGrip:return {{
            "Keeps the weapon glove attached to",
            "your physical weapon controller.",
            "Supports either weapon hand and",
            "two-hand grips on supported guns.",
            "The gun follows your aiming hand;",
            "the support hand steers while held.",
            "Each weapon has its own grip fit."},7};
        case VrLiveMod::Gunslinger:return {{
            "Fast physical weapon access using",
            "left/right hip quick-holster zones.",
            "Reach into a hip zone and Grip to",
            "equip its stored weapon instantly.",
            "Grip release in the appropriate",
            "zone assigns the held-side weapon.",
            "Behind-head weapon wheel included."},7};
        case VrLiveMod::Killtime:return {{
            "Adjusts Glory Kill action speed,",
            "giving executions a different",
            "cinematic rhythm while playing.",
            "Higher tick Hz = slower action.",
            "Lower tick Hz = faster action.",
            "Other normal gameplay keeps its",
            "regular timing and responsiveness."},7};
        case VrLiveMod::PhysicalGloryKills:return {{
            "Use real punching motions to",
            "perform physical Glory Kills.",
            "Aim at a vulnerable enemy, then",
            "drive your fist forward to strike.",
            "Choose left, right or both hands",
            "and the required punch speed.",
            "No artificial glove movement."},7};
        case VrLiveMod::PhysicalCrouch:return {{
            "Duck physically to crouch in DOOM.",
            "Headset height determines when",
            "you are standing or crouching.",
            "Custom height thresholds can be",
            "recorded for your play space.",
            "When disabled, a thumbstick",
            "crouch shortcut is available."},7};
        case VrLiveMod::Painsaw:return {{
            "Physical chainsaw rev controls",
            "during a confirmed native kill.",
            "Release and pull the trigger to",
            "add up to three speed pulses.",
            "Includes distinctive haptics.",
            "Uses original DOOM chainsaw",
            "audio, not an external sound."},7};
        default:return {{"Select a configurable mod to",
                          "read its gameplay description."},2};
    }
}
struct VrDevMenuEntry {
    const char* title;
    const char* why; // explanation displayed by the [i] button, never a toggle
};
std::vector<VrDevMenuEntry> devVrMenuEntries(){
    // Read-only, information-only entries. Only Mods with adjustable live
    // controls appear on the Configurable side of this in-headset menu.
    std::vector<VrDevMenuEntry> rows;
    const auto enabled=[&](const char* label,bool isOn,const char* why){
        if(isOn)rows.push_back({label,why});
    };
    enabled("Hand Focus",s.customMods.handFocusedRs,
        "Core hand-directed focus; no separate VR settings.");
    enabled("Hands Jump",s.handsJumpEnabled,
        "Jump gesture is configured in the desktop launcher.");
    enabled("Boxing Guard",s.customMods.boxingGuard,
        "Guard behaviour has no adjustable live menu controls.");
    enabled("Back-of-Hand HUD",s.customMods.backOfHandHud,
        "HUD position is configured separately from VR Mods.");
    enabled("No HUD",s.customMods.disableHud,
        "HUD visibility is a desktop gameplay option.");
    enabled("No Weapon Wheel",s.customMods.disableWeaponWheel,
        "Weapon wheel suppression is a desktop-only setting.");
    enabled("Directional Dash",s.customMods.directionalDash,
        "Movement action has no live configuration controls.");
    enabled("Gauss Charge Slow",s.customMods.gaussChargeSlowMovement,
        "Siege movement slowdown has no live options.");
    enabled("Persistent SSG Reload",s.customMods.persistentSuperShotgunReload,
        "SSG reload persistence has no live options.");
    enabled("Physical Grenade",s.customMods.physicalGrenadeThrow,
        "Grenade gesture has no live menu settings.");
    enabled("Story Skip",s.customMods.storyCinematicSkip,
        "Cinematic shortcut is fixed by desktop controls.");
    enabled("Story Free Walk",s.customMods.storyFreeWalk,
        "Story movement is controlled outside VR Mod Settings.");
    enabled("Story Camera Focus",s.customMods.storyCameraFocus,
        "Scripted camera focus is not adjusted live.");
    enabled("Uninterrupted Story",s.customMods.uninterruptedStory,
        "Story protection has no user-adjustable live values.");
    enabled("Revenge Demon",s.customMods.revengeDemon,
        "Gameplay feature has no live configuration values.");
    enabled("Haptic Overhaul",s.customMods.hapticOverhaul,
        "Haptic profiles are selected in the desktop launcher.");
    enabled("Fast Glory Kills",s.customMods.fastGloryKills,
        "Fixed execution speed override; no live sliders here.");
    enabled("Fast Chainsaw Kills",s.customMods.fastChainsawKills,
        "Fixed chainsaw execution override; no live sliders.");
    enabled("Cinematic Exit Handoff",s.customMods.cinematicExitHandoff,
        "Safety handoff is automatic and is not user-tuned.");
    rows.push_back({"Glory Kill Highlighting",
        "Developer override: adjust in the desktop launcher."});
    rows.push_back({"Glory Kill Motion Blur",
        "Developer override: adjust in the desktop launcher."});
    rows.push_back({"Camera Kicks & Shaking",
        "Developer override: adjust in the desktop launcher."});
    rows.push_back({"Native Weapon Kick",
        "Developer override: adjust in the desktop launcher."});
    rows.push_back({"Hand-Hit Reactions",
        "Developer override: adjust in the desktop launcher."});
    rows.push_back({"Native Ledge Speed",
        "Developer override: adjust in the desktop launcher."});
    rows.push_back({"Scripted Cinematic Speed",
        "Developer override: adjust in the desktop launcher."});
    rows.push_back({"RS Melee / Animation",
        "Physical GK owns melee input; safety behaviour is automatic."});
    return rows;
}
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
    // The two real toggles are grouped at the bottom.
    add("Physical Crouch",VrLiveMod::PhysicalCrouch,
        s.customMods.physicalCrouch,true);
    add("PAINSAW",VrLiveMod::Painsaw,
        s.customMods.chainsawTriggerThrottle
            ||s.customMods.motionChainsawKillSpeed,true);
    // No DEV category in the in-game window. Desktop diagnostics remain
    // available separately, without cluttering gameplay configuration.
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
