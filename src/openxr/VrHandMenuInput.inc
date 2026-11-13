// All VR menu inputs are handled before DOOM's native melee/pause/calibration
// routes. The menu owns both clicks and triggers while it is visible; trigger
// save is edge-based and cannot fire the selected weapon.
void refreshVrHandMenuSavedStatus(){
    auto& m=s.vrHandMenu;
    const auto factory=kharvox::readRightHandGripSettings(
        kharvox::runtimePathA("CalibrationDefaults/offhand_right_hand_positions.cfg"));
    for(size_t i=0;i<kharvox::kVrCalibrationWeapons.size();++i){
        const std::string key=kharvox::kVrCalibrationWeapons[i];
        // Factory profiles are shipped into UserData on first run, so they
        // should display DEFAULT, NOT BOTH. Only modifications to PRIMARY
        // position/rotation get a left or right custom marker.
        m.rightSaved[i]=kharvox::vrPrimaryPoseChangedFromFactory(
            s.rightGripSettings,factory,key,false);
        m.leftSaved[i]=kharvox::vrPrimaryPoseChangedFromFactory(
            s.rightGripSettings,factory,key,true);
    }
}
void updateVrMenuPreview(bool primaryWeaponGripHeld){
    auto& m=s.vrHandMenu;
    const auto weapon=KharvoxWeaponKindFromKey(m.weapon());
    const bool primaryHand=m.rightHand!=s.weaponHandLeft;
    const auto& freeGrip=s.weaponHandLeft?s.rightGripController:s.leftGripController;
    const auto& freeAim=s.weaponHandLeft?s.rightController:s.leftController;
    // PRIMARY uses the free controller for wrist rotation only.
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
        // A primary calibration is a single UNSAVED draft. Losing free-hand
        // tracking or switching Grip modes must never discard its gun offset
        // or jump to the previously saved position/rotation.
        if(!canSample){
            m.rotationEdit.referenceValid=false;
            if(!primaryWeaponGripHeld||!gunTracked||!matchingWeapon)
                m.primaryGripPreviewHeld=false;
        }else{
            const auto inverted=conjugate(s.lastWeaponRenderedTrackingRotation);
            const auto local=normalizeQuaternion(multiply(
                inverted,placement.orientation));
            const kharvox::RightHandGripRotation freeRotation{
                local.x,local.y,local.z,local.w,true};
            // On release, first sample becomes a reference, not an absolute
            // glove angle. Subsequent opposite-controller movement changes
            // the glove relative to its CURRENT rotation. Held Grip freezes
            // that rotation and re-arms the reference for the next release.
            m.previewRotation=kharvox::vrCalibrationRelativeRotation(
                m.rotationEdit,primaryWeaponGripHeld,freeRotation,
                m.previewRotation);
        }
        if(primaryWeaponGripHeld){
            // Grip-held does not need the free controller's tracking pose.
            // The physical controller anchors the glove; only the 1cm
            // stick nudges move the native gun's draft positional offset.
            kharvox::vrCalibrationRelativeRotation(
                m.rotationEdit,true,{},m.previewRotation);
        }
        m.previewValid=kharvox::validRightHandVisualPlacement(
                m.previewPosition)
            &&m.previewRotation.valid
            &&kharvox::validRightHandRotation(m.previewRotation);
        const auto& physicalGrip=s.weaponHandLeft
            ?s.leftGripController:s.rightGripController;
        m.primaryGripPreviewHeld=kharvox::vrPrimaryGripHoldPreviewAllowed(
                m.mode,true,m.weapon())
            &&primaryWeaponGripHeld&&m.previewValid
            &&matchingWeapon&&gunTracked&&physicalGrip.valid;
    }else{
        // SUPPORT position follows the real free-hand GRIP tracker, not
        // its displaced AIM laser. The saved local offset and displayed
        // hand are measured in the exact same weapon reference frame.
        if(!canSample){
            m.previewValid=false;
            m.primaryGripPreviewHeld=false;
        }else if(kharvox::vrWeaponManualCalibrationLive(
                m.mode,primaryWeaponGripHeld,false)){
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
            +(primaryWeaponGripHeld?" Grip-held GUN_FINE_TUNE"
                :" Grip-released GLOVE_ROTATION")
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
// comfortable fixed pace. Grip HELD freezes glove rotation; only GUN moves.
// Grip RELEASE resumes visual glove rotation. Right trigger SAVES.
// The calibration menu still suppresses locomotion and firing.
void serviceVrWeaponFineTune(bool weaponGripDown){
    auto& m=s.vrHandMenu;
    const bool primary=m.mode==kharvox::VrHandMenuMode::Calibrating
        &&m.liveCapture==kharvox::VrLiveCapture::None
        &&m.page==kharvox::VrMenuPage::WeaponGrip
        &&m.rightHand!=s.weaponHandLeft
        &&m.calibratingWeaponLeft==s.weaponHandLeft
        &&m.previewValid&&m.primaryGripPreviewHeld
        &&m.weapon()==std::string(KharvoxWeaponKindKey(KharvoxWeaponCurrentKind()));
    XrVector2f rightStick{};
    if(primary&&weaponGripDown){
        // The normal right-stick sample is read AFTER menu dispatch.
        // Fetch it here only for the grip-held fine-tune, never modify
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
void saveVrHandMenuCalibration(){
    auto& m=s.vrHandMenu;
    if(!m.previewValid){m.message="Tracking/placement invalid; try again.";return;}
    const auto kind=KharvoxWeaponKindFromKey(m.weapon());
    if(kind!=KharvoxWeaponCurrentKind()
        ||s.weaponHandLeft!=m.calibratingWeaponLeft){
        m.message="Weapon or holding hand changed; capture not saved.";return;
    }
    const bool primaryHand=m.rightHand!=s.weaponHandLeft;
    std::string why;
    bool ok=false;
    const bool alternate=m.alternateProfile
        &&(kind==KharvoxWeaponKind::GaussCannon
            ||kind==KharvoxWeaponKind::Chaingun);
    if(primaryHand){
        ok=kharvox::saveRightHandVisualPose(
            kharvox::runtimePathA("offhand_right_hand_positions.cfg"),
            m.weapon(),alternate,m.previewPosition,m.previewRotation,
            &why,s.weaponHandLeft,&m.fineTuneAccumulated);
        if(ok){
            s.rightGripSettings=kharvox::readRightHandGripSettings(
                kharvox::runtimePathA("offhand_right_hand_positions.cfg"));
            s.primaryVisualLocked=false;
        }
    }else if(alternate){
        // Preserve separate Siege/Turret support positions formerly saved
        // by a physical shortcut; now configurable only through the menu.
        const auto rightFile=kharvox::runtimePathA("offhand_right_hand_positions.cfg");
        ok=kharvox::saveHeavyAlternativeSupport(
            rightFile,m.weapon(),m.previewPosition,&why,s.weaponHandLeft);
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
        (alternate?" ALTERNATE":" NORMAL")+
        (m.rightHand?" RIGHT":" LEFT")+
        (primaryHand?" primary pose+rotation":" support position (grab rotation preserved)"));
    kharvox::queueControllerZoneCue(s.handZoneCues[1],GetTickCount64(),
        190,.80f,185.f);
    m.mode=kharvox::VrHandMenuMode::Menu;
    m.message=std::string(m.rightHand?"RIGHT":"LEFT")+
        (primaryHand?" PRIMARY":" SUPPORT")+
        (alternate?" ALT":" NORMAL")+" saved for "+m.weapon();
    m.previewValid=false;
    m.primaryGripPreviewHeld=false;
    m.rotationEdit={};
    refreshVrHandMenuSavedStatus();
}
void resetVrMenuSelectedWeapon(){
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
        return;
    }
    s.rightGripSettings=kharvox::readRightHandGripSettings(rightFile);
    s.twoHandCalibrations={};
    s.twoHandCalibrationsLeft={};
    loadTwoHandCalibrations();
    s.primaryVisualLocked=false;s.supportVisualLocked=false;
    s.twoHandLatched=false;
    refreshVrHandMenuSavedStatus();
    m.message=std::string("Your factory calibration restored: ")+m.weapon();
    log("[VR-HAND-MENU] "+m.message);
}
void applyRightGripToLeft(){
    auto& m=s.vrHandMenu;
    const auto rightFile=kharvox::runtimePathA("offhand_right_hand_positions.cfg");
    // Read the saved user values, not a temporary hand-position preview.
    const auto latest=kharvox::readRightHandGripSettings(rightFile);
    kharvox::RightHandGripOffset pos{};
    kharvox::RightHandGripRotation rot{};
    const auto weapon=KharvoxWeaponKindFromKey(m.weapon());
    const bool alternate=m.alternateProfile
        &&(weapon==KharvoxWeaponKind::GaussCannon
            ||weapon==KharvoxWeaponKind::Chaingun);
    // Transfer relative to the factory grip for EACH hand model. The
    // physical left model is not a mirrored copy of the right model, so
    // reflecting the absolute right rotation breaks forward alignment.
    const auto factory=kharvox::readRightHandGripSettings(
        kharvox::runtimePathA("CalibrationDefaults/offhand_right_hand_positions.cfg"));
    if(!kharvox::factoryAlignedRightPrimaryToLeft(
        latest,factory,m.weapon(),alternate,pos,rot)){
        m.message="COPY FAILED: missing saved RIGHT or factory LEFT reference.";
        log("[VR-HAND-MENU] "+m.message);
        return;
    }
    std::string why;
    const auto rightTune=kharvox::primaryVisualFineTune(
        latest,m.weapon(),false,alternate);
    const kharvox::RightHandGripOffset leftTune{
        -rightTune.x,rightTune.y,rightTune.z};
    if(!kharvox::saveRightHandVisualPose(rightFile,m.weapon(),alternate,
        pos,rot,&why,true,&leftTune)){
        m.message="MIRROR FAILED: "+why;return;
    }
    s.rightGripSettings=kharvox::readRightHandGripSettings(rightFile);
    s.primaryVisualLocked=false;
    s.supportVisualLocked=false;
    refreshVrHandMenuSavedStatus();
    m.message=std::string("LEFT grip updated from RIGHT adjustment: ")
        +m.weapon()+(alternate?" (alternate mode)":"")
        +". Factory LEFT alignment retained; fine-tune if needed.";
    log("[VR-HAND-MENU] "+m.message);
}
void startVrSelectedWeaponCalibration(){
    auto& m=s.vrHandMenu;
    m.autoSelectingWeapon=false;
    m.pendingNativeSelection=0;
    m.mode=kharvox::VrHandMenuMode::Calibrating;
    m.calibratingWeaponLeft=s.weaponHandLeft;
    m.previewValid=false;
    m.primaryGripPreviewHeld=false;
    m.rotationEdit={}; // a fresh unsaved rotation draft for this calibration
    m.lastPreviewDiagnosticAt=0;
    m.fineTuneEdges={};
    m.fineTuneAccumulated={};
    // Saved fine-tune offsets now belong to the GUN, never the glove.
    // Free-controller motion in primary calibration adjusts rotation only.
    const bool calibratingPrimary=m.rightHand!=s.weaponHandLeft;
    m.primaryLateralBase=0.f;
    const auto kind=KharvoxWeaponKindFromKey(m.weapon());
    const bool alternate=m.alternateProfile
        &&(kind==KharvoxWeaponKind::GaussCannon
            ||kind==KharvoxWeaponKind::Chaingun);
    if(calibratingPrimary)
        m.fineTuneAccumulated=kharvox::primaryVisualFineTune(
            s.rightGripSettings,m.weapon(),s.weaponHandLeft,alternate);
    const auto savedX=[&](const kharvox::RightHandGripSettings& settings,
                          float& value)->bool {
        if(s.weaponHandLeft){
            const auto key=std::string(m.weapon())
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
                settings,m.weapon(),alternate,position,rotation))return false;
        value=position.x;
        return true;
    };
    if(!savedX(s.rightGripSettings,m.primaryLateralBase))
        savedX(s.rightGripFactorySettings,m.primaryLateralBase);
    if(calibratingPrimary){
        // Explicitly seed from this weapon's saved position rather than
        // the opposite controller: this is a gun-only positional reference.
        kharvox::RightHandGripOffset savedPosition{};
        kharvox::RightHandGripRotation unusedRotation{};
        bool loaded=false;
        const std::string key=std::string(m.weapon())
            +(alternate?(kind==KharvoxWeaponKind::GaussCannon
                ?"_siege":"_turret"):"");
        if(s.weaponHandLeft){
            auto it=s.rightGripSettings.leftVisualPositions.find(key);
            if(it!=s.rightGripSettings.leftVisualPositions.end()){
                savedPosition=it->second;loaded=true;
            }
        }else{
            loaded=kharvox::rightPrimaryVisualPose(
                s.rightGripSettings,m.weapon(),alternate,
                savedPosition,unusedRotation);
        }
        if(!loaded){
            if(s.weaponHandLeft){
                auto it=s.rightGripFactorySettings.leftVisualPositions.find(key);
                if(it!=s.rightGripFactorySettings.leftVisualPositions.end()){
                    savedPosition=it->second;loaded=true;
                }
            }else loaded=kharvox::rightPrimaryVisualPose(
                s.rightGripFactorySettings,m.weapon(),alternate,
                savedPosition,unusedRotation);
        }
        m.previewPosition=loaded?savedPosition:kharvox::RightHandGripOffset{
            s.primaryVisualLocalPosition.x,
            s.primaryVisualLocalPosition.y,
            s.primaryVisualLocalPosition.z};
    }
    if(calibratingPrimary){
        // The first frame of calibration, including one entered with
        // weapon Grip already HELD, must have a real visual wrist pose.
        // Otherwise an unset quaternion cannot be frozen/saved and the
        // fine-tune becomes inaccessible until an unrelated grip release.
        const auto& existing=s.primaryVisualLocalOrientation;
        m.previewRotation={existing.x,existing.y,existing.z,existing.w,true};
        if(!kharvox::validRightHandRotation(m.previewRotation))
            m.previewRotation={0.f,0.f,0.f,1.f,true};
        m.previewValid=kharvox::validRightHandVisualPlacement(
            m.previewPosition)&&kharvox::validRightHandRotation(m.previewRotation);
    }
    s.primaryVisualLocked=false;
    s.supportVisualLocked=false;
    m.message="Hold Grip: shift GUN 1cm. Release Grip: keep GUN offset, rotate glove. Trigger saves; B or hold RS cancels.";
    log(std::string("[VR-MODS] calibrating ")+m.weapon()+
        (m.rightHand?" RIGHT":" LEFT")+
        (m.rightHand!=s.weaponHandLeft?" PRIMARY":" SUPPORT"));
}
void requestVrSelectedWeaponCalibration(XrTime displayTime){
    auto& m=s.vrHandMenu;
    if(!s.customMods.offHandGripCapture){
        m.message="Enable Weapon Grip Calibration with VR checkbox first.";
        return;
    }
    const auto weapon=KharvoxWeaponKindFromKey(m.weapon());
    if(weapon==KharvoxWeaponCurrentKind()){
        startVrSelectedWeaponCalibration();
        return;
    }
    // Never guess ownership or manually select an undiscovered weapon.
    // Empty ammo is still a discovered gun; unknown/unavailable cannot
    // be auto-selected safely.
    const auto ammo=KharvoxWeaponGetAmmoState(weapon);
    if(ammo==KharvoxWeaponAmmoState::Unknown
        ||ammo==KharvoxWeaponAmmoState::Unavailable){
        m.message=std::string("NOT AVAILABLE: ")+m.weapon()+
            " not discovered. Current weapon unchanged.";
        return;
    }
    const auto slot=backWeaponKindFromActive(weapon);
    const auto key=kharvox::backWeaponKeyboardVirtualKey(slot);
    bool sent=false;
    if(key){
        sent=startBackWeaponKeyboardPulse(key,displayTime);
    }else if(slot==kharvox::BackWeaponKind::Bfg
             ||slot==kharvox::BackWeaponKind::Chainsaw){
        m.pendingNativeSelection=slot==kharvox::BackWeaponKind::Bfg?1:2;
        m.nativeSelectionPulseUntil=displayTime+100000000;
        sent=true;
    }
    if(!sent){
        m.message="Could not select weapon. No calibration started.";
        return;
    }
    m.autoSelectingWeapon=true;
    m.autoSelectionDeadlineMs=GetTickCount64()+1300ULL;
    m.message=std::string("Equipping ")+m.weapon()+
        "... calibration starts after DOOM confirms selection.";
    log(std::string("[VR-MODS] requested available weapon ")+m.weapon());
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
    if(s.vrHandMenu.page==kharvox::VrMenuPage::Mods){
        if(pixelX>=38&&pixelX<=493){
            for(int i=0;i<10;++i)
                if(pixelY>=136+i*45&&pixelY<176+i*45)
                    return 1000+s.vrHandMenu.modScroll+i;
        }
        if(pixelX>=531&&pixelX<=915){
            const auto list=activeVrMenuEntries();
            const int row=s.vrHandMenu.selectedRow;
            if(row<0||row>=static_cast<int>(list.size()))return -1;
            if(!s.vrHandMenu.navigation.settingsColumn){
                // Browse panel is read-only, except Configure.
                return pixelY>=548&&pixelY<613?150:-1;
            }
            if(pixelY>=583&&pixelY<632)return 151;
            const bool toggleable=kharvox::vrLiveModToggleable(list[row].mod);
            if(toggleable&&pixelY>=159&&pixelY<219)return 110;
            const int optionY=toggleable?248:186;
            for(int i=0;i<5;++i)
                if(pixelY>=optionY+i*68&&pixelY<optionY+58+i*68)
                    return 130+i;
        }
        return -1;
    }
    if(pixelX>=800&&pixelX<927&&pixelY>=10&&pixelY<76)return 95;
    if(pixelX>=38&&pixelX<=493){
        for(int i=0;i<10;++i)
            if(pixelY>=136+i*45&&pixelY<176+i*45)return i;
    }
    if(pixelY>=151&&pixelY<209){
        if(pixelX>=531&&pixelX<701)return 20;
        if(pixelX>=715&&pixelX<917)return 21;
    }
    if(pixelX>=531&&pixelX<915&&pixelY>=306&&pixelY<355)return 32;
    if(pixelX>=531&&pixelX<915){
        if(pixelY>=363&&pixelY<432)return 30;
        if(pixelY>=454&&pixelY<523)return 31;
        if(pixelY>=535&&pixelY<593)return 33; // optional RIGHT -> LEFT
    }
    return -1;
}
bool serviceVrHandMenuInput(XrTime displayTime,bool gameplay,bool leftClick,bool rightClick,
                            bool rightTrigger,bool leftTrigger,
                            bool primaryWeaponGripDown,bool menuA,bool menuB){
    auto& m=s.vrHandMenu;
    // Display-only: reflect actual physical Grip even if the live preview
    // is invalid. Never gate calibration mechanics or input on this flag.
    m.calibrationGripHeld=m.mode==kharvox::VrHandMenuMode::Calibrating
        &&m.liveCapture==kharvox::VrLiveCapture::None
        &&m.rightHand!=s.weaponHandLeft&&primaryWeaponGripDown;
    // A single 1-second RIGHT-stick hold opens/closes this menu.
    // Ordinary RS taps still belong to Hand Focus.
    // The menu remains accessible when Weapon Grip Calibration is OFF,
    // otherwise the player could not turn the mod back on from VR.
    const auto shortcut=kharvox::updateVrMenuHoldShortcut(
        m.shortcut,rightClick,GetTickCount64());
    if(!gameplay){
        m.mode=kharvox::VrHandMenuMode::Closed;
        m.autoSelectingWeapon=false;
        m.pendingNativeSelection=0;
        m.liveCapture=kharvox::VrLiveCapture::None;
        m.previewValid=false;
        m.shortcut={};
        m.rightTriggerPreviously=rightTrigger;
        m.menuAPreviously=menuA;
        m.menuBPreviously=menuB;
        m.consumeMenuFaceUntilRelease=false;
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
            m.page=kharvox::VrMenuPage::Mods;
            m.selectedMod=kharvox::VrLiveMod::WeaponGrip;
            m.selectedRow=0;
            m.modScroll=0;
            m.devScroll=0;
            m.devInfoIndex=-1;
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
            m.message="Browse mod descriptions with LEFT stick; A opens settings. B goes back.";
            refreshVrHandMenuSavedStatus();
            log("[VR-HAND-MENU] opened by 1-second RIGHT-stick hold");
        }else{
            m.mode=m.mode==kharvox::VrHandMenuMode::Menu
                ?kharvox::VrHandMenuMode::Closed:kharvox::VrHandMenuMode::Menu;
            m.previewValid=false;
            m.rotationEdit={};
            m.primaryGripPreviewHeld=false;
            m.liveCapture=kharvox::VrLiveCapture::None;
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
        m.rightTriggerPreviously=rightTrigger;
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
    m.rightTriggerPreviously=rightTrigger;
    const bool aEdge=kharvox::vrMenuButtonEdge(menuA,m.menuAPreviously);
    const bool bEdge=kharvox::vrMenuButtonEdge(menuB,m.menuBPreviously);
    // B cancels capture without saving. From weapon profiles B returns to
    // Mods; from the top-level Mods page it closes the overlay.
    if(bEdge){
        const bool wasLiveModCapture=m.liveCapture!=kharvox::VrLiveCapture::None;
        m.liveCapture=kharvox::VrLiveCapture::None;
        m.autoSelectingWeapon=false;
        m.pendingNativeSelection=0;
        m.previewValid=false;
        m.primaryGripPreviewHeld=false;
        m.rotationEdit={};
        s.primaryVisualLocked=false;
        s.supportVisualLocked=false;
        if(m.mode==kharvox::VrHandMenuMode::Calibrating){
            m.mode=kharvox::VrHandMenuMode::Menu;
            // Gunslinger/Crouch capture returns to Mods; weapon grip capture
            // returns to the selected weapon. Neither cancellation saves.
            m.page=wasLiveModCapture?kharvox::VrMenuPage::Mods
                :kharvox::VrMenuPage::WeaponGrip;
            m.message="Calibration cancelled. No changes saved.";
        }else if(m.page==kharvox::VrMenuPage::WeaponGrip){
            m.page=kharvox::VrMenuPage::Mods;
            m.navigation={};
            m.message="Back to VR Mod Settings.";
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
            if(triggerEdge||aEdge)captureVrLivePosition();
        }else{
            updateVrMenuPreview(primaryWeaponGripDown);
            serviceVrWeaponFineTune(primaryWeaponGripDown);
            if(triggerEdge)saveVrHandMenuCalibration();
        }
    }else if(!bEdge&&m.mode==kharvox::VrHandMenuMode::Menu){
        m.hovered=vrMenuRayTarget(m.cursorX,m.cursorY);
        const auto rows=activeVrMenuEntries();
        if(m.page==kharvox::VrMenuPage::Mods){
            const int n=static_cast<int>(rows.size());
            if(n>0)m.selectedRow=std::clamp(m.selectedRow,0,n-1);
            const auto optionCount=[&](const VrLiveMenuEntry& entry)->int{
                using kharvox::VrLiveMod;
                const auto mod=entry.mod;
                if(!kharvox::vrLiveModEditable(mod))return 0;
                switch(mod){
                    case VrLiveMod::WeaponGrip:return 1; // open calibration
                    case VrLiveMod::Gunslinger:return 5;
                    case VrLiveMod::PhysicalCrouch:return 4; // toggle + 3 heights
                    case VrLiveMod::Killtime:return 3;
                    case VrLiveMod::PhysicalGloryKills:return 4;
                    case VrLiveMod::Painsaw:return 1; // toggle only
                    default:return 0;
                }
            };
            const auto before=m.selectedRow;
            const int previousOptions=n>0?optionCount(rows[m.selectedRow]):0;
            kharvox::vrMenuNavigateStick(m.navigation,s.leftStick.x,s.leftStick.y,
                n,m.selectedRow,previousOptions);
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
            // Hovering a row updates its description without a click.
            if(!m.navigation.settingsColumn
                &&m.hovered>=1000&&m.hovered<1000+n){
                m.selectedRow=m.hovered-1000;
                m.selectedMod=rows[m.selectedRow].mod;
                m.navigation.actionIndex=0;
                m.navigation.usingStick=false;
            }
            const int pointed=triggerEdge?m.hovered:-1;
            const int chosen=pointed>=0?pointed:
                aEdge?kharvox::vrMenuModsGamepadTarget(m.navigation,
                    m.selectedRow,n,n>0?optionCount(rows[m.selectedRow]):0,
                    n>0&&kharvox::vrLiveModToggleable(rows[m.selectedRow].mod)):-1;
            if(pointed>=0)m.navigation.usingStick=false;
            if(chosen>=1000&&chosen<1000+n){
                const bool wasInSettings=m.navigation.settingsColumn;
                const bool alreadySelected=m.selectedRow==chosen-1000;
                m.selectedRow=chosen-1000;
                m.selectedMod=rows[m.selectedRow].mod;
                m.navigation.actionIndex=0;
                if(aEdge||(pointed>=0&&!wasInSettings&&alreadySelected)){
                    m.navigation.settingsColumn=true;
                    m.navigation.usingStick=aEdge;
                    m.message="Mod settings open. B returns to descriptions.";
                }else{
                    m.navigation.settingsColumn=false;
                    m.message="Read about the mod; press A or CONFIGURE.";
                }
            }else if(chosen==150){
                m.navigation.settingsColumn=true;
                m.navigation.actionIndex=0;
                m.navigation.usingStick=false;
                m.message="Mod settings open. B returns to descriptions.";
            }else if(chosen==151){
                m.navigation.settingsColumn=false;
                m.navigation.actionIndex=0;
                m.navigation.usingStick=false;
                m.message="Back to mod descriptions.";
            }else if(m.navigation.settingsColumn
                &&(chosen==110||(chosen>=130&&chosen<=134))){
                handleVrLiveMenuAction(chosen);
                if(m.page==kharvox::VrMenuPage::WeaponGrip)m.navigation={};
            }
        }else{
            const auto previousWeapon=m.selectedWeapon;
            const auto selectedKind=KharvoxWeaponKindFromKey(m.weapon());
            const bool heavy=selectedKind==KharvoxWeaponKind::GaussCannon
                ||selectedKind==KharvoxWeaponKind::Chaingun;
            kharvox::vrMenuNavigateStick(m.navigation,s.leftStick.x,s.leftStick.y,
                10,m.selectedWeapon,heavy?7:6);
            if(previousWeapon!=m.selectedWeapon){
                m.navigation.actionIndex=0;
                m.autoSelectingWeapon=false;
                m.pendingNativeSelection=0;
                m.alternateProfile=false;
            }
            const auto selectedNow=KharvoxWeaponKindFromKey(m.weapon());
            const bool heavyNow=selectedNow==KharvoxWeaponKind::GaussCannon
                ||selectedNow==KharvoxWeaponKind::Chaingun;
            m.navigation.actionIndex=std::clamp(m.navigation.actionIndex,
                0,heavyNow?6:5);
            if(m.autoSelectingWeapon){
                const auto desired=KharvoxWeaponKindFromKey(m.weapon());
                if(desired==KharvoxWeaponCurrentKind()){
                    startVrSelectedWeaponCalibration();
                }else if(GetTickCount64()>=m.autoSelectionDeadlineMs){
                    m.autoSelectingWeapon=false;
                    m.pendingNativeSelection=0;
                    m.message="Weapon unavailable or selection timed out. Not calibrated.";
                }
            }
            const int pointed=triggerEdge?m.hovered:-1;
            const int chosen=pointed>=0?pointed:
                aEdge?kharvox::vrMenuGripGamepadTarget(m.navigation,
                    m.selectedWeapon,heavyNow):-1;
            if(pointed>=0)m.navigation.usingStick=false;
            if(chosen==95){
                m.page=kharvox::VrMenuPage::Mods;
                m.navigation={};
                m.autoSelectingWeapon=false;
                m.pendingNativeSelection=0;
                m.message="Live VR Mod Settings";
            }else if(chosen>=0&&chosen<10){
                m.selectedWeapon=chosen;
                m.autoSelectingWeapon=false;
                m.pendingNativeSelection=0;
                m.alternateProfile=false;
                if(aEdge){m.navigation.settingsColumn=true;
                    m.navigation.usingStick=true;}
            }else if(chosen==20)m.rightHand=false;
            else if(chosen==21)m.rightHand=true;
            else if(chosen==31)resetVrMenuSelectedWeapon();
            else if(chosen==33)applyRightGripToLeft();
            else if(chosen==32){
                if(heavyNow){
                    m.alternateProfile=!m.alternateProfile;
                    m.message=m.alternateProfile
                        ?"Siege/Turret auto-engages while in calibration."
                        :"Normal profile; automatic alternate fire released.";
                }
            }else if(chosen==30)requestVrSelectedWeaponCalibration(displayTime);
            m.leftTriggerPreviously=leftTrigger;
        }
    }
    // Siege/Turret alternate mode engages automatically when selected.
    // This is confined to the hand-calibration page and its live capture.
    // The weapon Grip now always controls the preview pose, even on heavies.
    // Trigger is consumed for SAVE, so it cannot fire the weapon here.
    const auto liveKind=KharvoxWeaponCurrentKind();
    const bool heavyLiveWeapon=liveKind==KharvoxWeaponKind::GaussCannon
        ||liveKind==KharvoxWeaponKind::Chaingun;
    const bool liveAltFire=gameplay&&s.customMods.offHandGripCapture
        &&m.page==kharvox::VrMenuPage::WeaponGrip&&m.alternateProfile
        &&heavyLiveWeapon&&liveKind==KharvoxWeaponKindFromKey(m.weapon())
        &&(m.mode==kharvox::VrHandMenuMode::Menu
            ||(m.mode==kharvox::VrHandMenuMode::Calibrating
                &&s.weaponHandLeft==m.calibratingWeaponLeft));
    if(s.heavyAlternateInputForwarded!=liveAltFire)
        log(std::string("[VR-HAND-MENU] live calibration native secondary fire ")
            +(liveAltFire?"ON":"OFF")+" (physical weapon-hand Grip)");
    s.heavyAlternateInputForwarded=liveAltFire;
    releaseMovement(m.autoSelectingWeapon);
    // Special BFG/Chainsaw auto-equip uses its native selection channel.
    // Never forward right-trigger FIRE or any melee while this menu owns input.
    const bool selectionPulse=m.autoSelectingWeapon
        &&displayTime<m.nativeSelectionPulseUntil;
    updateVirtualButtons(false,false,false,liveAltFire,false,false,
        selectionPulse&&m.pendingNativeSelection==2,false,false,false,
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
    s.bfgPulseUntil=0;
    s.grenadeCyclePulseUntil=0;
    s.equipmentThrowPulseUntil=0;
    s.twoHandLatched=false;
    return true;
}
