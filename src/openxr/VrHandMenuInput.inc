// All VR menu inputs are handled before DOOM's native melee/pause/calibration
// routes. The menu owns both clicks and triggers while it is visible; trigger
// save is edge-based and cannot fire the selected weapon.
void refreshVrHandMenuSavedStatus(){
    auto& m=s.vrHandMenu;
    for(size_t i=0;i<kharvox::kVrCalibrationWeapons.size();++i){
        const std::string key=kharvox::kVrCalibrationWeapons[i];
        // Show PRIMARY gun-in-hand calibrations. The off-hand support grip
        // is separate and does NOT make an uncalibrated left-primary gun
        // appear as "LEFT calibrated".
        m.rightSaved[i]=kharvox::hasSavedVrPrimaryPose(s.rightGripSettings,key,false);
        m.leftSaved[i]=kharvox::hasSavedVrPrimaryPose(s.rightGripSettings,key,true);
    }
}
void updateVrMenuPreview(){
    auto& m=s.vrHandMenu;
    m.previewValid=false;
    const auto weapon=KharvoxWeaponKindFromKey(m.weapon());
    const auto& placement=s.weaponHandLeft?s.rightGripController:s.leftGripController;
    if(weapon==KharvoxWeaponKind::Unknown||weapon!=KharvoxWeaponCurrentKind()
        ||s.weaponHandLeft!=m.calibratingWeaponLeft
        ||!s.lastWeaponRenderedValid||s.lastWeaponRenderedKind!=weapon
        ||!placement.valid||!placement.positionTracked)return;
    const bool primaryHand=m.rightHand!=s.weaponHandLeft;
    XrVector3f probe=placement.position;
    if(primaryHand){
        const auto forward=kharvox::liveHeavyGripProbePoint(
            {probe.x,probe.y,probe.z},
            {placement.orientation.x,placement.orientation.y,
             placement.orientation.z,placement.orientation.w,true});
        probe={forward.x,forward.y,forward.z};
    }
    const auto inverted=conjugate(s.lastWeaponRenderedTrackingRotation);
    const auto local=rotateVector(inverted,
        subtractVector(probe,s.lastWeaponRenderedTrackingAnchor));
    const auto rotation=normalizeQuaternion(multiply(
        inverted,placement.orientation));
    m.previewPosition={local.x,local.y,local.z};
    m.previewRotation={rotation.x,rotation.y,rotation.z,rotation.w,true};
    m.previewValid=(primaryHand
        ?kharvox::validRightHandVisualPlacement(m.previewPosition)
        :kharvox::validRightHandPlacement(m.previewPosition))
        &&kharvox::validRightHandRotation(m.previewRotation);
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
            &why,s.weaponHandLeft);
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
    m.previewValid=false;
    s.primaryVisualLocked=false;
    s.supportVisualLocked=false;
    m.message="Live weapon calibration. RIGHT TRIGGER saves.";
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
            if(row>=0&&row<static_cast<int>(list.size())
                &&list[row].devSection){
                const auto dev=devVrMenuEntries();
                for(int i=0;i<10;++i){
                    const int index=s.vrHandMenu.devScroll+i;
                    if(index>=static_cast<int>(dev.size()))break;
                    if(pixelY>=186+i*39&&pixelY<222+i*39)
                        return 1200+index;
                }
                return -1; // DEV status cannot activate gameplay
            }
            if(pixelY>=150&&pixelY<210)return 110;
            for(int i=0;i<5;++i)
                if(pixelY>=248+i*68&&pixelY<306+i*68)return 130+i;
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
    }
    return -1;
}
bool serviceVrHandMenuInput(XrTime displayTime,bool gameplay,bool leftClick,bool rightClick,
                            bool rightTrigger,bool leftTrigger,
                            bool primaryWeaponGripDown,bool menuA,bool menuB){
    auto& m=s.vrHandMenu;
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
            updateVrMenuPreview();
            if(triggerEdge||aEdge)saveVrHandMenuCalibration();
        }
    }else if(!bEdge&&m.mode==kharvox::VrHandMenuMode::Menu){
        m.hovered=vrMenuRayTarget(m.cursorX,m.cursorY);
        const auto rows=activeVrMenuEntries();
        if(m.page==kharvox::VrMenuPage::Mods){
            const int n=static_cast<int>(rows.size());
            if(n>0)m.selectedRow=std::clamp(m.selectedRow,0,n-1);
            const auto devEntries=devVrMenuEntries();
            const auto optionCount=[&](const VrLiveMenuEntry& entry)->int{
                using kharvox::VrLiveMod;
                if(entry.devSection)return static_cast<int>(devEntries.size());
                const auto mod=entry.mod;
                if(!kharvox::vrLiveModEditable(mod))return 0;
                switch(mod){
                    case VrLiveMod::WeaponGrip:return 2; // toggle + open calibration
                    case VrLiveMod::Gunslinger:return 6;
                    case VrLiveMod::PhysicalCrouch:return 4;
                    case VrLiveMod::Killtime:return 4;
                    case VrLiveMod::PhysicalGloryKills:return 5;
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
                if(rows[m.selectedRow].devSection){
                    const int devN=static_cast<int>(devEntries.size());
                    m.devScroll=std::clamp(m.devScroll,0,std::max(0,devN-10));
                    if(m.navigation.actionIndex<m.devScroll)
                        m.devScroll=m.navigation.actionIndex;
                    if(m.navigation.actionIndex>=m.devScroll+10)
                        m.devScroll=m.navigation.actionIndex-9;
                }
                m.modScroll=std::clamp(m.modScroll,0,std::max(0,n-10));
                if(m.selectedRow<m.modScroll)m.modScroll=m.selectedRow;
                if(m.selectedRow>=m.modScroll+10)m.modScroll=m.selectedRow-9;
            }
            const int pointed=triggerEdge?m.hovered:-1;
            const int chosen=pointed>=0?pointed:
                aEdge?kharvox::vrMenuModsGamepadTarget(m.navigation,
                    m.selectedRow,n,n>0?optionCount(rows[m.selectedRow]):0):-1;
            if(pointed>=0)m.navigation.usingStick=false;
            if(chosen>=1000&&chosen<1000+n){
                m.selectedRow=chosen-1000;
                m.selectedMod=rows[m.selectedRow].mod;
                m.navigation.actionIndex=0;
                // A on a highlighted mod moves focus to its options.
                if(aEdge){m.navigation.settingsColumn=true;
                    m.navigation.usingStick=true;}
                m.message=rows[m.selectedRow].devSection
                    ?"DEV: read-only mods and locked launcher overrides."
                    :kharvox::vrLiveModEditable(m.selectedMod)
                        ?"Live options on the RIGHT.":"Read-only; desktop launcher only.";
            }else if(chosen>=1200&&chosen<1200+static_cast<int>(devEntries.size())
                &&rows[m.selectedRow].devSection){
                m.navigation.settingsColumn=true;
                m.navigation.actionIndex=chosen-1200;
                m.navigation.usingStick=false;
                m.message=std::string(devEntries[chosen-1200].title)
                    +" is read-only in headset.";
            }else if(!rows[m.selectedRow].devSection
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
                10,m.selectedWeapon,heavy?6:5);
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
                0,heavyNow?5:4);
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
            else if(chosen==32){
                if(heavyNow){
                    m.alternateProfile=!m.alternateProfile;
                    m.message=m.alternateProfile?"Alternate handle selected.":"Normal handle selected.";
                }
            }else if(chosen==30)requestVrSelectedWeaponCalibration(displayTime);
            m.leftTriggerPreviously=leftTrigger;
        }
    }
    // Keep the ordinary menu entirely input-muted. ONLY during LIVE
    // calibration, pass the CURRENT weapon hand's physical Grip directly to
    // native alternate fire, allowing Gauss Siege / Chaingun Turret to deploy
    // while the other hand positions the grip. The RIGHT trigger still SAVES
    // calibration; it never fires the weapon. All other inputs stay muted.
    const bool liveAltFire=kharvox::vrCalibrationNativeAltFireAllowed(
        m.mode,gameplay,s.customMods.offHandGripCapture,
        KharvoxWeaponCurrentKind()==KharvoxWeaponKindFromKey(m.weapon()),
        s.weaponHandLeft==m.calibratingWeaponLeft,primaryWeaponGripDown);
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
