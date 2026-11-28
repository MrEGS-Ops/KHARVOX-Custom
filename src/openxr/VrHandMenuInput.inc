// All VR menu inputs are handled before DOOM's native melee/pause/calibration
// routes. The menu owns both clicks and triggers while it is visible; trigger
// save is edge-based and cannot fire the selected weapon.
// Persistent successful-save markers live alongside the editable calibration file.
std::string vrCalibrationStatusPath(){return kharvox::runtimePathA("offhand_right_hand_positions.cfg")+".status";}
void refreshVrHandMenuSavedStatus(){
    auto& m=s.vrHandMenu;
    std::ifstream in(vrCalibrationStatusPath());
    unsigned right{},left{};
    if(in>>right>>left&&right<1024&&left<1024){
        for(size_t j=0;j<kharvox::kVrCalibrationWeapons.size();++j){
            m.rightSaved[j]=(right&(1u<<j))!=0;m.leftSaved[j]=(left&(1u<<j))!=0;
        }
        return;
    }
    for(size_t j=0;j<kharvox::kVrCalibrationWeapons.size();++j){
        const std::string key=kharvox::kVrCalibrationWeapons[j];
        m.rightSaved[j]=kharvox::vrPrimaryPoseChangedFromFactory(s.rightGripSettings,s.rightGripFactorySettings,key,false);
        m.leftSaved[j]=kharvox::vrPrimaryPoseChangedFromFactory(s.rightGripSettings,s.rightGripFactorySettings,key,true);
    }
}
bool persistVrCalibrationStatus(){
    const auto& m=s.vrHandMenu;unsigned right{},left{};
    for(size_t j=0;j<kharvox::kVrCalibrationWeapons.size();++j){
        if(m.rightSaved[j])right|=1u<<j;if(m.leftSaved[j])left|=1u<<j;
    }
    const auto path=vrCalibrationStatusPath(),tmp=path+".tmp";
    {std::ofstream out(tmp,std::ios::trunc);out<<right<<" "<<left<<"\n";out.close();if(!out){DeleteFileA(tmp.c_str());return false;}}
    if(!MoveFileExA(tmp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){
        DeleteFileA(tmp.c_str());return false;
    }
    return true;
}
void recordVrCalibrationStatus(bool reset=false){
    auto& m=s.vrHandMenu;refreshVrHandMenuSavedStatus();
    for(size_t j=0;j<kharvox::kVrCalibrationWeapons.size();++j){
        if(std::string(kharvox::kVrCalibrationWeapons[j])!=m.weapon())continue;
        if(reset)m.rightSaved[j]=m.leftSaved[j]=false;
        else if(s.weaponHandLeft)m.leftSaved[j]=true;
        else m.rightSaved[j]=true;
        break;
    }
    if(!persistVrCalibrationStatus()){
        log("[VR-HAND-MENU] profile status persistence FAILED; calibration data write separate");
        m.message+=" (status file write failed)";
    }
}
void updateVrMenuPreview(bool primaryWeaponGripHeld){
    auto& m=s.vrHandMenu;
    const auto weapon=KharvoxWeaponKindFromKey(m.weapon());
    const bool primaryHand=m.rightHand!=s.weaponHandLeft;
    const auto& freeGrip=s.weaponHandLeft?s.rightGripController:s.leftGripController;
    const auto& freeAim=s.weaponHandLeft?s.rightController:s.leftController;
    // PRIMARY rotation uses the tracked WEAPON-HAND GRIP controller while
    // the native gun pose is temporarily frozen. The opposite controller
    // does not influence primary calibration.
    // SUPPORT must position its glove from its own physical GRIP origin,
    // matching the normal free hand. Its AIM ray is forward-offset and
    // must not silently replace invalid GRIP tracking during capture.
    const auto placement=primaryHand
        ?kharvox::vrCalibrationLivePose(freeGrip,freeAim)
        :kharvox::vrSupportCalibrationPhysicalGrip(freeGrip,freeAim);
    const bool matchingWeapon=weapon!=KharvoxWeaponKind::Unknown
        &&weapon==KharvoxWeaponCurrentKind()
        &&s.weaponHandLeft==m.calibratingWeaponLeft;
    const bool gunTracked=s.lastWeaponRenderedValid
        &&s.lastWeaponRenderedKind==weapon;
    const bool canSample=matchingWeapon&&gunTracked&&placement.valid;
    if(primaryHand){
        const auto& weaponGrip=s.weaponHandLeft
            ?s.leftGripController:s.rightGripController;
        // Single unsaved draft: Grip release holds the latest glove angle;
        // default sticks can nudge the gun without touching that angle.
        // On Grip press, seed the rotation delta from the ACTUAL weapon
        // controller in the current (soon frozen) gun-local frame.
        if(primaryWeaponGripHeld&&matchingWeapon&&gunTracked
            &&weaponGrip.valid){
            const auto inverted=conjugate(s.lastWeaponRenderedTrackingRotation);
            const auto local=normalizeQuaternion(multiply(
                inverted,weaponGrip.orientation));
            const kharvox::RightHandGripRotation weaponRotation{
                local.x,local.y,local.z,local.w,true};
            m.previewRotation=kharvox::vrCalibrationRelativeRotation(
                m.rotationEdit,true,weaponRotation,m.previewRotation);
        }else{
            m.previewRotation=kharvox::vrCalibrationRelativeRotation(
                m.rotationEdit,false,{},m.previewRotation);
        }
        m.previewValid=m.previewValid&&kharvox::validRightHandVisualPlacement(
                m.previewPosition)
            &&m.previewRotation.valid
            &&kharvox::validRightHandRotation(m.previewRotation);
        m.primaryGripPreviewHeld=kharvox::vrPrimaryGripHoldPreviewAllowed(
                m.mode,true,m.weapon())
            &&primaryWeaponGripHeld&&m.previewValid
            &&matchingWeapon&&gunTracked&&weaponGrip.valid;
    }else{
        // SUPPORT position follows the real free-hand GRIP tracker, not
        // its displaced AIM laser. The saved local offset and displayed
        // hand are measured in the exact same weapon reference frame.
        if(!canSample){
            m.previewValid=false;
            m.primaryGripPreviewHeld=false;
        }else if(kharvox::vrWeaponManualCalibrationLive(
                m.mode,primaryWeaponGripHeld,m.supportPreviewHeld)){
            const auto inverted=conjugate(s.lastWeaponRenderedTrackingRotation);
            const auto local=rotateVector(inverted,
                subtractVector(placement.position,
                    s.lastWeaponRenderedTrackingAnchor));
            const auto rotation=normalizeQuaternion(multiply(
                inverted,placement.orientation));
            m.previewPosition={local.x,local.y,local.z};
            m.previewRotation={rotation.x,rotation.y,rotation.z,rotation.w,true};
            m.previewValid=kharvox::validRightHandPlacement(m.previewPosition)
                &&kharvox::validRightHandRotation(m.previewRotation);
            m.primaryGripPreviewHeld=false;
        }
    }
    const auto now=GetTickCount64();
    if(now>=m.lastPreviewDiagnosticAt+2000ULL){
        m.lastPreviewDiagnosticAt=now;
        log(std::string("[VR-HAND-CAL] ")+m.weapon()
            +(primaryHand?" primary":" support")
            +(primaryHand
                ?(primaryWeaponGripHeld?" Grip-held WEAPON_HAND_GLOVE_ROTATION"
                    :" Grip-released GUN_FINE_TUNE")
                :(primaryWeaponGripHeld?" support Grip held"
                    :" support Grip released"))
            +" matched="+(matchingWeapon?"1":"0")
            +" gunPose="+(gunTracked?"1":"0")
            +" freePose="+(placement.valid?"1":"0")
            +" rotationReference="+(m.rotationEdit.referenceValid?"1":"0")
            +" gunDraftXYZ="+std::to_string(m.fineTuneAccumulated.x)
            +","+std::to_string(m.fineTuneAccumulated.y)
            +","+std::to_string(m.fineTuneAccumulated.z)
            +" valid="+(m.previewValid?"1":"0"));
    }
}
// A quick stick flick nudges 1 cm; holding a stick repeats 1 cm at a
// comfortable fixed pace. DEFAULT no Grip adjusts the GUN only.
// Grip HELD freezes native gun pose and the weapon hand rotates its GLOVE.
// Release Grip locks glove angle. Right trigger SAVES.
// The calibration menu still suppresses locomotion and firing.
void serviceVrWeaponFineTune(bool weaponGripDown){
    auto& m=s.vrHandMenu;
    const bool primary=m.mode==kharvox::VrHandMenuMode::Calibrating
        &&m.liveCapture==kharvox::VrLiveCapture::None
        &&m.page==kharvox::VrMenuPage::WeaponGrip
        &&m.rightHand!=s.weaponHandLeft
        &&m.calibratingWeaponLeft==s.weaponHandLeft
        &&m.previewValid&&!weaponGripDown&&s.lastWeaponRenderedValid
        &&s.lastWeaponRenderedKind==KharvoxWeaponCurrentKind()
        &&m.weapon()==std::string(KharvoxWeaponKindKey(KharvoxWeaponCurrentKind()));
    XrVector2f rightStick{};
    if(primary){
        // The normal right-stick sample is read AFTER menu dispatch.
        // Fetch it here for the default (NO Grip) fine-tune; never modify
        // the gameplay mapping or the normal empty-hand control routes.
        XrActionStateGetInfo info{XR_TYPE_ACTION_STATE_GET_INFO};
        info.action=s.doomTurn;
        XrActionStateVector2f state{XR_TYPE_ACTION_STATE_VECTOR2F};
        if(XR_SUCCEEDED(s.getActionStateVector2f(s.session,&info,&state))
            &&state.isActive)rightStick=state.currentState;
    }
    const auto nowMs=GetTickCount64();
    const auto step=kharvox::vrWeaponFineTuneCentimeterStep(
        m.fineTuneEdges,primary,weaponGripDown,
        s.leftStick.x,s.leftStick.y,rightStick.y,nowMs);
    if(step.x==0.f&&step.y==0.f&&step.z==0.f)return;
    const auto candidate=kharvox::RightHandGripOffset{
        m.fineTuneAccumulated.x+step.x,
        m.fineTuneAccumulated.y+step.y,
        m.fineTuneAccumulated.z+step.z};
    if(!kharvox::validRightHandPlacement(candidate)){
        m.message="Gun fine-tune reached its safe 85 cm limit.";
        return;
    }
    // Move the gun; the tracked physical glove and its rotation remain
    // untouched by the nudge. Runtime reads this each preview frame.
    m.fineTuneAccumulated=candidate;
    kharvox::queueControllerZoneCue(
        s.handZoneCues[kharvox::hapticOutputForPhysicalSide(
            s.weaponHandLeft?1:0)],nowMs,28,.28f,170.f);
    log(std::string("[VR-HAND-FINETUNE] 1cm nudge weapon=")+m.weapon()
        +(s.weaponHandLeft?" left":" right")
        +" x="+std::to_string(step.x)+" y="+std::to_string(step.y)
        +" z="+std::to_string(step.z)
        +" totalX="+std::to_string(m.fineTuneAccumulated.x));
}
void startVrSelectedWeaponCalibration();
void finishVrWeaponSequence(){
    auto& m=s.vrHandMenu;
    if(m.weaponCalibrationStage)selectPhysicalWeaponHand(m.calibrationOriginalWeaponLeft,"calibration sequence exit");
    m.page=kharvox::VrMenuPage::Mods;m.supportPreviewHeld=false;
    m.weaponCalibrationStage=0;m.rightHand=!s.weaponHandLeft;m.previewValid=false;m.primaryGripPreviewHeld=false;m.rotationEdit={};
    m.mode=kharvox::VrHandMenuMode::Menu;
}
void beginVrWeaponSequence(bool left){
    auto& m=s.vrHandMenu;
    m.calibrationOriginalWeaponLeft=s.weaponHandLeft;
    m.calibrationStartedLeft=left;
    m.weaponCalibrationStage=left?4:1;
    selectPhysicalWeaponHand(left,"calibration primary");
    m.rightHand=!left;
    startVrSelectedWeaponCalibration();
}
void saveVrHandMenuCalibration(){
    auto& m=s.vrHandMenu;
    if(!m.previewValid){m.message="Tracking/placement invalid; try again.";return;}
    const auto kind=KharvoxWeaponKindFromKey(m.weapon());
    if(kind!=KharvoxWeaponCurrentKind()
        ||s.weaponHandLeft!=m.calibratingWeaponLeft){
        m.message="Weapon or holding hand changed; capture not saved.";return;
    }
    // Keep the chosen NORMAL / ALT FIRE slot stable for this capture.
    // No attachment-name scan or separate Rotor/Turret confirmation.
    if(m.calibrationAlternateProfile!=m.alternateProfile){
        m.message="MODE CHANGED: cancel and select the Alt Fire toggle again.";
        return;
    }
    const bool primaryHand=m.rightHand!=s.weaponHandLeft;
    std::string why;
    bool ok=false;
    const bool alternate=m.alternateProfile
        &&(kind==KharvoxWeaponKind::GaussCannon
            ||kind==KharvoxWeaponKind::Chaingun);
    const std::string profileKey=m.weapon();
    if(primaryHand){
        ok=kharvox::saveRightHandVisualPose(
            kharvox::runtimePathA("offhand_right_hand_positions.cfg"),
            profileKey,alternate,m.previewPosition,m.previewRotation,
            &why,s.weaponHandLeft,&m.fineTuneAccumulated);
        if(ok){
            s.rightGripSettings=kharvox::readRightHandGripSettings(
                kharvox::runtimePathA("offhand_right_hand_positions.cfg"));
            s.primaryVisualLocked=false;
        }
    }else if(alternate){
        // One alternate support-hand profile, regardless of the attachment.
        // Legacy _siege / _turret filenames remain compatible on disk.
        const auto rightFile=kharvox::runtimePathA("offhand_right_hand_positions.cfg");
        ok=kharvox::saveHeavyAlternativeSupport(
            rightFile,profileKey,m.previewPosition,&why,s.weaponHandLeft);
        if(ok){
            s.rightGripSettings=kharvox::readRightHandGripSettings(rightFile);
            s.twoHandLatched=false;
            s.supportVisualLocked=false;
        }
    }else{
        auto* profile=twoHandCalibrationFor(kind);
        if(!profile){m.message="No support-hand profile for this weapon.";return;}
        const auto mode=profile->aimMode;
        // Left-primary supports are stored in their own file, in the
        // original canonical (right-primary) coordinates. Runtime mirrors
        // them back when the weapon is held in the left hand.
        auto position=m.previewPosition;
        if(s.weaponHandLeft)position.x=-position.x;
        ok=kharvox::saveHeavySupportGrip(
            kharvox::runtimePathA(s.weaponHandLeft
                ?"two_hand_weapon_profiles_left_primary_saved.cfg"
                :"two_hand_weapon_profiles_saved.cfg"),
            m.weapon(),position,mode==TwoHandAimMode::SideGrip,
            std::clamp(profile->captureRadius,.08f,.40f),
            std::clamp(profile->maximumHandLineWeight,0.f,1.f),&why);
        if(ok){
            profile->supportPosition={position.x,position.y,
                position.z};
            profile->valid=true;
            s.twoHandLatched=false;
            s.supportVisualLocked=false;
        }
    }
    if(!ok){
        m.message="SAVE FAILED: "+why;
        log("[VR-HAND-MENU] "+m.message);
        return;
    }
    log(std::string("[VR-HAND-MENU] saved ")+m.weapon()+
        (alternate?" ALT FIRE":" NORMAL")+
        (m.rightHand?" RIGHT":" LEFT")+
        (primaryHand?" primary pose+rotation":" support position (grab rotation preserved)"));
    kharvox::queueCalibrationSaveCue(s.configSaveCues,m.rightHand,ok,GetTickCount64());
    m.mode=kharvox::VrHandMenuMode::Menu;
    m.message=std::string(m.rightHand?"RIGHT":"LEFT")+
        (primaryHand?" PRIMARY":" SUPPORT")+
        (alternate?" ALT FIRE":" NORMAL")
        +" saved for "+m.weapon();
    m.previewValid=false;
    m.primaryGripPreviewHeld=false;
    m.rotationEdit={};
    recordVrCalibrationStatus();
    if(m.weaponCalibrationStage==1||m.weaponCalibrationStage==4){
        const auto* support=twoHandCalibrationFor(kind);
        if((support&&support->valid)||alternate){m.weaponCalibrationStage=m.weaponCalibrationStage==1?2:5;m.rightHand=s.weaponHandLeft;startVrSelectedWeaponCalibration();return;}
    }
    const bool firstHand=m.calibrationStartedLeft
        ?m.weaponCalibrationStage>=4
        :m.weaponCalibrationStage==1||m.weaponCalibrationStage==2;
    if(m.weaponCalibrationStage&&firstHand){
        m.weaponCalibrationStage=3;
        m.supportPreviewHeld=false;
        m.mode=kharvox::VrHandMenuMode::Calibrating;
        return;
    }
    if(m.weaponCalibrationStage)finishVrWeaponSequence();
}
bool resetVrMenuSelectedWeapon(){
    auto& m=s.vrHandMenu;
    const auto rightFile=kharvox::runtimePathA("offhand_right_hand_positions.cfg");
    const auto leftFile=kharvox::runtimePathA("two_hand_weapon_profiles_saved.cfg");
    const auto leftPrimarySupport=kharvox::runtimePathA(
        "two_hand_weapon_profiles_left_primary_saved.cfg");
    std::string why;
    // Factory defaults are the user's uploaded in-headset calibration,
    // packaged as read-only files inside the native bridge. The active
    // editable profiles live in top-level UserData; only this weapon is
    // restored, while every other weapon's adjustments remain intact.
    const auto defaultPrimary=kharvox::runtimePathA(
        "CalibrationDefaults/offhand_right_hand_positions.cfg");
    const auto defaultRightSupport=kharvox::runtimePathA(
        "CalibrationDefaults/two_hand_weapon_profiles_saved.cfg");
    const auto defaultLeftSupport=kharvox::runtimePathA(
        "CalibrationDefaults/two_hand_weapon_profiles_left_primary_saved.cfg");
    if(!kharvox::restoreVrWeaponCalibrationDefault(
            rightFile,defaultPrimary,m.weapon(),true,&why)
        ||!kharvox::restoreVrWeaponCalibrationDefault(
            leftFile,defaultRightSupport,m.weapon(),false,&why)
        ||!kharvox::restoreVrWeaponCalibrationDefault(
            leftPrimarySupport,defaultLeftSupport,m.weapon(),false,&why)){
        m.message="RESET FAILED: "+why;
        log("[VR-HAND-MENU] "+m.message);
        return false;
    }
    s.rightGripSettings=kharvox::readRightHandGripSettings(rightFile);
    s.twoHandCalibrations={};
    s.twoHandCalibrationsLeft={};
    loadTwoHandCalibrations();
    s.primaryVisualLocked=false;s.supportVisualLocked=false;
    s.twoHandLatched=false;
    m.message=std::string("Your factory calibration restored: ")+m.weapon();
    recordVrCalibrationStatus(true);
    kharvox::queueMenuSaveCue(s.configSaveCues,true,true,GetTickCount64());
    log("[VR-HAND-MENU] "+m.message);
    return true;
}
void startVrSelectedWeaponCalibration(){
    auto& m=s.vrHandMenu;
    m.autoSelectingWeapon=false;
    m.pendingNativeSelection=0;
    m.mode=kharvox::VrHandMenuMode::Calibrating;
    m.calibratingWeaponLeft=s.weaponHandLeft;
    m.calibrationAlternateProfile=m.alternateProfile;
    m.previewValid=false;
    m.supportPreviewHeld=m.resetWeaponPending=false;
    m.primaryGripPreviewHeld=false;
    m.rotationEdit={}; // a fresh unsaved rotation draft for this calibration
    m.lastPreviewDiagnosticAt=0;
    m.fineTuneEdges={};
    m.fineTuneAccumulated={};
    // Saved fine-tune offsets now belong to the GUN, never the glove.
    // Primary rotation belongs to the weapon hand; default fine-tune
    // uses the joysticks without requiring Grip. Support placement unchanged.
    const bool calibratingPrimary=m.rightHand!=s.weaponHandLeft;
    m.primaryLateralBase=0.f;
    const auto kind=KharvoxWeaponKindFromKey(m.weapon());
    const bool alternate=m.alternateProfile
        &&(kind==KharvoxWeaponKind::GaussCannon
            ||kind==KharvoxWeaponKind::Chaingun);
    const std::string profileKey=m.weapon();
    const auto& seed=s.rightGripSettings;
    if(calibratingPrimary)
        m.fineTuneAccumulated=kharvox::primaryVisualFineTune(seed,profileKey,s.weaponHandLeft,alternate);
    const auto savedX=[&](const kharvox::RightHandGripSettings& settings,
                          float& value)->bool {
        if(s.weaponHandLeft){
            const auto key=profileKey
                +(alternate?(kind==KharvoxWeaponKind::GaussCannon
                    ?"_siege":"_turret"):"");
            const auto it=settings.leftVisualPositions.find(key);
            if(it==settings.leftVisualPositions.end())return false;
            value=it->second.x;
            return true;
        }
        kharvox::RightHandGripOffset position{};
        kharvox::RightHandGripRotation rotation{};
        if(!kharvox::rightPrimaryVisualPose(
                settings,profileKey,alternate,position,rotation))return false;
        value=position.x;
        return true;
    };
    if(!savedX(seed,m.primaryLateralBase))
        savedX(s.rightGripFactorySettings,m.primaryLateralBase);
    if(calibratingPrimary){
        kharvox::RightHandGripOffset position{};
        kharvox::RightHandGripRotation rotation{};
        const std::string key=profileKey+(alternate?(kind==KharvoxWeaponKind::GaussCannon?"_siege":"_turret"):"");
        const auto load=[&](const kharvox::RightHandGripSettings& settings){
            if(!s.weaponHandLeft)return kharvox::rightPrimaryVisualPose(settings,profileKey,alternate,position,rotation);
            auto pos=settings.leftVisualPositions.find(key);
            auto rot=settings.leftVisualRotations.find(key);
            if(pos==settings.leftVisualPositions.end()||rot==settings.leftVisualRotations.end())return false;
            position=pos->second;rotation=rot->second;return rotation.valid;
        };
        const bool loaded=load(seed)||load(s.rightGripFactorySettings);
        m.previewPosition=position;m.previewRotation=rotation;
        m.previewValid=loaded&&kharvox::validRightHandVisualPlacement(position)
            &&kharvox::validRightHandRotation(rotation);
        log("[VR-HAND-CAL] seed="+std::string(s.weaponHandLeft&&m.weaponCalibrationStage==4?"RIPSHIFT_LEFT_DEFAULT":"CURRENT_PROFILE")
            +" hand="+(s.weaponHandLeft?"LEFT":"RIGHT")+" valid="+std::to_string(m.previewValid));
    }
    s.primaryVisualLocked=false;
    s.supportVisualLocked=false;
    m.message.clear();
    log(std::string("[VR-MODS] calibrating ")+m.weapon()+
        (m.rightHand?" RIGHT":" LEFT")+
        (m.rightHand!=s.weaponHandLeft?" PRIMARY":" SUPPORT"));
}
void logVrCalibrationWheelYaw(const char* stage){
    const auto& m=s.vrHandMenu;
    std::ostringstream out;
    out<<"[CALIBRATION-WHEEL-YAW] "<<stage<<" bodyYaw="<<s.previousBodyYaw
        <<" bodyValid="<<s.previousBodyYawValid<<" headYaw="<<s.head.yaw
        <<" manualTurn="<<s.previousManualTurnActive<<" catchup="<<s.artificialTurnRemainingDegrees
        <<" bodyFollow="<<s.bodyFollowTurnX<<" acceptedYaw="<<s.acceptedPhysicalYaw
        <<" syntheticAxis="<<m.wheelAxisX<<","<<m.wheelAxisY
        <<" physicalStick="<<s.rightStick.x<<","<<s.rightStick.y
        <<" weapon="<<int(KharvoxWeaponCurrentKind())<<" left="<<m.calibrationWheelLeft;
    log(out.str());
}
#include "VrRadialMenu.inc"
int vrMenuRayTarget(float&pixelX,float&pixelY,XrTime displayTime,bool left=false){
    pixelX=pixelY=-1.f;menuAimRay={};menuAimRayMetres=0.f;
    const auto& controller=left?s.leftController:s.rightController;
    if(!controller.valid||!controller.positionTracked
        ||!s.trackingHeadPositionValid)return -1;
    // The compositor quad is head-locked in viewSpace at Z=-1.1 m.
    XrSpaceLocation view{XR_TYPE_SPACE_LOCATION};
    if(XR_FAILED(s.locateSpace(s.viewSpace,s.space,displayTime,&view))
        ||!(view.locationFlags&XR_SPACE_LOCATION_POSITION_VALID_BIT)
        ||!(view.locationFlags&XR_SPACE_LOCATION_ORIENTATION_VALID_BIT))return -1;
    const XrQuaternionf inverse=conjugate(normalizeQuaternion(view.pose.orientation));
    const auto origin=rotateVector(inverse,subtractVector(controller.position,view.pose.position));
    const auto aim=normalizeQuaternion(controller.orientation);
    const auto direction=rotateVector(inverse,rotateVector(aim,{0,0,-1}));
    if(direction.z>=-.05f)return -1;
    const float distance=(-1.1f-origin.z)/direction.z;
    if(distance<=.01f||distance>5.f)return -1;
    const float x=origin.x+direction.x*distance;
    const float y=origin.y+direction.y*distance;
    pixelX=(x/1.28f+.5f)*float(vrMenuW);
    pixelY=(.5f-y/.96f)*float(vrMenuH);
    if(pixelX<0||pixelX>=vrMenuW||pixelY<0||pixelY>=vrMenuH)return -1;
    const auto& pos=controller.position;
    if(!s.vrHandMenu.calibrationWheel&&!s.vrHandMenu.waitingForEquip){
        menuAimRay={{pos.x,pos.y,pos.z},{aim.x,aim.y,aim.z,aim.w},true};menuAimRayMetres=distance;
    }
    if(s.vrHandMenu.mode==kharvox::VrHandMenuMode::Menu&&s.vrHandMenu.page==kharvox::VrMenuPage::Mods)
        return radialTarget(pixelX,pixelY);
    if(s.vrHandMenu.page==kharvox::VrMenuPage::Records
        ||s.vrHandMenu.page==kharvox::VrMenuPage::DevRecords){
        const bool dev=s.vrHandMenu.page==kharvox::VrMenuPage::DevRecords;
        if(pixelY>=584&&pixelY<624&&pixelX>=38&&pixelX<493)return pixelX<263?96:97;
        if(pixelX>=800&&pixelX<927&&pixelY>=10&&pixelY<76)return 95;
        if(pixelX>=38&&pixelX<=493)
            for(int i=0;i<10;++i)
                if(pixelY>=136+i*45&&pixelY<176+i*45)
                    return 1000+s.vrHandMenu.recordsScroll+i;
        if(pixelX>=531&&pixelX<=915){
            if(!dev&&pixelY>=190&&pixelY<237)return 131; // SORT
            if(pixelY>=(dev?186:565)&&pixelY<(dev?244:618))
                return 130; // RESET or TRACK
        }
        return -1;
    }
    if(s.vrHandMenu.page==kharvox::VrMenuPage::DevStats){
        const bool dev=s.vrHandMenu.page==kharvox::VrMenuPage::DevStats;
        if(pixelX>=800&&pixelX<927&&pixelY>=10&&pixelY<76)return 95;
        if(pixelX>=38&&pixelX<=493)
            for(int i=0;i<10;++i)
                if(pixelY>=136+i*45&&pixelY<176+i*45)
                    return 1000+s.vrHandMenu.statsScroll+i;
        if(pixelX>=531&&pixelX<=915){
            if(!dev&&pixelY>=159&&pixelY<219)return 110;
            if(dev){
                if(pixelY>=186&&pixelY<244)return 130;
                if(pixelY>=397&&pixelY<455)return 131; // RESET ALL
                return -1;
            }
            const int cell=kharvox::statsMenuGridCellAt(
                static_cast<int>(pixelX),static_cast<int>(pixelY));
            if(cell>=0)return 130+cell;
        }
        return -1;
    }
    if(s.vrHandMenu.page==kharvox::VrMenuPage::Dev){
        if(pixelX>=800&&pixelX<927&&pixelY>=10&&pixelY<76)return 95;
        if(pixelX>=38&&pixelX<=493){
            for(int i=0;i<10;++i)
                if(pixelY>=136+i*45&&pixelY<176+i*45)
                    return 1000+s.vrHandMenu.modScroll+i;
        }
        if(pixelX>=531&&pixelX<=915){
            const auto list=activeVrMenuEntries();
            const int row=s.vrHandMenu.selectedRow;
            const bool expInline=row>=0&&row<static_cast<int>(list.size())
                &&s.vrHandMenu.page==kharvox::VrMenuPage::Dev
                &&(list[row].developer==kharvox::VrMenuDevOption::Experimental
                    ||list[row].developer==kharvox::VrMenuDevOption::GrappleAnchor
                    ||list[row].developer==kharvox::VrMenuDevOption::NativeInteractionResearch
);
            if(!s.vrHandMenu.navigation.settingsColumn&&!expInline){
                const bool devFolder=row>=0&&row<static_cast<int>(list.size())
                    &&list[row].developer==kharvox::VrMenuDevOption::Folder;
                if(!devFolder&&pixelY>=550&&pixelY<609)return 115;
                return -1;
            }
            if(row<0||row>=static_cast<int>(list.size()))return -1;
            const int optionY=186;
            const int optionCount=list[row].developer
                ==kharvox::VrMenuDevOption::GuidedTesting?5:list[row].developer==kharvox::VrMenuDevOption::NativeInteractionResearch?7:list[row].developer==kharvox::VrMenuDevOption::GrappleAnchor?4:expInline?3:list[row].mod==kharvox::VrLiveMod::Gunslinger?3:5;
            const bool grappleInline=list[row].developer==kharvox::VrMenuDevOption::GrappleAnchor;
            const bool researchInline=list[row].developer==kharvox::VrMenuDevOption::NativeInteractionResearch;
            const int optionStep=researchInline?52:grappleInline?60:expInline?91:68;
            const int optionHeight=researchInline?46:grappleInline?52:expInline?77:58;
            for(int i=0;i<optionCount;++i)
                if(pixelY>=optionY+i*optionStep
                    &&pixelY<optionY+optionHeight+i*optionStep)
                    return 130+i;
        }
        return -1;
    }
    if(s.vrHandMenu.page==kharvox::VrMenuPage::WeaponRecoil){
        if(pixelX>=800&&pixelX<927&&pixelY>=10&&pixelY<76)return 95;
        if(pixelX>=38&&pixelX<=493)
            for(int i=0;i<kharvox::kVrRecoilMenuEntryCount;++i)
                if(pixelY>=136+i*45&&pixelY<176+i*45)return 200+i;
        if(pixelX>=531&&pixelX<=915)
            for(int i=0;i<6;++i)
                if(pixelY>=157+i*63&&pixelY<211+i*63)return 140+i;
        return -1;
    }
    if(pixelX>=800&&pixelX<927&&pixelY>=10&&pixelY<76)return 95;
    if(pixelX>=38&&pixelX<=493){
        for(int i=0;i<10;++i)
            if(pixelY>=136+i*45&&pixelY<176+i*45)return i;
    }
    if(pixelX>=531&&pixelX<915&&pixelY>=306&&pixelY<355)return 32;
    if(pixelX>=531&&pixelX<915&&pixelY>=540&&pixelY<590
        &&KharvoxWeaponKindFromKey(s.vrHandMenu.weapon())
            ==KharvoxWeaponKind::GaussCannon)return 33;
    if(pixelX>=531&&pixelX<915){
        if(pixelY>=363&&pixelY<432)return 30;
        if(pixelY>=454&&pixelY<523)return 31;
    }
    return -1;
}
bool serviceVrHandMenuInput(XrTime displayTime,bool gameplay,bool leftClick,bool rightClick,
                            bool rightTrigger,bool leftTrigger,
                            bool primaryWeaponGripDown,bool supportGripDown,bool menuA,bool menuB,
                            bool openFromWelcome=false){
    const bool rightGrip=s.weaponHandLeft?supportGripDown:primaryWeaponGripDown;
    const bool leftGrip=s.weaponHandLeft?primaryWeaponGripDown:supportGripDown;
    auto& m=s.vrHandMenu;
    // Display-only: reflect actual physical Grip even if the live preview
    // is invalid. Never gate calibration mechanics or input on this flag.
    m.calibrationGripHeld=m.mode==kharvox::VrHandMenuMode::Calibrating
        &&m.liveCapture==kharvox::VrLiveCapture::None
        &&m.rightHand!=s.weaponHandLeft&&primaryWeaponGripDown;
    // A single 500 ms RIGHT-stick hold opens/closes this menu.
    // Ordinary RS taps still belong to Hand Focus.
    // The menu remains accessible when Weapon Grip Calibration is OFF,
    // otherwise the player could not turn the mod back on from VR.
    const auto shortcut=kharvox::vrMenuShortcutWithWelcome(
        m.shortcut,rightClick,GetTickCount64(),openFromWelcome);
    if(!gameplay){
        s.menuHoverCueState={};s.menuHoverCue={};
        if(m.calibrationWheel||m.waitingForEquip)KharvoxWeaponSetWheelPresentationActive(false);
        m.calibrationWheel=m.waitingForEquip=m.supportPreviewHeld=false;
        if(m.weaponCalibrationStage)finishVrWeaponSequence();
        m.hudDrag=m.hudSelection=-1;
        m.mode=kharvox::VrHandMenuMode::Closed;
        m.autoSelectingWeapon=false;
        m.pendingNativeSelection=0;
        m.liveCapture=kharvox::VrLiveCapture::None;
        m.combinedCrouch=false;
        m.crouchAwaitRelease=false;
        m.previewValid=false;
        m.shortcut={};
        m.rightTriggerPreviously=rightTrigger;
        m.menuAPreviously=menuA;
        m.menuBPreviously=menuB;
        m.consumeMenuFaceUntilRelease=false;
        m.consumeMenuTriggerUntilRelease=false;
        m.consumeDevGripUntilRelease=false;
        m.radialDevUnlock.clicks=0;
        return false;
    }
    if(shortcut==kharvox::VrMenuShortcutAction::Toggle){
        // Swallow the long click so it cannot trigger Hand Focus or melee.
        s.offHandStickInput.right={};
        if(m.mode==kharvox::VrHandMenuMode::Closed){
            if(!vrHandMenuSurface.ready){
                log("[VR-HAND-MENU] cannot open: XR overlay texture unavailable");
                return true;
            }
            m.mode=kharvox::VrHandMenuMode::Menu;
            m.radialCategory=m.suppressedCategory=m.radialResetRow=-1;
            m.radialPinned=false;
            m.radialDescription=-1;
            m.radialDescriptionFadeOut=0;
            m.combinedCrouch=false;
            m.gunslingerZoneDraftValid=false;
            m.gunslingerZoneMoving=false;
            m.gunslingerZonePreviewCue={};
            m.hovered=-1;m.cursorX=m.cursorY=-1.f;
            m.radialDevUnlock.clicks=0;
            m.supportPreviewHeld=m.calibrationWheel=m.waitingForEquip=false;
            m.page=kharvox::VrMenuPage::Mods;
            // A pending timed-run confirmation must never survive closing
            // and reopening the menu, even inside its six-second window.
            m.pendingStatsReset=-1;
            m.confirmDeathResetUntilMs=0;
            m.recoilReturnToDev=false;
            m.selectedMod=kharvox::VrLiveMod::WeaponGrip;
            m.selectedRow=0;
            m.modScroll=0;
            m.navigation={};
            m.liveCapture=kharvox::VrLiveCapture::None;
            m.autoSelectingWeapon=false;
            m.pendingNativeSelection=0;
            m.alternateProfile=false;
                // A left-holster draw means LEFT primary, not "LEFT support".
            m.rightHand=!s.weaponHandLeft;
            const auto equipped=KharvoxWeaponCurrentKind();
            for(int i=0;i<10;++i)
                if(KharvoxWeaponKindFromKey(kharvox::kVrCalibrationWeapons[i])
                    ==equipped)m.selectedWeapon=i;
            m.message.clear();
            refreshVrHandMenuSavedStatus();
            log("[VR-HAND-MENU] opened by 500 ms RIGHT-stick hold");
        }else{
            if(m.calibrationWheel||m.waitingForEquip)KharvoxWeaponSetWheelPresentationActive(false);
            m.calibrationWheel=m.waitingForEquip=m.supportPreviewHeld=false;
            const bool wasMenu=m.mode==kharvox::VrHandMenuMode::Menu;
            if(m.weaponCalibrationStage)finishVrWeaponSequence();
            m.mode=wasMenu
                ?kharvox::VrHandMenuMode::Closed:kharvox::VrHandMenuMode::Menu;
            m.previewValid=false;
            m.rotationEdit={};
            m.primaryGripPreviewHeld=false;
            m.liveCapture=kharvox::VrLiveCapture::None;
            m.combinedCrouch=false;
            m.crouchAwaitRelease=false;
            m.autoSelectingWeapon=false;
            m.pendingNativeSelection=0;
            s.primaryVisualLocked=false;
            s.supportVisualLocked=false;
            log("[VR-HAND-MENU] RIGHT-stick hold closes/cancels screen");
        }
        s.offHandStickInput={};
        m.rightTriggerPreviously=rightTrigger;
        m.leftTriggerPreviously=leftTrigger;
        m.menuAPreviously=menuA;
        m.menuBPreviously=menuB;
    }
    if(m.mode==kharvox::VrHandMenuMode::Closed){
        s.menuHoverCueState={};s.menuHoverCue={};
        m.rightTriggerPreviously=rightTrigger;
        m.leftTriggerPreviously=leftTrigger;
        m.radialDevUnlock.clicks=0;
        if(m.consumeDevGripUntilRelease){
            if(rightGrip)return true;
            m.consumeDevGripUntilRelease=false;
        }
        // Swallow held release edges after Stats closes the menu automatically.
        // Never let the confirmation trigger fire a shot on the next frame.
        if(m.consumeMenuTriggerUntilRelease){
            if(!rightTrigger&&!leftTrigger){
                m.consumeMenuTriggerUntilRelease=false;
            }else return true;
        }
        // Keep a menu-closing B/A held press out of normal jump/crouch.
        if(m.consumeMenuFaceUntilRelease){
            if(!menuA&&!menuB)m.consumeMenuFaceUntilRelease=false;
            else return true;
        }
        // Closing during a held right click must also swallow release.
        if(m.shortcut.triggered
            ||shortcut==kharvox::VrMenuShortcutAction::ConsumedRelease){
            s.offHandStickInput.right={};
            return true;
        }
        return false;
    }
    const bool triggerEdge=rightTrigger&&!m.rightTriggerPreviously;
    const bool leftTriggerEdge=leftTrigger&&!m.leftTriggerPreviously;
    m.rightTriggerPreviously=rightTrigger;m.leftTriggerPreviously=leftTrigger;
    // Sample real menu inputs on EVERY frame, including live Gunslinger
    // captures. The DEV monitor renders each bit separately and never
    // rewires gameplay bindings or commits this information to UserData.
    m.devInput={rightTrigger,leftTrigger,leftClick,rightClick,
        primaryWeaponGripDown,menuA,menuB,s.leftStick.x,s.leftStick.y};
    const bool aEdge=kharvox::vrMenuButtonEdge(menuA,m.menuAPreviously);
    const bool bEdge=kharvox::vrMenuButtonEdge(menuB,m.menuBPreviously);
    // A fresh deliberate action can produce the same feedback again.
    if(bEdge||aEdge||triggerEdge||leftTriggerEdge)m.messageNotice={};
    // B cancels capture without saving. From weapon profiles B returns to
    // Mods; from the top-level Mods page it closes the overlay.
    if(bEdge&&m.page==kharvox::VrMenuPage::Mods&&m.radialResetRow>=0){
        m.radialResetRow=-1;
        m.message.clear();
    }else if(bEdge){
        m.radialDevUnlock.clicks=0;
        m.supportPreviewHeld=m.resetWeaponPending=false;
        if(m.calibrationWheel||m.waitingForEquip)KharvoxWeaponSetWheelPresentationActive(false);
        m.calibrationWheel=m.waitingForEquip=false;
        const bool wasCalibrating=m.mode==kharvox::VrHandMenuMode::Calibrating;
        if(m.weaponCalibrationStage)finishVrWeaponSequence();
        m.liveCapture=kharvox::VrLiveCapture::None;
        const bool cancelledCombined=m.combinedCrouch;
        m.combinedCrouch=false;
        m.gunslingerZoneDraftValid=false;
        m.gunslingerZoneMoving=false;
        m.autoSelectingWeapon=false;
        m.pendingNativeSelection=0;
        m.previewValid=false;
        m.primaryGripPreviewHeld=false;
        m.rotationEdit={};
        s.primaryVisualLocked=false;
        s.supportVisualLocked=false;
        if(wasCalibrating){
            m.mode=kharvox::VrHandMenuMode::Menu;
            // Gunslinger/Crouch capture returns to Mods; weapon grip capture
            // returns to the selected weapon. Neither cancellation saves.
            m.page=kharvox::VrMenuPage::Mods;
            m.message=cancelledCombined?"Crouch calibration cancelled. Previous heights retained.":"Current draft discarded. Earlier confirmed stages retained.";
        }else if(m.page==kharvox::VrMenuPage::Records
            ||m.page==kharvox::VrMenuPage::DevRecords){
            const bool dev=m.page==kharvox::VrMenuPage::DevRecords;
            m.page=dev?kharvox::VrMenuPage::Dev:kharvox::VrMenuPage::Mods;
            m.navigation={};
            m.pendingStatsReset=-1;
            m.confirmDeathResetUntilMs=0;
            m.message="Back to "+std::string(dev?"DEV tools.":"Stats.");
        }else if(m.page==kharvox::VrMenuPage::Stats
            ||m.page==kharvox::VrMenuPage::DevStats){
            const bool dev=m.page==kharvox::VrMenuPage::DevStats;
            if(!dev){m.mode=kharvox::VrHandMenuMode::Closed;m.consumeMenuFaceUntilRelease=true;}
            else if(m.navigation.settingsColumn){
                m.navigation.settingsColumn=false;
                m.navigation.actionIndex=0;
                m.message="Back to Stats options.";
            }else{
                m.page=dev?kharvox::VrMenuPage::Dev:kharvox::VrMenuPage::Mods;
                m.navigation={};
                m.message=dev?"Back to developer tools.":"Back to mod settings.";
            }
        }else if(m.page==kharvox::VrMenuPage::WeaponGrip
            ||m.page==kharvox::VrMenuPage::WeaponRecoil){
            const bool backToDev=m.page==kharvox::VrMenuPage::WeaponRecoil
                &&m.recoilReturnToDev;
            m.page=backToDev?kharvox::VrMenuPage::Dev
                :kharvox::VrMenuPage::Mods;
            m.navigation={};
            m.message=backToDev?"Back to DEV tools.":"Back to VR Mod Settings.";
        }else if(m.page==kharvox::VrMenuPage::Dev){
            m.page=kharvox::VrMenuPage::Mods;
            m.selectedRow=0;
            m.modScroll=0;
            m.navigation={};
            m.message="Back to MOD SETTINGS.";
        }else if(m.page==kharvox::VrMenuPage::Mods){
            m.mode=kharvox::VrHandMenuMode::Closed;m.consumeMenuFaceUntilRelease=true;
        }else if(m.navigation.settingsColumn){
            // Return to the mod list before closing the main overlay.
            m.navigation.settingsColumn=false;
            m.navigation.actionIndex=0;
            m.message="Back to mod list.";
        }else{
            m.mode=kharvox::VrHandMenuMode::Closed;
            m.consumeMenuFaceUntilRelease=true;
        }
    }
    if(!bEdge&&m.mode==kharvox::VrHandMenuMode::Calibrating){
        if(m.liveCapture!=kharvox::VrLiveCapture::None){
            updateVrLiveZoneDraft();
            const bool crouch=m.liveCapture==kharvox::VrLiveCapture::CrouchStanding
                ||m.liveCapture==kharvox::VrLiveCapture::CrouchLowered;
            if(crouch&&m.crouchAwaitRelease){
                if(!rightTrigger&&!menuA)m.crouchAwaitRelease=false;
            }else if(triggerEdge||(crouch&&aEdge))captureVrLivePosition();
        }else if(m.weaponCalibrationStage&&([&]{
            vrMenuRayTarget(m.cursorX,m.cursorY,displayTime);
            const auto kind=KharvoxWeaponCurrentKind();
            const int action=kharvox::weaponGuideAction(m.cursorX,m.cursorY,
                kind==KharvoxWeaponKind::GaussCannon||kind==KharvoxWeaponKind::Chaingun);
            const int target=m.weaponCalibrationStage==3?-1:action;
            const int context=90000+int(m.weaponCalibrationStage);
            const auto hoverNow=GetTickCount64();
            if(kharvox::menuHoverCueEntered(s.menuHoverCueState,context,target,hoverNow)
                &&!triggerEdge&&!leftTriggerEdge&&!aEdge)
                kharvox::queueMenuHoverCue(s.menuHoverCue,hoverNow);
            if(!triggerEdge||target<0)return false;
            kharvox::queueMenuButtonCue(s.configSaveCues,true,true,hoverNow);
            if(action==2){
                m.alternateProfile=!m.alternateProfile;
                startVrSelectedWeaponCalibration();
            }else if(action==1)openCalibrationWheel();
            else if(m.resetWeaponPending){
                m.resetWeaponPending=false;
                if(resetVrMenuSelectedWeapon()){
                    startVrSelectedWeaponCalibration();
                    m.message="Factory grip restored.";
                }
            }else m.resetWeaponPending=true;
            return true;
        })()){
            m.consumeMenuTriggerUntilRelease=true;
        }else if(m.weaponCalibrationStage==3){
            const bool otherLeft=!m.calibrationStartedLeft;
            if(otherLeft?leftTriggerEdge:triggerEdge){
                m.weaponCalibrationStage=otherLeft?4:1;
                selectPhysicalWeaponHand(otherLeft,"calibration other primary");
                m.rightHand=!otherLeft;
                startVrSelectedWeaponCalibration();
            }else if(leftTriggerEdge||triggerEdge)finishVrWeaponSequence();
        }else{
            const bool primary=m.rightHand!=s.weaponHandLeft;
            const bool grip=primary?primaryWeaponGripDown:
                supportGripDown;
            updateVrMenuPreview(grip);
            serviceVrWeaponFineTune(primaryWeaponGripDown);
            m.supportPreviewHeld=!primary&&grip&&m.previewValid;
            if((m.rightHand?triggerEdge:leftTriggerEdge)&&(primary||m.supportPreviewHeld)){
                m.consumeMenuTriggerUntilRelease=true;saveVrHandMenuCalibration();
            }
        }
    }else if(!bEdge&&m.mode==kharvox::VrHandMenuMode::Menu){
        if(m.calibrationWheel&&!m.waitingForEquip){
            if(triggerEdge||leftTriggerEdge)
                m.calibrationWheelLeft=leftTriggerEdge&&!triggerEdge;
            else if(leftGrip!=rightGrip)
                m.calibrationWheelLeft=leftGrip;
        }
        m.hovered=vrMenuRayTarget(m.cursorX,m.cursorY,displayTime,
            m.calibrationWheel&&m.calibrationWheelLeft);
        const auto rows=m.page==kharvox::VrMenuPage::Dev?activeVrMenuEntries():std::vector<VrLiveMenuEntry>{};
        // Use the same hit targets as input dispatch. Normal flyout rows
        // count as one item, while minus/plus stepper controls are separate.
        int hoverTarget=m.hovered;
        int hoverContext=static_cast<int>(m.page)*10000;
        if(m.calibrationWheel||m.waitingForEquip||m.hudDrag>=0){
            hoverTarget=-1;
        }else if(m.page==kharvox::VrMenuPage::Mods&&hoverTarget>=kharvox::radialRowTarget){
            hoverContext+=m.radialCategory+1;
            hoverTarget=radialInteractiveTarget(hoverTarget);
        }else if(m.page==kharvox::VrMenuPage::Stats){
            hoverTarget=hudTarget(int(m.cursorX),int(m.cursorY));
        }else if(m.page==kharvox::VrMenuPage::Dev){
            if(hoverTarget>=1000&&hoverTarget<1000+100&&hoverTarget-1000>=int(rows.size()))
                hoverTarget=-1;
            if(hoverTarget>=110&&hoverTarget<200)hoverContext+=m.selectedRow+1;
        }
        const auto hoverNow=GetTickCount64();
        if(kharvox::menuHoverCueEntered(s.menuHoverCueState,hoverContext,hoverTarget,hoverNow)
            &&!triggerEdge&&!leftTriggerEdge&&!aEdge)
            kharvox::queueMenuHoverCue(s.menuHoverCue,hoverNow);
        // A valid on-screen target clicks once, before dispatch. A successful
        // physical save in the same frame replaces this with its double buzz.
        bool buttonActivated=triggerEdge&&hoverTarget>=0;
         if(buttonActivated&&(m.page==kharvox::VrMenuPage::Records
            ||m.page==kharvox::VrMenuPage::DevRecords)&&hoverTarget>=1000){
            const int count=int(kharvox::kCampaignMissions.size())
                +(m.page==kharvox::VrMenuPage::DevRecords?1:0);
            buttonActivated=hoverTarget<1000+count;
        }
        if(buttonActivated&&m.page==kharvox::VrMenuPage::DevStats&&hoverTarget>=1000)
            buttonActivated=hoverTarget<1004;
        kharvox::queueMenuButtonCue(s.configSaveCues,true,buttonActivated,hoverNow);
        if(m.calibrationWheel||m.waitingForEquip){
            const bool pointing=m.cursorX>=0&&m.cursorY>=0
                &&m.cursorX<vrMenuW&&m.cursorY<vrMenuH;
            if(m.calibrationWheel&&!m.waitingForEquip&&(triggerEdge||leftTriggerEdge)&&pointing){
                kharvox::queueMenuButtonCue(s.configSaveCues,!m.calibrationWheelLeft,true,GetTickCount64());
                m.wheelAxisX=(m.cursorX-480.f)/190.f;
                m.wheelAxisY=(360.f-m.cursorY)/190.f;
                m.wheelPreviousWeapon=int(KharvoxWeaponCurrentKind());
                m.waitingForEquip=true;
                m.wheelReleasedAt=GetTickCount64();
                logVrCalibrationWheelYaw("confirm");
            }else if(m.waitingForEquip){
                if(m.calibrationWheel){
                    m.calibrationWheel=false;
                    m.wheelReleasedAt=GetTickCount64();
                    logVrCalibrationWheelYaw("release");
                }
                const auto elapsed=GetTickCount64()-m.wheelReleasedAt;
                const auto kind=KharvoxWeaponCurrentKind();
                if(elapsed>=400&&(int(kind)!=m.wheelPreviousWeapon||elapsed>=1200)){
                    for(int i=0;i<int(kharvox::kVrCalibrationWeapons.size());++i){
                        if(KharvoxWeaponKindFromKey(kharvox::kVrCalibrationWeapons[i])!=kind)continue;
                        m.waitingForEquip=false;
                        m.selectedWeapon=i;
                        m.alternateProfile=false;
                        logVrCalibrationWheelYaw("equip");
                        beginVrWeaponSequence(m.calibrationWheelLeft);
                        logVrCalibrationWheelYaw("enter calibration");
                        break;
                    }
                }
                if(m.waitingForEquip&&elapsed>=1800){
                    m.waitingForEquip=false;
                    m.page=kharvox::VrMenuPage::Mods;
                    m.message="No supported weapon equipped.";
                }
            }
        }else if(m.page==kharvox::VrMenuPage::Mods){
            if(rightGrip&&m.hovered==kharvox::radialCenter)
                m.consumeDevGripUntilRelease=true;
            radialInput(m.hovered,triggerEdge,rightGrip);
        }else if(m.page==kharvox::VrMenuPage::Stats){
            hudInput(int(m.cursorX),int(m.cursorY),triggerEdge,rightTrigger);
        }else if(m.page==kharvox::VrMenuPage::Records
            ||m.page==kharvox::VrMenuPage::DevRecords){
            const bool dev=m.page==kharvox::VrMenuPage::DevRecords;
            const int count=static_cast<int>(
                kharvox::kCampaignMissions.size())+(dev?1:0);
            const int previousRecord=m.recordSelected;
            if(dev)kharvox::vrMenuNavigateStick(m.navigation,
                s.leftStick.x,s.leftStick.y,
                count,m.recordSelected,1);
            m.recordSelected=std::clamp(m.recordSelected,0,std::max(0,count-1));
            if(previousRecord!=m.recordSelected){
                // A confirmation belongs to one exact mission/reset choice.
                // Scrolling to another row, even briefly, cancels it.
                m.pendingStatsReset=-1;
                m.confirmDeathResetUntilMs=0;
            }
            m.recordsScroll=std::clamp(m.recordsScroll,0,std::max(0,count-10));
            if(m.recordSelected<m.recordsScroll)m.recordsScroll=m.recordSelected;
            if(m.recordSelected>=m.recordsScroll+10)
                m.recordsScroll=m.recordSelected-9;
            const int pointed=triggerEdge?m.hovered:-1;
            const int chosen=pointed>=0?pointed:aEdge
                ?(m.navigation.settingsColumn
                    ?(dev||m.navigation.actionIndex==1?130:131)
                    :1000+m.recordSelected):-1;
            if(chosen==95){
                m.page=dev?kharvox::VrMenuPage::Dev:kharvox::VrMenuPage::Mods;
                m.navigation={};m.pendingStatsReset=-1;
                m.confirmDeathResetUntilMs=0;
            }else if(chosen==96||chosen==97){
                m.recordsScroll=std::clamp(m.recordsScroll+(chosen==96?-10:10),0,std::max(0,count-10));
                m.recordSelected=m.recordsScroll;m.pendingStatsReset=-1;m.confirmDeathResetUntilMs=0;
            }else if(chosen>=1000&&chosen<1000+count){
                m.recordSelected=chosen-1000;
                m.navigation.actionIndex=0;
                m.navigation.settingsColumn=aEdge;
                m.pendingStatsReset=-1;
                m.confirmDeathResetUntilMs=0;
            }else if(!dev&&chosen==131){
                m.recordSortDeaths=!m.recordSortDeaths;
                m.message=m.recordSortDeaths
                    ?"Top five runs ranked by fewest deaths."
                    :"Top five runs ranked by fastest time.";
            }else if(chosen==130){
                if(dev)handleVrLevelRecordReset(130);
                else handleVrLevelRecordTrack(m.recordSelected);
            }
            // Choosing/browsing a mission updates the PB displayed on HUD,
            // without starting a competitive attempt or changing live counters.
            if(!dev&&(previousRecord!=m.recordSelected
                ||(chosen>=1000&&chosen<1000+count)||chosen==130))
                m.hudRecordSelected=m.recordSelected;
        }else if(m.page==kharvox::VrMenuPage::DevStats){
            const bool dev=true;
            const int previous=m.statsSelected;
            if(dev)kharvox::vrMenuNavigateStick(m.navigation,
                m.navigation.settingsColumn?s.leftStick.x:0.f,
                s.leftStick.y,4,m.statsSelected,2);
            else kharvox::vrStatsNavigateStick(m.navigation,
                s.leftStick.x,s.leftStick.y,m.statsSelected,
                kharvox::statsGridCellFromPosition(
                    kharvox::vrStatsCorner(s.vrLiveSettings,m.statsSelected)),
                kharvox::kVrStatsItemCount+1);
            const int statsCount=dev?4:kharvox::kVrStatsItemCount+1;
            m.statsSelected=std::clamp(m.statsSelected,0,statsCount-1);
            m.statsScroll=std::clamp(m.statsScroll,0,std::max(0,statsCount-10));
            if(m.statsSelected<m.statsScroll)m.statsScroll=m.statsSelected;
            if(m.statsSelected>=m.statsScroll+10)
                m.statsScroll=m.statsSelected-9;
            if(previous!=m.statsSelected){
                m.navigation.actionIndex=0;
                m.pendingStatsReset=-1;
                m.confirmDeathResetUntilMs=0;
            }
            const int pointed=triggerEdge?m.hovered:-1;
            const int chosen=pointed>=0?pointed:
                aEdge?(m.navigation.settingsColumn
                    ?dev?130+m.navigation.actionIndex:m.navigation.actionIndex==0
                        ?110:129+m.navigation.actionIndex
                    :1000+m.statsSelected):-1;
            if(pointed>=0)m.navigation.usingStick=false;
            if(chosen==95){
                m.page=dev?kharvox::VrMenuPage::Dev:kharvox::VrMenuPage::Mods;
                m.navigation={};
                m.pendingStatsReset=-1;
                m.confirmDeathResetUntilMs=0;
            }else if(!dev&&chosen==1000+kharvox::kVrStatsItemCount){
                m.page=kharvox::VrMenuPage::Records;
                m.recordSelected=0;m.recordsScroll=0;m.navigation={};
                m.message="Saved level personal bests. B returns to Stats.";
            }else if(chosen>=1000&&chosen<1000+statsCount){
                m.statsSelected=chosen-1000;
                m.statsScroll=std::clamp(m.statsScroll,0,std::max(0,statsCount-10));
                m.navigation.actionIndex=0;
                m.pendingStatsReset=-1;
                m.confirmDeathResetUntilMs=0;
                m.navigation.settingsColumn=aEdge;
                m.message=std::string(dev?"Reset ":"Configure ")
                    +kharvox::vrStatsLabel(m.statsSelected);
            }else if((m.navigation.settingsColumn||pointed>=0)
                &&(chosen==110||(chosen>=130
                    &&chosen<130+kharvox::kStatsGridCells))){
                handleVrStatsMenuAction(chosen,dev);
            }
        }else if(m.page==kharvox::VrMenuPage::Dev){
            const int n=static_cast<int>(rows.size());
            if(n>0)m.selectedRow=std::clamp(m.selectedRow,0,n-1);
            const auto optionCount=[&](const VrLiveMenuEntry& entry)->int{
                using D=kharvox::VrMenuDevOption;
                return entry.developer==D::Experimental?3:
                    entry.developer==D::GuidedTesting?5:
                    entry.developer==D::NativeInteractionResearch?7:
                    entry.developer==D::GrappleAnchor?4:1;
            };
            const auto before=m.selectedRow;
            const int previousOptions=n>0?optionCount(rows[m.selectedRow]):0;
            // Overview is browse-only. Stick horizontal cannot silently
            // open a config or make A adjust a value. A / CONFIGURE enters
            // the right-side options; B returns to the overview.
            const bool experimentalSelected=n>0
                &&(rows[m.selectedRow].developer==kharvox::VrMenuDevOption::Experimental
                    ||rows[m.selectedRow].developer==kharvox::VrMenuDevOption::GrappleAnchor
                    ||rows[m.selectedRow].developer==kharvox::VrMenuDevOption::NativeInteractionResearch
);
            kharvox::vrMenuNavigateStick(m.navigation,
                (m.navigation.settingsColumn||experimentalSelected)
                    ?s.leftStick.x:0.f,
                s.leftStick.y,n,m.selectedRow,previousOptions);
            if(n>0){
                m.selectedMod=rows[m.selectedRow].mod;
                if(before!=m.selectedRow)m.navigation.actionIndex=0;
                const int options=optionCount(rows[m.selectedRow]);
                if(options>0)m.navigation.actionIndex=std::clamp(
                    m.navigation.actionIndex,0,options-1);
                else m.navigation.actionIndex=0;
                m.modScroll=std::clamp(m.modScroll,0,std::max(0,n-10));
                if(m.selectedRow<m.modScroll)m.modScroll=m.selectedRow;
                if(m.selectedRow>=m.modScroll+10)m.modScroll=m.selectedRow-9;
            }
            const int pointed=triggerEdge?m.hovered:-1;
            const int chosen=pointed>=0?pointed:
                aEdge?kharvox::vrMenuModsGamepadTarget(m.navigation,
                    m.selectedRow,n,n>0?optionCount(rows[m.selectedRow]):0,
                    false):-1;
            if(pointed>=0)m.navigation.usingStick=false;
            const auto openDevEntry=[&](const VrLiveMenuEntry& entry)->bool {
                using D=kharvox::VrMenuDevOption;
                if(entry.developer==D::None)return false;
                if(entry.developer==D::Folder){
                    m.page=kharvox::VrMenuPage::Dev;
                    m.selectedRow=0;m.modScroll=0;m.navigation={};
                    m.message="DEV tools. B returns to MOD SETTINGS.";
                }else if(entry.developer==D::LevelRecordsReset){
                    m.page=kharvox::VrMenuPage::DevRecords;
                    m.recordSelected=0;m.recordsScroll=0;
                    m.navigation={};m.pendingStatsReset=-1;
                    m.confirmDeathResetUntilMs=0;
                    m.message="Choose current or all level records. Confirm twice.";
                }else if(entry.developer==D::StatsReset){
                    m.page=kharvox::VrMenuPage::DevStats;
                    m.statsSelected=0;
                    m.navigation={};
                    m.pendingStatsReset=-1;
                    m.message="Choose a stat to reset; confirm twice.";
                }else if(entry.developer==D::WeaponStability){
                    m.page=kharvox::VrMenuPage::WeaponRecoil;
                    m.recoilReturnToDev=true;
                    m.navigation={};
                    m.message="Tune weapon stability; B returns to DEV.";
                }else if(entry.developer==D::GrappleAnchor){
                    m.navigation.settingsColumn=true;
                    m.navigation.actionIndex=0;
                    m.message="Grapple Grab: single universal grab path.";
                }else if(entry.developer==D::Experimental){
                    m.navigation.settingsColumn=true;
                    m.navigation.actionIndex=0;
                    m.message="Object Discovery settings: select a toggle or slider.";
                }else{
                    m.navigation.settingsColumn=true;
                    m.navigation.actionIndex=0;
                    m.message="Press A or trigger on the tool switch.";
                }
                return true;
            };
            if(chosen>=1000&&chosen<1000+n){
                // Browse changes highlight only: the right panel is a
                // gameplay description until A/CONFIGURE is explicitly
                // pressed, never an accidental ON/OFF or capture.
                const int newRow=chosen-1000;
                m.selectedRow=newRow;
                m.selectedMod=rows[newRow].mod;
                m.navigation.actionIndex=0;
                // Browse by pointer; A or OPEN / TOGGLE enters the selected DEV tool.
                if(aEdge){
                    openDevEntry(rows[newRow]);
                }else{
                    m.navigation.settingsColumn=false;
                    m.message=rows[newRow].developer
                        ==kharvox::VrMenuDevOption::Experimental
                        ?"Object Discovery settings on right."
                        :rows[newRow].developer==kharvox::VrMenuDevOption::GrappleAnchor
                            ?"Grapple Anchor switch on right."
                        :"Browse mod details. A opens configuration.";
                }
            }else if(chosen==115&&n>0){
                m.selectedMod=rows[m.selectedRow].mod;
                openDevEntry(rows[m.selectedRow]);
            }else if(chosen==95){
                m.page=kharvox::VrMenuPage::Mods;
                m.selectedRow=0;
                m.modScroll=0;m.navigation={};
                m.message="Back to MOD SETTINGS.";
            }else if(chosen>=130&&chosen<=136&&n>0
                &&(m.navigation.settingsColumn
                    ||rows[m.selectedRow].developer==kharvox::VrMenuDevOption::Experimental
                    ||rows[m.selectedRow].developer==kharvox::VrMenuDevOption::GrappleAnchor
                    ||rows[m.selectedRow].developer==kharvox::VrMenuDevOption::NativeInteractionResearch
)){
                using D=kharvox::VrMenuDevOption;
                const auto kind=rows[m.selectedRow].developer;
                if(kind==D::GaussTwoHandFire){
                    const bool old=s.vrLiveSettings.gaussRequireTwoHandFire;
                    s.vrLiveSettings.gaussRequireTwoHandFire=!old;
                    if(!saveVrLiveSettings())s.vrLiveSettings.gaussRequireTwoHandFire=old;
                    else m.message=s.vrLiveSettings.gaussRequireTwoHandFire?"Gauss 2-hand fire ON.":"Gauss 2-hand fire OFF.";
                }else if(kind==D::WeaponAlignment){
                    m.devWeaponAlignment=!m.devWeaponAlignment;
                    m.message=m.devWeaponAlignment?"Weapon anchor readout ON.":"Weapon anchor readout OFF.";
                }else if(kind==D::HandFocus){
                    s.customMods.handFocusedRs=!s.customMods.handFocusedRs;
                    m.message=s.customMods.handFocusedRs
                        ?"Hand Focus enabled for this session."
                        :"Head Focus enabled for this session.";
                }else if(kind==D::ControllerMonitor){
                    m.devControllerInputs=!m.devControllerInputs;
                    m.message=m.devControllerInputs?"Gunslinger input monitor ON.":"Gunslinger input monitor OFF.";
                }else if(kind==D::GuidedTesting&&chosen<=134){
                    if(chosen==130){m.guidedTestingEnabled=!m.guidedTestingEnabled;m.message=m.guidedTestingEnabled?"Guided Testing ON":"Guided Testing OFF";}
                    else{m.guidedCommand=static_cast<unsigned>(chosen-130);m.message="Guide command queued; resume gameplay.";}
                    log("[GUIDED-TEST] menu "+m.message);
                }else if(kind==D::GrappleAnchor&&chosen==130){
                    const auto previous=m.grappleAnchor;
                    m.grappleAnchor.enabled=m.dynamicSurfaceOffset||!m.grappleAnchor.enabled;
                    if(!kharvox::grappleAnchorSave(kharvox::runtimePathA(
                        "grapple_anchor.cfg"),m.grappleAnchor)){
                        m.grappleAnchor=previous;
                        m.message="Grapple Anchor: UserData save failed.";
                    }else{
                        kharvox::queueMenuSaveCue(s.configSaveCues,true,true,GetTickCount64());
                        if(m.grappleAnchor.enabled){m.pistolGrappleEnabled=false;m.dynamicSurfaceOffset=false;}
                        m.message=std::string("Grapple Grab ")
                            +(m.grappleAnchor.enabled?"ON":"OFF")
                            +" (visual-mesh hit not yet verified)";
                        log("[GRAPPLE-ANCHOR] "+m.message);
                    }
                }else if(kind==D::GrappleAnchor&&chosen==131){
                    if(m.dynamicSurfaceOffset)m.message="Surface Offset is kick-only; melee disabled.";
                    else if(m.pistolGrappleEnabled)m.message="Pistol Grapple has its own automatic melee.";
                    else{
                        m.objectMeleeActivation=!m.objectMeleeActivation;
                        m.message=m.objectMeleeActivation?"Melee activation ON.":"Melee activation OFF.";
                        log("[MELEE] "+m.message);
                    }
                }else if(kind==D::GrappleAnchor&&chosen==132){
                    if(m.pistolGrappleEnabled){
                        m.pistolGrappleEnabled=false;
                        m.message="Pistol Grapple OFF.";
                    }else{
                        const auto previous=m.grappleAnchor;
                        m.grappleAnchor.enabled=false;
                        if(!kharvox::grappleAnchorSave(kharvox::runtimePathA(
                            "grapple_anchor.cfg"),m.grappleAnchor)){
                            m.grappleAnchor=previous;
                            m.message="Pistol Grapple: unable to save original OFF.";
                        }else{
                            kharvox::queueMenuSaveCue(s.configSaveCues,true,true,GetTickCount64());
                            m.pistolGrappleEnabled=true;m.dynamicSurfaceOffset=false;
                            m.message="Pistol Grapple ON; original OFF.";
                        }
                    }
                    log("[PISTOL-GRAPPLE] "+m.message);
                }else if(kind==D::GrappleAnchor&&chosen==133){
                    m.dynamicSurfaceOffset=!m.dynamicSurfaceOffset;
                    if(m.dynamicSurfaceOffset)m.pistolGrappleEnabled=false;
                    m.message=m.dynamicSurfaceOffset
                        ?"Dynamic Surface Offset ON; kick first, no melee."
                        :"Dynamic Surface Offset OFF; original preference restored.";
                    log("[SURFACE-GRAPPLE] "+m.message);
                }else if(kind==D::NativeInteractionResearch){
                    const int index=chosen-130;
                    if(!index){m.nativeResearchTestPage=!m.nativeResearchTestPage;m.message=m.nativeResearchTestPage?"TEST: held movement comparison only.":"COLLECT: select groups, then close DEV.";}
                    else{
                        const unsigned bit=1u<<(index-1);
                        const unsigned ready=KharvoxCameraNativeResearchReady();
                        if(m.nativeResearchTestPage){
                            if(m.nativeResearchTestMask&bit)m.nativeResearchTestMask&=~bit;
                            else if(bit==8u)m.nativeResearchTestMask|=bit;
                            else{m.message="No verified functional test yet; collection only.";}
                        }else m.nativeResearchMask^=bit;
                        log("[NATIVE-RESEARCH] collect="+std::to_string(m.nativeResearchMask)+" test="+std::to_string(m.nativeResearchTestMask)+" ready="+std::to_string(ready));
                    }
                }else if(kind==D::Experimental&&chosen<=132){
                    const auto previous=m.objectDiscovery;
                    const int option=chosen-130;
                    if(option==0){
                        m.objectDiscovery.enabled=!m.objectDiscovery.enabled;
                    }else if(option==2){
                        m.objectDiscovery.grabReachLaserEnabled=
                            !m.objectDiscovery.grabReachLaserEnabled;
                    }else{
                        const bool pointerSet=pointed==chosen
                            &&std::isfinite(m.cursorX);
                        // A advances by one step; the pointer snaps to
                        // the nearest tick on the displayed slider.
                        const float fraction=pointerSet?std::clamp(
                            (m.cursorX-556.f)/338.f,0.f,1.f):-1.f;
                        const float next=pointerSet?1.f+49.f*fraction:m.objectDiscovery.scanRadiusMetres+1.f;
                        m.objectDiscovery.scanRadiusMetres=next>50.f?1.f:next;
                    }
                    m.objectDiscovery=kharvox::sanitizedObjectDiscoverySettings(
                        m.objectDiscovery);
                    if(!kharvox::objectDiscoverySave(kharvox::runtimePathA(
                        "object_discovery.cfg"),m.objectDiscovery)){
                        m.objectDiscovery=previous;
                        m.message="Object Discovery: UserData save failed.";
                    }else{
                        kharvox::queueMenuSaveCue(s.configSaveCues,true,true,GetTickCount64());
                        m.message=std::string("Discovery ")
                            +(m.objectDiscovery.enabled?"ON":"OFF")
                            +" scan="+std::to_string(static_cast<int>(
                                m.objectDiscovery.scanRadiusMetres))
                            +"m reach=2m laser="
                            +(m.objectDiscovery.grabReachLaserEnabled?"ON":"OFF");
                    }
                }
            }
        }else if(m.page==kharvox::VrMenuPage::WeaponRecoil){
            const int previous=m.selectedRecoilWeapon;
            kharvox::vrMenuNavigateStick(m.navigation,
                s.leftStick.x,s.leftStick.y,
                kharvox::kVrRecoilMenuEntryCount,
                m.selectedRecoilWeapon,6);
            if(previous!=m.selectedRecoilWeapon)m.navigation.actionIndex=0;
            m.selectedRecoilWeapon=std::clamp(m.selectedRecoilWeapon,0,
                kharvox::kVrRecoilMenuEntryCount-1);
            const int pointed=triggerEdge?m.hovered:-1;
            const int chosen=pointed>=0?pointed:
                aEdge?kharvox::vrMenuRecoilGamepadTarget(
                    m.navigation,m.selectedRecoilWeapon):-1;
            if(pointed>=0)m.navigation.usingStick=false;
            if(chosen==95){
                m.page=m.recoilReturnToDev?kharvox::VrMenuPage::Dev
                    :kharvox::VrMenuPage::Mods;
                m.navigation={};
                m.message=m.recoilReturnToDev
                    ?"Back to DEV tools.":"Back to VR Mod Settings.";
            }else if(chosen>=200&&chosen<200+kharvox::kVrRecoilMenuEntryCount){
                m.selectedRecoilWeapon=chosen-200;
                if(aEdge){
                    m.navigation.settingsColumn=true;
                    m.navigation.actionIndex=0;
                    m.navigation.usingStick=true;
                }else{
                    m.navigation.settingsColumn=false;
                }
            }else if(chosen>=140&&chosen<=145){
                handleVrRecoilCalibrationAction(chosen);
            }
        }
    }
    // The calibration guide is interactive too; do not erase its hover
    // state/cue every frame. Other capture screens have no pointer buttons.
    const bool calibrationButtons=m.mode==kharvox::VrHandMenuMode::Calibrating
        &&m.liveCapture==kharvox::VrLiveCapture::None
        &&m.weaponCalibrationStage&&m.weaponCalibrationStage!=3;
    if((m.mode!=kharvox::VrHandMenuMode::Menu&&!calibrationButtons)
        ||m.calibrationWheel||m.waitingForEquip){
        s.menuHoverCueState={};s.menuHoverCue={};
    }
    // Generic Alt Fire input engages automatically when selected.
    // This is confined to the hand-calibration page and its live capture.
    // The weapon Grip now always controls the preview pose, even on heavies.
    // Trigger is consumed for SAVE, so it cannot fire the weapon here.
    const auto liveKind=KharvoxWeaponCurrentKind();
    const bool heavyLiveWeapon=liveKind==KharvoxWeaponKind::GaussCannon
        ||liveKind==KharvoxWeaponKind::Chaingun;
    const bool liveAltFire=gameplay&&s.customMods.offHandGripCapture
        &&m.page==kharvox::VrMenuPage::WeaponGrip
        &&m.alternateProfile
        &&heavyLiveWeapon&&liveKind==KharvoxWeaponKindFromKey(m.weapon())
        &&(m.mode==kharvox::VrHandMenuMode::Menu
            ||(m.mode==kharvox::VrHandMenuMode::Calibrating
                &&s.weaponHandLeft==m.calibratingWeaponLeft));
    if(s.heavyAlternateInputForwarded!=liveAltFire)
        log(std::string("[VR-HAND-MENU] live calibration native secondary fire ")
            +(liveAltFire?"ON":"OFF")+" (physical weapon-hand Grip)");
    s.heavyAlternateInputForwarded=liveAltFire;
    if(!rightGrip)m.consumeDevGripUntilRelease=false;
    const bool menuWalk=kharvox::vrMenuGameplayTurnAllowed(m.mode,m.page,
        m.calibrationWheel,m.waitingForEquip,m.consumeDevGripUntilRelease);
    if(menuWalk){
        if(std::abs(s.rightStick.x)>=s.turnDeadzone)
            kharvox::cancelPostCinematicYawGuard(s.postCinematicYawGuard);
        const float manual=nativeManualTurnX(displayTime,gameplay);
        const float physical=nativePhysicalBodyTurnX(gameplay,manual!=0.f);
        holdNativeTurnCvars(physical!=0.f?bodyFollowYawSpeed:manualTurnCarrierSpeed());
        s.previousManualTurnActive=manual!=0.f;
        updateVirtualLeftStick(s.leftStick,true);
        updateVirtualRightStick({manual!=0.f?manual:physical,0.f},true);
    }else{
        releaseMovement(m.autoSelectingWeapon);
        s.previousTurnDisplayTime=displayTime;
        s.previousManualTurnActive=false;
        s.bodyFollowTurnX=0.f;
        if(m.calibrationWheel||m.waitingForEquip){
            s.snapTurnActivationPending=false;
            s.artificialTurnActive=false;
            s.artificialTurnRemainingDegrees=0.f;
            publishArtificialTurnYaw();
        }
    }
    if(m.calibrationWheel){
        const float dx=m.waitingForEquip?m.wheelAxisX:(m.cursorX-480.f)/190.f;
        const float dy=m.waitingForEquip?m.wheelAxisY:(360.f-m.cursorY)/190.f;
        const float length=std::max(1.f,std::sqrt(dx*dx+dy*dy));
        updateVirtualRightStick({dx/length,dy/length},m.waitingForEquip||(m.cursorX>=0&&m.cursorY>=0&&m.cursorX<vrMenuW&&m.cursorY<vrMenuH));
    }
    KharvoxWeaponSetWheelPresentationActive(m.calibrationWheel);
    // Special BFG/Chainsaw auto-equip uses its native selection channel.
    // Never forward right-trigger FIRE or any melee while this menu owns input.
    const bool selectionPulse=m.autoSelectingWeapon
        &&displayTime<m.nativeSelectionPulseUntil;
    updateVirtualButtons(false,false,false,liveAltFire,false,false,
        selectionPulse&&m.pendingNativeSelection==2,false,m.calibrationWheel,false,
        false,selectionPulse&&m.pendingNativeSelection==1,false,false);
    s.offHandStickInput={};
    s.offHandStickOutput={};
    s.primaryFireDown=false;
    s.firePressed=rightTrigger;
    s.weaponSelectPressed=false;s.weaponWheelOpened=false;
    s.equipmentButtonPressed=false;
    s.usePulseUntil=0;s.meleePulseUntil=0;
    s.chainsawPulseUntil=0;
    s.backWeaponPulseUntil=0;
    const bool preview=m.mode==kharvox::VrHandMenuMode::Calibrating&&m.supportPreviewHeld
        &&m.previewValid&&(m.weaponCalibrationStage==2||m.weaponCalibrationStage==5);
    s.supportGripPressed=preview;
    if(!preview)s.twoHandLatched=false;
    return true;
}
