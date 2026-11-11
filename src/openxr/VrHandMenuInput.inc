// All VR menu inputs are handled before DOOM's native melee/pause/calibration
// routes. The menu owns both clicks and triggers while it is visible; trigger
// save is edge-based and cannot fire the selected weapon.
void refreshVrHandMenuSavedStatus(){
    auto& m=s.vrHandMenu;
    const auto& positions=s.rightGripSettings.visualPositions;
    const std::string supportFile=kharvox::runtimePathA("two_hand_weapon_profiles_saved.cfg");
    for(size_t i=0;i<kharvox::kVrCalibrationWeapons.size();++i){
        const std::string key=kharvox::kVrCalibrationWeapons[i];
        m.rightSaved[i]=positions.find(key)!=positions.end();
        m.leftSaved[i]=kharvox::hasSavedVrSupport(supportFile,key);
    }
}
void updateVrMenuPreview(){
    auto& m=s.vrHandMenu;
    m.previewValid=false;
    const auto weapon=KharvoxWeaponKindFromKey(m.weapon());
    if(weapon==KharvoxWeaponKind::Unknown||weapon!=KharvoxWeaponCurrentKind()
        ||s.weaponHandLeft||!s.lastWeaponRenderedValid
        ||s.lastWeaponRenderedKind!=weapon
        ||!s.leftGripController.valid||!s.leftGripController.positionTracked)return;
    XrVector3f probe=s.leftGripController.position;
    if(m.rightHand){
        const auto forward=kharvox::liveHeavyGripProbePoint(
            {probe.x,probe.y,probe.z},
            {s.leftGripController.orientation.x,s.leftGripController.orientation.y,
             s.leftGripController.orientation.z,s.leftGripController.orientation.w,true});
        probe={forward.x,forward.y,forward.z};
    }
    const auto inverted=conjugate(s.lastWeaponRenderedTrackingRotation);
    const auto local=rotateVector(inverted,
        subtractVector(probe,s.lastWeaponRenderedTrackingAnchor));
    const auto rotation=normalizeQuaternion(multiply(
        inverted,s.leftGripController.orientation));
    m.previewPosition={local.x,local.y,local.z};
    m.previewRotation={rotation.x,rotation.y,rotation.z,rotation.w,true};
    m.previewValid=(m.rightHand
        ?kharvox::validRightHandVisualPlacement(m.previewPosition)
        :kharvox::validRightHandPlacement(m.previewPosition))
        &&kharvox::validRightHandRotation(m.previewRotation);
}
void saveVrHandMenuCalibration(){
    auto& m=s.vrHandMenu;
    if(!m.previewValid){m.message="Tracking/placement invalid; try again.";return;}
    const auto kind=KharvoxWeaponKindFromKey(m.weapon());
    if(kind!=KharvoxWeaponCurrentKind()){
        m.message="Equip the chosen weapon first.";return;
    }
    std::string why;
    bool ok=false;
    if(m.rightHand){
        ok=kharvox::saveRightHandVisualPose(
            kharvox::runtimePathA("offhand_right_hand_positions.cfg"),
            m.weapon(),false,m.previewPosition,m.previewRotation,&why);
        if(ok){
            s.rightGripSettings=kharvox::readRightHandGripSettings(
                kharvox::runtimePathA("offhand_right_hand_positions.cfg"));
            s.primaryVisualLocked=false;
        }
    }else{
        auto* profile=twoHandCalibrationFor(kind);
        if(!profile){m.message="No support-hand profile for this weapon.";return;}
        const auto mode=profile->aimMode;
        ok=kharvox::saveHeavySupportGrip(
            kharvox::runtimePathA("two_hand_weapon_profiles_saved.cfg"),
            m.weapon(),m.previewPosition,mode==TwoHandAimMode::SideGrip,
            std::clamp(profile->captureRadius,.08f,.40f),
            std::clamp(profile->maximumHandLineWeight,0.f,1.f),&why);
        if(ok){
            profile->supportPosition={m.previewPosition.x,m.previewPosition.y,
                m.previewPosition.z};
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
        (m.rightHand?" RIGHT pose+rotation":" LEFT position (grab rotation preserved)"));
    kharvox::queueControllerZoneCue(s.handZoneCues[1],GetTickCount64(),
        190,.80f,185.f);
    m.mode=kharvox::VrHandMenuMode::Menu;
    m.message=std::string(m.rightHand?"RIGHT":"LEFT")+" hand saved for "+m.weapon();
    m.previewValid=false;
    refreshVrHandMenuSavedStatus();
}
void resetVrMenuSelectedWeapon(){
    auto& m=s.vrHandMenu;
    const auto rightFile=kharvox::runtimePathA("offhand_right_hand_positions.cfg");
    const auto leftFile=kharvox::runtimePathA("two_hand_weapon_profiles_saved.cfg");
    std::string why;
    if(!kharvox::resetVrWeaponCalibrationFile(rightFile,m.weapon(),true,&why)
        ||!kharvox::resetVrWeaponCalibrationFile(leftFile,m.weapon(),false,&why)){
        m.message="RESET FAILED: "+why;
        log("[VR-HAND-MENU] "+m.message);
        return;
    }
    s.rightGripSettings=kharvox::readRightHandGripSettings(rightFile);
    s.twoHandCalibrations={};
    loadTwoHandCalibrations();
    s.primaryVisualLocked=false;s.supportVisualLocked=false;
    s.twoHandLatched=false;
    refreshVrHandMenuSavedStatus();
    m.message=std::string("Default hand positions restored: ")+m.weapon();
    log("[VR-HAND-MENU] "+m.message);
}
int vrMenuRayTarget(float&pixelX,float&pixelY){
    pixelX=pixelY=-1.f;
    if(!s.rightController.valid||!s.rightController.positionTracked
        ||!s.trackingHeadPositionValid)return -1;
    // The compositor quad is head-locked in viewSpace at Z=-1.1 m.
    const XrQuaternionf inverse=conjugate(
        normalizeQuaternion(s.views[0].pose.orientation));
    const auto origin=rotateVector(inverse,subtractVector(
        s.rightController.position,s.trackingHeadPosition));
    const auto direction=rotateVector(inverse,rotateVector(
        s.rightController.orientation,{0,0,-1}));
    if(direction.z>=-.05f)return -1;
    const float distance=(-1.1f-origin.z)/direction.z;
    if(distance<=.01f||distance>5.f)return -1;
    const float x=origin.x+direction.x*distance;
    const float y=origin.y+direction.y*distance;
    pixelX=(x/1.28f+.5f)*float(vrMenuW);
    pixelY=(.5f-y/.96f)*float(vrMenuH);
    if(pixelX<0||pixelX>=vrMenuW||pixelY<0||pixelY>=vrMenuH)return -1;
    if(pixelX>=38&&pixelX<=493){
        for(int i=0;i<10;++i)
            if(pixelY>=136+i*45&&pixelY<176+i*45)return i;
    }
    if(pixelY>=151&&pixelY<209){
        if(pixelX>=531&&pixelX<701)return 20;
        if(pixelX>=715&&pixelX<917)return 21;
    }
    if(pixelX>=531&&pixelX<915){
        if(pixelY>=363&&pixelY<432)return 30;
        if(pixelY>=454&&pixelY<523)return 31;
    }
    return -1;
}
bool serviceVrHandMenuInput(bool gameplay,bool leftClick,bool rightClick,
                            bool rightTrigger,bool leftTrigger){
    auto& m=s.vrHandMenu;
    const bool chord=kharvox::vrHandMenuBothStickEdge(m,leftClick,rightClick);
    if(!gameplay){
        m.mode=kharvox::VrHandMenuMode::Closed;
        m.previewValid=false;
        m.rightTriggerPreviously=rightTrigger;
        return false;
    }
    if(chord){
        if(m.mode==kharvox::VrHandMenuMode::Closed){
            if(!vrHandMenuSurface.ready){
                log("[VR-HAND-MENU] cannot open: XR overlay texture unavailable");
                return true;
            }
            m.mode=kharvox::VrHandMenuMode::Menu;
            const auto equipped=KharvoxWeaponCurrentKind();
            for(int i=0;i<10;++i)
                if(KharvoxWeaponKindFromKey(kharvox::kVrCalibrationWeapons[i])
                    ==equipped)m.selectedWeapon=i;
            m.message="Select weapon + hand. Right trigger starts.";
            refreshVrHandMenuSavedStatus();
            log("[VR-HAND-MENU] opened by BOTH thumbsticks");
        }else{
            m.mode=m.mode==kharvox::VrHandMenuMode::Menu
                ?kharvox::VrHandMenuMode::Closed:kharvox::VrHandMenuMode::Menu;
            m.previewValid=false;
            s.primaryVisualLocked=false;
            s.supportVisualLocked=false;
            log("[VR-HAND-MENU] thumbstick chord closes/cancels current screen");
        }
        s.offHandCaptureChord={};
        s.offHandStickInput={};
        m.rightTriggerPreviously=rightTrigger;
        m.leftTriggerPreviously=leftTrigger;
    }
    if(m.mode==kharvox::VrHandMenuMode::Closed){
        m.rightTriggerPreviously=rightTrigger;
        return chord||(leftClick&&rightClick); // never leak the held chord
    }
    const bool triggerEdge=rightTrigger&&!m.rightTriggerPreviously;
    m.rightTriggerPreviously=rightTrigger;
    if(m.mode==kharvox::VrHandMenuMode::Calibrating){
        updateVrMenuPreview();
        if(triggerEdge)saveVrHandMenuCalibration();
    }else{
        m.hovered=vrMenuRayTarget(m.cursorX,m.cursorY);
        if(std::abs(s.leftStick.y)>.75f&&m.leftStickCentered){
            m.selectedWeapon=(m.selectedWeapon+(s.leftStick.y<0.f?1:9))%10;
            m.leftStickCentered=false;
        }else if(std::abs(s.leftStick.y)<.3f)m.leftStickCentered=true;
        if(triggerEdge){
            if(m.hovered>=0&&m.hovered<10)m.selectedWeapon=m.hovered;
            else if(m.hovered==20)m.rightHand=false;
            else if(m.hovered==21)m.rightHand=true;
            else if(m.hovered==31)resetVrMenuSelectedWeapon();
            else if(m.hovered==30||m.hovered==-1){
                if(!s.customMods.offHandGripCapture)
                    m.message="Enable Off-Hand Grip Capture in desktop Mod Manager.";
                else if(s.weaponHandLeft)
                    m.message="Switch to right-hand weapon mode first.";
                else if(KharvoxWeaponKindFromKey(m.weapon())!=KharvoxWeaponCurrentKind())
                    m.message="Equip the selected weapon first, then open this menu.";
                else{
                    m.mode=kharvox::VrHandMenuMode::Calibrating;
                    m.previewValid=false;
                    s.primaryVisualLocked=false;s.supportVisualLocked=false;
                    m.message="Live hand calibration";
                    log(std::string("[VR-HAND-MENU] calibrating ")+m.weapon()+
                        (m.rightHand?" RIGHT pose+rotation":" LEFT position only"));
                }
            }
        }
        if(leftTrigger&&!m.leftTriggerPreviously)
            resetVrMenuSelectedWeapon(); // fallback for thumbstick navigation
        m.leftTriggerPreviously=leftTrigger;
    }
    // While the menu OR capture is active, never forward gamepad movement,
    // weapon fire, grenade, melee, pause or use actions to native DOOM.
    releaseMovement();
    updateVirtualButtons(false,false,false,false,false,false,false,
        false,false,false,false,false,false,false);
    s.offHandStickInput={};s.offHandCaptureChord={};
    s.offHandStickOutput={};
    s.liveHeavyGripActive=false;s.liveHeavyCaptureValid=false;
    s.primaryFireDown=false;
    s.firePressed=rightTrigger;
    s.weaponSelectPressed=false;s.weaponWheelOpened=false;
    s.equipmentButtonPressed=false;
    s.usePulseUntil=0;s.meleePulseUntil=0;
    s.chainsawPulseUntil=0;
    s.backWeaponPulseUntil=0;
    s.bfgPulseUntil=0;
    s.grenadeCyclePulseUntil=0;
    s.equipmentThrowPulseUntil=0;
    s.twoHandLatched=false;
    return true;
}
