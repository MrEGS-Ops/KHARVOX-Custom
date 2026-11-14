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
    add("Weapon Stability",VrLiveMod::WeaponRecoil,
        kharvox::anyWeaponRecoilEnabled(s.recoilSettings),true);
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
};
VrLiveModOverview vrLiveModOverview(kharvox::VrLiveMod mod){
    using kharvox::VrLiveMod;
    switch(mod){
        case VrLiveMod::WeaponGrip:return {{{
            "Fit guns to your physical VR hands.",
            "Equip LEFT-hand gun to fit LEFT.",
            "Equip RIGHT-hand gun to fit RIGHT.",
            "Choose a weapon and CALIBRATE.",
            "Fit primary and support grips.",
            "Left/right profiles save separately."}}};
        case VrLiveMod::Gunslinger:return {{{
            "LEFT and RIGHT behind-head wheels.",
            "Put either hand behind your head.",
            "Exit holding Grip to select a gun.",
            "Exit without Grip to cancel.",
            "Left/right hip zones quick-draw guns.",
            "Release Grip in a hip zone to set."}}};
        case VrLiveMod::Killtime:return {{{
            "Change cinematic Glory Kill speed.",
            "Lower tick Hz = faster execution.",
            "Higher tick Hz = slower execution.",
            "Only affects the finisher timing.",
            "Normal movement is unchanged.",
            "Choose speed in CONFIGURE."}}};
        case VrLiveMod::WeaponRecoil:return {{{
            "One-handed firing kicks the barrel.",
            "Repeated shots make the muzzle climb.",
            "Lower your hand to compensate.",
            "A true support-grip steadies the gun.",
            "Set strength and recovery per gun.",
            "Open CONFIGURE to calibrate."}}};
        case VrLiveMod::PhysicalGloryKills:return {{{
            "Trigger Glory Kills with punches.",
            "Strike a staggered demon in VR.",
            "Actual hand velocity is measured.",
            "Choose right, left, or both hands.",
            "Adjust sensitivity to your swing.",
            "Set punch options in CONFIGURE."}}};
        case VrLiveMod::PhysicalCrouch:return {{{
            "Physically lower your head to duck.",
            "Stand back up to rise in-game.",
            "Your headset height is tracked.",
            "Fit standing and crouching height.",
            "When OFF, use right-stick crouch.",
            "Toggle and fit in CONFIGURE."}}};
        case VrLiveMod::Painsaw:return {{{
            "Three-pull chainsaw revving mode.",
            "First pull starts the native kill.",
            "Release and re-pull up to twice.",
            "Each pull boosts execution speed.",
            "",
            "Toggle mode in CONFIGURE."}}};
        default:return {{{"Select a mod for information."}}};
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
                s.vrLiveSettings.gloryKillHz=60;changed=true; // Reset KILLTIME to factory default.
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
