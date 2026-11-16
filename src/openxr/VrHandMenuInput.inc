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
        m.previewValid=kharvox::validRightHandVisualPlacement(
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
        m.message="MODE CHANGED: cancel and select Normal or Alt Fire again.";
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
    kharvox::queueControllerZoneCue(s.handZoneCues[1],GetTickCount64(),
        190,.80f,185.f);
    m.mode=kharvox::VrHandMenuMode::Menu;
    m.message=std::string(m.rightHand?"RIGHT":"LEFT")+
        (primaryHand?" PRIMARY":" SUPPORT")+
        (alternate?" ALT FIRE":" NORMAL")
        +" saved for "+m.weapon();
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
void startVrSelectedWeaponCalibration(){
    auto& m=s.vrHandMenu;
    m.autoSelectingWeapon=false;
    m.pendingNativeSelection=0;
    m.mode=kharvox::VrHandMenuMode::Calibrating;
    m.calibratingWeaponLeft=s.weaponHandLeft;
    m.calibrationAlternateProfile=m.alternateProfile;
    m.previewValid=false;
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
    if(calibratingPrimary)
        m.fineTuneAccumulated=kharvox::primaryVisualFineTune(
            s.rightGripSettings,profileKey,s.weaponHandLeft,alternate);
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
    if(!savedX(s.rightGripSettings,m.primaryLateralBase))
        savedX(s.rightGripFactorySettings,m.primaryLateralBase);
    if(calibratingPrimary){
        // Explicitly seed from this weapon's saved position rather than
        // the opposite controller: this is a gun-only positional reference.
        kharvox::RightHandGripOffset savedPosition{};
        kharvox::RightHandGripRotation unusedRotation{};
        bool loaded=false;
        const std::string key=profileKey
            +(alternate?(kind==KharvoxWeaponKind::GaussCannon
                ?"_siege":"_turret"):"");
        if(s.weaponHandLeft){
            auto it=s.rightGripSettings.leftVisualPositions.find(key);
            if(it!=s.rightGripSettings.leftVisualPositions.end()){
                savedPosition=it->second;loaded=true;
            }
        }else{
            loaded=kharvox::rightPrimaryVisualPose(
                s.rightGripSettings,profileKey,alternate,
                savedPosition,unusedRotation);
        }
        if(!loaded){
            if(s.weaponHandLeft){
                auto it=s.rightGripFactorySettings.leftVisualPositions.find(key);
                if(it!=s.rightGripFactorySettings.leftVisualPositions.end()){
                    savedPosition=it->second;loaded=true;
                }
            }else loaded=kharvox::rightPrimaryVisualPose(
                s.rightGripFactorySettings,profileKey,alternate,
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
    m.message="NO GRIP: sticks move gun 1cm. HOLD "+std::string(m.rightHand?"RIGHT":"LEFT")
        +" GRIP: gun freezes; rotate weapon hand. TRIGGER: SAVE.";
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
    // The owner decides which in-game attachment is fitted. Selecting the
    // generic Alt Fire profile needs only one CALIBRATE press.
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
    if(s.vrHandMenu.page==kharvox::VrMenuPage::Records
        ||s.vrHandMenu.page==kharvox::VrMenuPage::DevRecords){
        const bool dev=s.vrHandMenu.page==kharvox::VrMenuPage::DevRecords;
        if(pixelX>=800&&pixelX<927&&pixelY>=10&&pixelY<76)return 95;
        if(pixelX>=38&&pixelX<=493)
            for(int i=0;i<10;++i)
                if(pixelY>=136+i*45&&pixelY<176+i*45)
                    return 1000+s.vrHandMenu.recordsScroll+i;
        if(dev&&pixelX>=531&&pixelX<=915
            &&pixelY>=186&&pixelY<244)return 130;
        return -1;
    }
    if(s.vrHandMenu.page==kharvox::VrMenuPage::Stats
        ||s.vrHandMenu.page==kharvox::VrMenuPage::DevStats){
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
    if(s.vrHandMenu.page==kharvox::VrMenuPage::Mods
        ||s.vrHandMenu.page==kharvox::VrMenuPage::Dev){
        if(s.vrHandMenu.page==kharvox::VrMenuPage::Dev
            &&pixelX>=800&&pixelX<927&&pixelY>=10&&pixelY<76)
            return 95;
        // Secret gesture is restricted to the PUBLIC mod menu heading.
        if(s.vrHandMenu.page==kharvox::VrMenuPage::Mods
            &&pixelX>=38&&pixelX<=505&&pixelY>=18&&pixelY<75)
            return 1500;
        if(pixelX>=38&&pixelX<=493){
            for(int i=0;i<10;++i)
                if(pixelY>=136+i*45&&pixelY<176+i*45)
                    return 1000+s.vrHandMenu.modScroll+i;
        }
        if(pixelX>=531&&pixelX<=915){
            if(!s.vrHandMenu.navigation.settingsColumn){
                if(pixelY>=550&&pixelY<609)return 115; // CONFIGURE
                return -1; // right panel overview is read-only
            }
            const auto list=activeVrMenuEntries();
            const int row=s.vrHandMenu.selectedRow;
            if(row<0||row>=static_cast<int>(list.size()))return -1;
            const bool toggleable=s.vrHandMenu.page==kharvox::VrMenuPage::Mods
                &&kharvox::vrLiveModToggleable(list[row].mod);
            if(toggleable&&pixelY>=159&&pixelY<219)return 110;
            const int optionY=toggleable?248:186;
            for(int i=0;i<5;++i)
                if(pixelY>=optionY+i*68&&pixelY<optionY+58+i*68)
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
    if(pixelY>=151&&pixelY<209){
        if(pixelX>=531&&pixelX<701)return 20;
        if(pixelX>=715&&pixelX<917)return 21;
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
        m.consumeMenuTriggerUntilRelease=false;
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
            m.message="LEFT stick navigates; A selects; B backs out. RIGHT pointer + trigger also works.";
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
        // Swallow held release edges after Stats closes the menu automatically.
        // Never let the confirmation trigger fire a shot on the next frame.
        if(m.consumeMenuTriggerUntilRelease){
            if(!rightTrigger){
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
    m.rightTriggerPreviously=rightTrigger;
    // Sample real menu inputs on EVERY frame, including live Gunslinger
    // captures. The DEV monitor renders each bit separately and never
    // rewires gameplay bindings or commits this information to UserData.
    m.devInput={rightTrigger,leftTrigger,leftClick,rightClick,
        primaryWeaponGripDown,menuA,menuB,s.leftStick.x,s.leftStick.y};
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
        }else if(m.page==kharvox::VrMenuPage::Records
            ||m.page==kharvox::VrMenuPage::DevRecords){
            const bool dev=m.page==kharvox::VrMenuPage::DevRecords;
            m.page=dev?kharvox::VrMenuPage::Dev:kharvox::VrMenuPage::Stats;
            m.navigation={};
            m.pendingStatsReset=-1;
            m.confirmDeathResetUntilMs=0;
            m.message="Back to "+std::string(dev?"DEV tools.":"Stats.");
        }else if(m.page==kharvox::VrMenuPage::Stats
            ||m.page==kharvox::VrMenuPage::DevStats){
            const bool dev=m.page==kharvox::VrMenuPage::DevStats;
            if(m.navigation.settingsColumn){
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
            m.selectedRow=std::max(0,static_cast<int>(activeVrMenuEntries().size())-1);
            m.modScroll=0;
            m.navigation={};
            m.message="Back to MOD SETTINGS.";
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
        if(m.page==kharvox::VrMenuPage::Records
            ||m.page==kharvox::VrMenuPage::DevRecords){
            const bool dev=m.page==kharvox::VrMenuPage::DevRecords;
            const int count=dev?2:static_cast<int>(s.levelRecords.bestMs.size());
            kharvox::vrMenuNavigateStick(m.navigation,
                dev?s.leftStick.x:0.f,s.leftStick.y,
                count,m.recordSelected,dev?1:0);
            m.recordSelected=std::clamp(m.recordSelected,0,std::max(0,count-1));
            m.recordsScroll=std::clamp(m.recordsScroll,0,std::max(0,count-10));
            if(m.recordSelected<m.recordsScroll)m.recordsScroll=m.recordSelected;
            if(m.recordSelected>=m.recordsScroll+10)
                m.recordsScroll=m.recordSelected-9;
            const int pointed=triggerEdge?m.hovered:-1;
            const int chosen=pointed>=0?pointed:aEdge
                ?(m.navigation.settingsColumn&&dev?130:1000+m.recordSelected):-1;
            if(chosen==95){
                m.page=dev?kharvox::VrMenuPage::Dev:kharvox::VrMenuPage::Stats;
                m.navigation={};m.pendingStatsReset=-1;
                m.confirmDeathResetUntilMs=0;
            }else if(chosen>=1000&&chosen<1000+count){
                m.recordSelected=chosen-1000;
                m.navigation.actionIndex=0;
                m.navigation.settingsColumn=aEdge&&dev;
                m.pendingStatsReset=-1;
                m.confirmDeathResetUntilMs=0;
            }else if(dev&&chosen==130)
                handleVrLevelRecordReset(130);
        }else if(m.page==kharvox::VrMenuPage::Stats
            ||m.page==kharvox::VrMenuPage::DevStats){
            const bool dev=m.page==kharvox::VrMenuPage::DevStats;
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
        }else if(m.page==kharvox::VrMenuPage::Mods
            ||m.page==kharvox::VrMenuPage::Dev){
            const bool devPage=m.page==kharvox::VrMenuPage::Dev;
            const int n=static_cast<int>(rows.size());
            if(n>0)m.selectedRow=std::clamp(m.selectedRow,0,n-1);
            const auto optionCount=[&](const VrLiveMenuEntry& entry)->int{
                using kharvox::VrLiveMod;
                const auto mod=entry.mod;
                if(entry.developer!=kharvox::VrMenuDevOption::None)
                    return entry.developer==kharvox::VrMenuDevOption::Experimental?0:1;
                if(!kharvox::vrLiveModEditable(mod))return 0;
                switch(mod){
                    case VrLiveMod::WeaponGrip:return 1; // open calibration
                    case VrLiveMod::WeaponRecoil:return 1; // dedicated per-gun page
                    case VrLiveMod::Gunslinger:return 5;
                    case VrLiveMod::PhysicalCrouch:return 4; // toggle + 3 heights
                    case VrLiveMod::Killtime:return 3;
                    case VrLiveMod::TurnSpeed:return 3;
                    case VrLiveMod::PhysicalGloryKills:return 4;
                    case VrLiveMod::Painsaw:return 1; // toggle only
                    case VrLiveMod::DeathCounter:return 5; // toggle + four corners
                    default:return 0;
                }
            };
            const auto before=m.selectedRow;
            const int previousOptions=n>0?optionCount(rows[m.selectedRow]):0;
            // Overview is browse-only. Stick horizontal cannot silently
            // open a config or make A adjust a value. A / CONFIGURE enters
            // the right-side options; B returns to the overview.
            kharvox::vrMenuNavigateStick(m.navigation,
                m.navigation.settingsColumn?s.leftStick.x:0.f,
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
                    n>0&&!devPage
                        &&kharvox::vrLiveModToggleable(rows[m.selectedRow].mod)):-1;
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
                }else if(entry.developer==D::Experimental){
                    m.message="No experimental prototype installed yet.";
                }else{
                    m.navigation.settingsColumn=true;
                    m.navigation.actionIndex=0;
                    m.message="Press A or trigger on the tool switch.";
                }
                return true;
            };
            if(chosen==1500){
                if(kharvox::vrMenuDeveloperTap(m.devReveal,GetTickCount64())){
                    const int updatedCount=static_cast<int>(activeVrMenuEntries().size());
                    m.selectedRow=std::clamp(m.selectedRow,0,
                        std::max(0,updatedCount-1));
                    m.modScroll=0;
                    m.navigation={};
                    m.message=m.devReveal.visible
                        ?"Developer tools folder revealed."
                        :"Developer tools folder hidden.";
                    log(std::string("[VR-MODS] DEV folder ")
                        +(m.devReveal.visible?"shown":"hidden"));
                }
            }else if(chosen>=1000&&chosen<1000+n){
                // Browse changes highlight only: the right panel is a
                // gameplay description until A/CONFIGURE is explicitly
                // pressed, never an accidental ON/OFF or capture.
                const int newRow=chosen-1000;
                m.selectedRow=newRow;
                m.selectedMod=rows[newRow].mod;
                m.navigation.actionIndex=0;
                if(aEdge){
                    if(openDevEntry(rows[newRow])){
                        // DEV entries own navigation; never fall through to
                        // the ordinary mod toggle/configuration handlers.
                    }else if(m.selectedMod==kharvox::VrLiveMod::Stats){
                        m.page=kharvox::VrMenuPage::Stats;
                        m.statsSelected=0;
                        m.navigation={};
                        m.message="Choose an item and configure its display.";
                    }else if(m.selectedMod==kharvox::VrLiveMod::WeaponGrip
                        ||m.selectedMod==kharvox::VrLiveMod::WeaponRecoil){
                        // Both calibration mods have their own complete page.
                        const bool grip=m.selectedMod==kharvox::VrLiveMod::WeaponGrip;
                        m.page=grip?kharvox::VrMenuPage::WeaponGrip
                            :kharvox::VrMenuPage::WeaponRecoil;
                        m.navigation={};
                        m.message=grip
                            ?"Select weapon and hand; trigger calibrates."
                            :"Select weapon; tune kick and recovery.";
                    }else{
                        m.navigation.settingsColumn=true;
                        m.navigation.usingStick=true;
                        m.message="Configuring "+std::string(rows[newRow].title)
                            +". Press B for mod descriptions.";
                    }
                }else{
                    m.navigation.settingsColumn=false;
                    m.message="Browse mod details. A opens configuration.";
                }
            }else if(chosen==115&&n>0){
                m.selectedMod=rows[m.selectedRow].mod;
                if(openDevEntry(rows[m.selectedRow])){
                    // Submenu or DEV toggle panel opened.
                }else if(m.selectedMod==kharvox::VrLiveMod::Stats){
                    m.page=kharvox::VrMenuPage::Stats;
                    m.statsSelected=0;
                    m.navigation={};
                    m.message="Choose an item and configure its display.";
                }else if(m.selectedMod==kharvox::VrLiveMod::WeaponGrip
                    ||m.selectedMod==kharvox::VrLiveMod::WeaponRecoil){
                    const bool grip=m.selectedMod==kharvox::VrLiveMod::WeaponGrip;
                    m.page=grip?kharvox::VrMenuPage::WeaponGrip
                        :kharvox::VrMenuPage::WeaponRecoil;
                    m.navigation={};
                    m.message=grip
                        ?"Select weapon and hand; trigger calibrates."
                        :"Select weapon; tune kick and recovery.";
                }else{
                    m.navigation.settingsColumn=true;
                    m.navigation.actionIndex=0;
                    m.navigation.usingStick=false;
                    m.message="Configuring "+std::string(rows[m.selectedRow].title)
                        +". B returns to overview.";
                }
            }else if(devPage&&chosen==95){
                m.page=kharvox::VrMenuPage::Mods;
                m.selectedRow=std::max(0,static_cast<int>(activeVrMenuEntries().size())-1);
                m.modScroll=0;m.navigation={};
                m.message="Back to MOD SETTINGS.";
            }else if(devPage&&m.navigation.settingsColumn&&chosen==130&&n>0){
                using D=kharvox::VrMenuDevOption;
                const auto kind=rows[m.selectedRow].developer;
                if(kind==D::WeaponAlignment){
                    m.devWeaponAlignment=!m.devWeaponAlignment;
                    m.message=m.devWeaponAlignment?"Weapon anchor readout ON.":"Weapon anchor readout OFF.";
                }else if(kind==D::HandFocus){
                    s.customMods.handFocusedRs=!s.customMods.handFocusedRs;
                    m.message=s.customMods.handFocusedRs
                        ?"Hand Focus enabled for this session."
                        :"Head Focus enabled for this session.";
                }else if(kind==D::HolsterZones){
                    m.devHolsterZones=!m.devHolsterZones;
                    m.message=m.devHolsterZones?"Gunslinger zone visualiser ON.":"Gunslinger zone visualiser OFF.";
                }else if(kind==D::ControllerMonitor){
                    m.devControllerInputs=!m.devControllerInputs;
                    m.message=m.devControllerInputs?"Gunslinger input monitor ON.":"Gunslinger input monitor OFF.";

                }
            }else if(!devPage&&m.navigation.settingsColumn
                &&(chosen==110||(chosen>=130&&chosen<=134))){
                handleVrLiveMenuAction(chosen);
                if(m.page!=kharvox::VrMenuPage::Mods)m.navigation={};
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
        }else{
            const auto previousWeapon=m.selectedWeapon;
            const auto selectedKind=KharvoxWeaponKindFromKey(m.weapon());
            const bool heavy=selectedKind==KharvoxWeaponKind::GaussCannon
                ||selectedKind==KharvoxWeaponKind::Chaingun;
            kharvox::vrMenuNavigateStick(m.navigation,s.leftStick.x,s.leftStick.y,
                10,m.selectedWeapon,selectedKind==KharvoxWeaponKind::GaussCannon
                    ?7:heavy?6:5);
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
                0,selectedNow==KharvoxWeaponKind::GaussCannon
                    ?6:heavyNow?5:4);
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
                    m.selectedWeapon,heavyNow,
                    selectedNow==KharvoxWeaponKind::GaussCannon):-1;
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
            }else if(chosen==20){
                m.rightHand=false;
            }else if(chosen==21){
                m.rightHand=true;
            }
            else if(chosen==31)resetVrMenuSelectedWeapon();
            else if(chosen==32){
                if(heavyNow){
                    m.alternateProfile=!m.alternateProfile;
                    m.message=m.alternateProfile
                        ?"Alt Fire profile selected; configure your equipped attachment."
                        :"Normal weapon profile selected.";
                }
            }else if(chosen==33
                &&selectedNow==KharvoxWeaponKind::GaussCannon){
                const bool prior=s.vrLiveSettings.gaussRequireTwoHandFire;
                s.vrLiveSettings.gaussRequireTwoHandFire=!prior;
                if(!kharvox::vrLiveSave(kharvox::runtimePathA(
                        "vr_live_mods_saved.cfg"),s.vrLiveSettings)){
                    s.vrLiveSettings.gaussRequireTwoHandFire=prior;
                    m.message="Gauss two-hand firing setting could not be saved.";
                }else{
                    m.message=s.vrLiveSettings.gaussRequireTwoHandFire
                        ?"Gauss: both Grips must hold the weapon before firing."
                        :"Gauss: normal trigger firing restored.";
                    log("[GAUSS-FIRE-GRIP] "+m.message);
                }
            }else if(chosen==30)requestVrSelectedWeaponCalibration(displayTime);
            m.leftTriggerPreviously=leftTrigger;
        }
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
