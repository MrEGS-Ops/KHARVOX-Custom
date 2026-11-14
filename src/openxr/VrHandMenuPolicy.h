#pragma once
#include "RightHandGripConfig.h"
#include "VrLiveModsConfig.h"
#include <array>
#include <string>
#include <vector>
#include <sstream>
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <cmath>
#include <cstdint>

// In-headset weapon-hand calibration. Left support hand saves TRANSLATION
// only. Its orientation is intentionally established anew on every Grip.
namespace kharvox {
inline constexpr std::array<const char*,10> kVrCalibrationWeapons{
    "pistol","shotgun","heavy_assault_rifle","plasma_rifle",
    "rocket_launcher","super_shotgun","gauss_cannon","chaingun",
    "bfg","chainsaw"
};
enum class VrHandMenuMode { Closed, Menu, Calibrating };
// One calibration slot per firing state. Neither mode is bound to a
// particular equipped Gauss or Chaingun attachment.
inline constexpr bool vrHeavyAlternateGripActive(
    bool menuOpen,bool matchingSelectedWeapon,bool selectedAlternate,
    bool nativeSecondaryGrip,bool gaussChargeStillActive) noexcept {
    if(menuOpen&&matchingSelectedWeapon)return selectedAlternate;
    return nativeSecondaryGrip||gaussChargeStillActive;
}
// Grip-held aim preview works on every primary weapon.
// Alt Fire is engaged automatically when its mode is selected;
// Grip itself is reserved for testing position, never alt fire in calibration.
inline bool vrPrimaryGripHoldPreviewAllowed(VrHandMenuMode mode,
    bool calibratingPrimary,const std::string& weapon) noexcept {
    return mode==VrHandMenuMode::Calibrating && calibratingPrimary
        && validVrCalibrationWeapon(weapon);
}
// Only LIVE weapon-pose calibration forwards the physically held PRIMARY
// weapon Grip to DOOM's native alternate-fire input (XInput left trigger).
// Never allow it from the visible settings menu, the wrong equipped weapon,
// a changed weapon hand, or outside live gameplay.
inline constexpr bool vrCalibrationNativeAltFireAllowed(
    VrHandMenuMode mode,bool gameplay,bool calibrationEnabled,
    bool selectedWeaponEquipped,bool weaponHandUnchanged,
    bool primaryWeaponGripDown) noexcept {
    return mode==VrHandMenuMode::Calibrating && gameplay
        && calibrationEnabled && selectedWeaponEquipped
        && weaponHandUnchanged && primaryWeaponGripDown;
}
// Single physical RIGHT thumbstick hold >= 1000ms opens/closes the menu.
// A short click is untouched, so Hand Focus retains its original input.
struct VrMenuHoldShortcut {
    bool held{},triggered{};
    std::uint64_t beganAt{};
};
enum class VrMenuShortcutAction {None,Toggle,ConsumedRelease};
inline VrMenuShortcutAction updateVrMenuHoldShortcut(
    VrMenuHoldShortcut& state,bool rightClick,std::uint64_t nowMs) noexcept {
    if(!rightClick){
        const bool consumed=state.triggered;
        state.held=false;state.triggered=false;state.beganAt=0;
        return consumed?VrMenuShortcutAction::ConsumedRelease
            :VrMenuShortcutAction::None;
    }
    if(!state.held){
        state.held=true;state.triggered=false;state.beganAt=nowMs;
    }
    if(!state.triggered&&nowMs>=state.beganAt
        &&nowMs-state.beganAt>=1000ULL){
        state.triggered=true;
        return VrMenuShortcutAction::Toggle;
    }
    return VrMenuShortcutAction::None;
}
enum class VrMenuPage {Mods,WeaponGrip,WeaponRecoil};
// Independent, edge-triggered left-stick navigation. It never modifies the
// in-game movement or the right-hand laser/pointer input outside this menu.
struct VrMenuNavigation {
    bool settingsColumn{},usingStick{};
    bool verticalCentered{true},horizontalCentered{true};
    int actionIndex{}; // 0=toggle on Mods, 1..n=setting actions
};
inline bool vrMenuButtonEdge(bool down,bool& previously) noexcept {
    const bool pressed=down&&!previously;
    previously=down;
    return pressed;
}
inline void vrMenuNavigateStick(VrMenuNavigation& nav,float x,float y,
    int listCount,int& selectedIndex,int settingsCount) noexcept {
    if(std::isfinite(x)){
        if(std::abs(x)<.3f)nav.horizontalCentered=true;
        else if(std::abs(x)>.75f&&nav.horizontalCentered){
            nav.horizontalCentered=false;
            nav.settingsColumn=x>0.f;
            nav.actionIndex=0;
            nav.usingStick=true;
        }
    }
    if(std::isfinite(y)){
        if(std::abs(y)<.3f)nav.verticalCentered=true;
        else if(std::abs(y)>.75f&&nav.verticalCentered){
            nav.verticalCentered=false;
            nav.usingStick=true;
            if(nav.settingsColumn){
                if(settingsCount>0)
                    nav.actionIndex=(nav.actionIndex+(y<0.f?1:settingsCount-1))%settingsCount;
            }else if(listCount>0){
                selectedIndex=(selectedIndex+(y<0.f?1:listCount-1))%listCount;
            }
        }
    }
}
inline int vrMenuModsGamepadTarget(const VrMenuNavigation& nav,
    int selectedRow,int count,int actionCount,bool toggleable=true) noexcept {
    if(!nav.settingsColumn)return selectedRow>=0&&selectedRow<count
        ?1000+selectedRow:-1;
    if(nav.actionIndex<0||nav.actionIndex>=actionCount)return -1;
    // Only Physical Crouch and PAINSAW have a checkbox. For every other
    // mod, first gamepad action maps DIRECTLY to its first settings button.
    return toggleable?(nav.actionIndex==0?110:130+nav.actionIndex-1)
        :130+nav.actionIndex;
}
// Dedicated recoil tuning: 9 firearms + separate Minigun Turret mode (10 rows).
// Controller settings are independent of the primary/off-hand pose profiles.
// The policy header intentionally has no OpenXR/weapon recoil runtime dependency.
inline constexpr int kVrRecoilMenuProfileCount=10;
inline constexpr int kVrRecoilMenuEntryCount=11; // final row: shared dynamics
inline int vrMenuRecoilGamepadTarget(const VrMenuNavigation& nav,
                                    int weapon) noexcept {
    return nav.settingsColumn
        ?(nav.actionIndex>=0&&nav.actionIndex<6?140+nav.actionIndex:-1)
        :(weapon>=0&&weapon<kVrRecoilMenuEntryCount?200+weapon:-1);
}
inline int vrMenuGripGamepadTarget(const VrMenuNavigation& nav,
    int selectedWeapon,bool heavy) noexcept {
    if(!nav.settingsColumn)return selectedWeapon>=0&&selectedWeapon<10
        ?selectedWeapon:-1;
    // No automatic RIGHT-to-LEFT mirroring: each physical hand is
    // calibrated independently. Preserve CALIBRATE, RESET and BACK.
    constexpr std::array<int,6> heavyActions{20,21,32,30,31,95};
    constexpr std::array<int,5> normalActions{20,21,30,31,95};
    return nav.actionIndex>=0&&nav.actionIndex<(heavy?6:5)
        ?(heavy?heavyActions[nav.actionIndex]:normalActions[nav.actionIndex]):-1;
}
// Grip RELEASED: the opposite controller drives the primary glove's live
// position/rotation. Grip HELD: the currently valid placement is frozen,
// enabling isolated 1cm joystick nudges (not free-controller movements).
// A Grip press with no valid placement may initialize its first frame.
inline constexpr bool vrWeaponManualCalibrationLive(
    VrHandMenuMode mode,bool gripHeld,bool frozenPreviewValid) noexcept {
    return mode==VrHandMenuMode::Calibrating
        &&(!gripHeld||!frozenPreviewValid);
}

// OpenXR reports POSITION_VALID even while POSITION_TRACKED is temporarily
// false (predicted location). Prefer real tracked GRIP; fall back to tracked
// AIM (the free controller's visible laser), then either valid position.
// Previously requiring GRIP POSITION_TRACKED made normal calibration reject
// an otherwise valid moving controller and left the gun glove fixed.
template<class ControllerPose>
inline const ControllerPose& vrCalibrationFreeControllerPose(
    const ControllerPose& grip,const ControllerPose& aim) noexcept {
    if(grip.valid&&grip.positionTracked)return grip;
    if(aim.valid&&aim.positionTracked)return aim;
    if(grip.valid)return grip;
    return aim;
}
// Native GRIP tracks the wrist orientation, whereas the tracked AIM position
// follows the visible controller laser reliably. Use AIM for the free-hand
// calibration TRANSLATION when available; preserve independent GRIP rotation.
// This avoids stale predicted GRIP positions pivoting the glove in place.
template<class ControllerPose>
inline ControllerPose vrCalibrationLivePose(
    const ControllerPose& grip,const ControllerPose& aim) noexcept {
    ControllerPose live=vrCalibrationFreeControllerPose(grip,aim);
    if(aim.valid&&aim.positionTracked)live.position=aim.position;
    if(grip.valid)live.orientation=grip.orientation;
    return live;
}
// SUPPORT calibration must sample the physical support controller GRIP
// position, which is also what renders the ordinary free-hand glove. AIM
// and GRIP have different origins: copying the AIM translation made the
// support glove jump forward on entry and saved a mismatched point.
// No AIM fallback when GRIP is invalid: reject capture rather than write
// an incorrect profile, even if the AIM ray is still tracked.
template<class ControllerPose>
inline const ControllerPose& vrSupportCalibrationPhysicalGrip(
    const ControllerPose& grip,const ControllerPose&) noexcept {
    return grip;
}

// Fine-tune runs ONLY during primary-weapon calibration with the physical
// weapon-hand GRIP held. Each stick is independent. A quick flick = 1 cm.
// Holding past 340 ms repeats 1 cm every 175 ms (~5.7 cm/second).
// LEFT stick X = gun left/right; LEFT stick Y = gun forward/back (local Z);
// RIGHT stick Y = gun up/down. No controller/glove motion or gun rotation.
// Sticks must be centred on Grip entry, and between direction reversals;
// changing grip/mode immediately cancels all held-repeat state. At most one
// step per stick per frame, even after frame delays or tracking stalls.
inline constexpr std::uint64_t kVrWeaponFineTuneRepeatDelayMs=340;
inline constexpr std::uint64_t kVrWeaponFineTuneRepeatIntervalMs=175;
struct VrWeaponFineTuneEdges {
    bool active{},leftArmed{},rightArmed{};
    int leftDirection{},rightDirection{};
    std::uint64_t leftNextStepAt{},rightNextStepAt{};
};
inline RightHandGripOffset vrWeaponFineTuneCentimeterStep(
    VrWeaponFineTuneEdges& state,bool allowed,bool gripHeld,
    float leftX,float leftY,float rightY,std::uint64_t nowMs) noexcept {
    RightHandGripOffset step{};
    if(!allowed||!gripHeld||!std::isfinite(leftX)
        ||!std::isfinite(leftY)||!std::isfinite(rightY)){
        state={};
        return step;
    }
    constexpr float neutral=.28f,activation=.78f,cm=.01f;
    const bool leftCentered=std::abs(leftX)<neutral&&std::abs(leftY)<neutral;
    const bool rightCentered=std::abs(rightY)<neutral;
    if(!state.active){
        state.active=true;
        state.leftArmed=leftCentered;
        state.rightArmed=rightCentered;
        return step; // first GRIP frame never moves the gun accidentally
    }

    const bool leftStrong=std::max(std::abs(leftX),std::abs(leftY))
        >=activation;
    const int leftCommand=!leftStrong?0:std::abs(leftX)>=std::abs(leftY)
        ?(leftX>=0.f?1:-1):(leftY>=0.f?2:-2);
    if(leftCentered){
        state.leftArmed=true;
        state.leftDirection=0;
        state.leftNextStepAt=0;
    }else if(leftCommand!=0){
        bool emit=false;
        if(state.leftArmed){
            state.leftArmed=false;
            state.leftDirection=leftCommand;
            state.leftNextStepAt=nowMs+kVrWeaponFineTuneRepeatDelayMs;
            emit=true;
        }else if(state.leftDirection==leftCommand
            &&nowMs>=state.leftNextStepAt){
            state.leftNextStepAt=nowMs+kVrWeaponFineTuneRepeatIntervalMs;
            emit=true;
        }else if(state.leftDirection!=leftCommand){
            // A reversal/axis change requires recenter, not a sudden jump.
            state.leftDirection=0;
            state.leftNextStepAt=0;
        }
        if(emit){
            if(leftCommand==1)step.x=cm;
            else if(leftCommand==-1)step.x=-cm;
            else if(leftCommand==2)step.z=-cm;
            else step.z=cm;
        }
    }

    const int rightCommand=std::abs(rightY)>=activation
        ?(rightY>=0.f?1:-1):0;
    if(rightCentered){
        state.rightArmed=true;
        state.rightDirection=0;
        state.rightNextStepAt=0;
    }else if(rightCommand!=0){
        bool emit=false;
        if(state.rightArmed){
            state.rightArmed=false;
            state.rightDirection=rightCommand;
            state.rightNextStepAt=nowMs+kVrWeaponFineTuneRepeatDelayMs;
            emit=true;
        }else if(state.rightDirection==rightCommand
            &&nowMs>=state.rightNextStepAt){
            state.rightNextStepAt=nowMs+kVrWeaponFineTuneRepeatIntervalMs;
            emit=true;
        }else if(state.rightDirection!=rightCommand){
            state.rightDirection=0;
            state.rightNextStepAt=0;
        }
        if(emit)step.y=rightCommand>0?cm:-cm;
    }
    return step;
}
// One calibration session can alternate Grip-held gun fine-tuning and
// Grip-released wrist rotation without committing either until SAVE.
// Every Grip release starts a fresh relative rotation reference, retaining
// the exact glove rotation from the previous mode with no jump.
struct VrPrimaryRotationEdit {
    bool referenceValid{};
    RightHandGripRotation controllerAtRelease{};
    RightHandGripRotation gloveAtRelease{};
};
inline RightHandGripRotation vrCalibrationMultiplyRotation(
    const RightHandGripRotation& a,
    const RightHandGripRotation& b) noexcept {
    return {
        a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y,
        a.w*b.y-a.x*b.z+a.y*b.w+a.z*b.x,
        a.w*b.z+a.x*b.y-a.y*b.x+a.z*b.w,
        a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z,true};
}
inline RightHandGripRotation vrCalibrationRelativeRotation(
    VrPrimaryRotationEdit& edit,bool weaponGripHeld,
    const RightHandGripRotation& freeControllerRotation,
    const RightHandGripRotation& currentGloveRotation) noexcept {
    if(weaponGripHeld){
        edit.referenceValid=false;
        return currentGloveRotation; // freeze rotation during gun-only nudges
    }
    if(!validRightHandRotation(freeControllerRotation)
        ||!validRightHandRotation(currentGloveRotation))
        return currentGloveRotation; // tracking gap: keep latest valid draft
    if(!edit.referenceValid){
        edit.controllerAtRelease=freeControllerRotation;
        edit.gloveAtRelease=currentGloveRotation;
        edit.referenceValid=true;
        return currentGloveRotation; // transition frame never snaps the glove
    }
    const auto& q=edit.controllerAtRelease;
    const RightHandGripRotation inverse{-q.x,-q.y,-q.z,q.w,true};
    const auto delta=vrCalibrationMultiplyRotation(freeControllerRotation,inverse);
    auto result=vrCalibrationMultiplyRotation(delta,edit.gloveAtRelease);
    const float magnitude=std::sqrt(result.x*result.x+result.y*result.y
        +result.z*result.z+result.w*result.w);
    if(!(magnitude>0.00001f)||!std::isfinite(magnitude))
        return currentGloveRotation;
    result.x/=magnitude;result.y/=magnitude;result.z/=magnitude;
    result.w/=magnitude;result.valid=true;
    return result;
}

struct VrHandMenuState {
    VrHandMenuMode mode{VrHandMenuMode::Closed};
    VrMenuPage page{VrMenuPage::Mods};
    VrLiveMod selectedMod{VrLiveMod::WeaponGrip};
    int modScroll{};
    int selectedRow{};
    VrLiveCapture liveCapture{VrLiveCapture::None};
    bool autoSelectingWeapon{};
    unsigned long long autoSelectionDeadlineMs{};
    int pendingNativeSelection{}; // 1=BFG (Y), 2=Chainsaw (X)
    long long nativeSelectionPulseUntil{};
    int selectedWeapon{1};
    int selectedRecoilWeapon{1}; // separate from physical hand calibration
    bool rightHand{true};
    // Both Gauss and Chaingun: NORMAL / ALT FIRE. The latter is a
    // generic secondary-fire grip slot, regardless of installed attachment.
    bool alternateProfile{},calibrationAlternateProfile{};
    // Latch holster ownership at capture start; switching hands while
    // calibrating never writes a pose to the wrong physical hand.
    bool calibratingWeaponLeft{};
    VrMenuHoldShortcut shortcut{};
    VrMenuNavigation navigation{};
    bool rightTriggerPreviously{},leftTriggerPreviously{};
    bool menuAPreviously{},menuBPreviously{},consumeMenuFaceUntilRelease{};
    int hovered{-1};
    float cursorX{-1.f},cursorY{-1.f};
    bool previewValid{};
    bool primaryGripPreviewHeld{}; // freezes valid live pose while Grip held, permits 1cm nudges
    bool calibrationGripHeld{}; // raw physical weapon Grip for contextual VR help, independent of tracking
    VrPrimaryRotationEdit rotationEdit{}; // resets its free-controller reference whenever Grip is held
    std::uint64_t lastPreviewDiagnosticAt{}; // throttled calibration pose log
    VrWeaponFineTuneEdges fineTuneEdges{};
    RightHandGripOffset fineTuneAccumulated{}; // unsaved, local X/Y/Z metres
    float primaryLateralBase{}; // pinned X, not sampled from moving preview
    RightHandGripOffset previewPosition{};
    RightHandGripRotation previewRotation{};
    std::array<bool,10> rightSaved{},leftSaved{};
    std::string message;
    const char* weapon() const noexcept {return kVrCalibrationWeapons[static_cast<size_t>(selectedWeapon)];}
};
inline bool isVrMenuInputOwner(const VrHandMenuState& m) noexcept {
    return m.mode!=VrHandMenuMode::Closed;
}
inline bool hasSavedVrPrimaryPose(const RightHandGripSettings& settings,
    const std::string&weapon,bool leftHand) {
    const auto& positions=leftHand?settings.leftVisualPositions:settings.visualPositions;
    const auto& rotations=leftHand?settings.leftVisualRotations:settings.visualRotations;
    return positions.find(weapon)!=positions.end()
        &&rotations.find(weapon)!=rotations.end();
}
// Factory files are seeded into UserData on a fresh installation. The mere
// presence of a profile is NOT evidence of a user calibration, so the
// LEFT/RIGHT/BOTH UI states must compare values against the packaged factory.
inline bool vrPrimaryPoseChangedFromFactory(
    const RightHandGripSettings& current,
    const RightHandGripSettings& factory,
    const std::string& weapon,bool leftHand) noexcept {
    const auto& positions=leftHand?current.leftVisualPositions:current.visualPositions;
    const auto& rotations=leftHand?current.leftVisualRotations:current.visualRotations;
    const auto& basePositions=leftHand?factory.leftVisualPositions:factory.visualPositions;
    const auto& baseRotations=leftHand?factory.leftVisualRotations:factory.visualRotations;
    const auto positionChanged=[&](const std::string& key) {
        const auto found=positions.find(key);
        if(found==positions.end())return false; // no user grip data at all
        const auto base=basePositions.find(key);
        if(base==basePositions.end())return true;
        const auto a=found->second,b=base->second;
        return std::abs(a.x-b.x)>.0001f||std::abs(a.y-b.y)>.0001f
            ||std::abs(a.z-b.z)>.0001f;
    };
    const auto rotationChanged=[&](const std::string& key) {
        const auto found=rotations.find(key);
        if(found==rotations.end())return false;
        const auto base=baseRotations.find(key);
        if(base==baseRotations.end())return true;
        const auto a=found->second,b=base->second;
        // q and -q represent the same orientation.
        const auto similar=[&](float sign){
            return std::abs(a.x-sign*b.x)<.0001f
                &&std::abs(a.y-sign*b.y)<.0001f
                &&std::abs(a.z-sign*b.z)<.0001f
                &&std::abs(a.w-sign*b.w)<.0001f;
        };
        return !similar(1.f)&&!similar(-1.f);
    };
    // Require a complete primary profile; support-only writes must not make
    // the primary hand appear calibrated.
    if(!hasSavedVrPrimaryPose(current,weapon,leftHand))return false;
    if(positionChanged(weapon)||rotationChanged(weapon))return true;
    if(weapon=="chaingun"&&(positionChanged("chaingun_rotor")
        ||rotationChanged("chaingun_rotor")))return true;
    const std::string alternate=weapon=="gauss_cannon"
        ?"gauss_cannon_siege":weapon=="chaingun"
            ?"chaingun_turret":"";
    if(alternate.empty())return false;
    if(leftHand)return positionChanged(alternate)||rotationChanged(alternate);
    // Right-heavy alternate values live in dedicated fields rather than the
    // generic profile map. Compare those as well.
    if(weapon=="gauss_cannon"){
        if(current.gaussSiegeHandValid!=factory.gaussSiegeHandValid)return true;
        if(current.gaussSiegeHandValid){
            const auto a=current.gaussSiegeHand,b=factory.gaussSiegeHand;
            if(std::abs(a.x-b.x)>.0001f||std::abs(a.y-b.y)>.0001f
               ||std::abs(a.z-b.z)>.0001f)return true;
        }
        return current.gaussSiegeRotation.valid
            &&(!factory.gaussSiegeRotation.valid
                ||std::abs(current.gaussSiegeRotation.x-factory.gaussSiegeRotation.x)>.0001f
                ||std::abs(current.gaussSiegeRotation.y-factory.gaussSiegeRotation.y)>.0001f
                ||std::abs(current.gaussSiegeRotation.z-factory.gaussSiegeRotation.z)>.0001f
                ||std::abs(current.gaussSiegeRotation.w-factory.gaussSiegeRotation.w)>.0001f);
    }
    if(current.chaingunTurretHandValid!=factory.chaingunTurretHandValid)return true;
    if(current.chaingunTurretHandValid){
        const auto a=current.chaingunTurretHand,b=factory.chaingunTurretHand;
        if(std::abs(a.x-b.x)>.0001f||std::abs(a.y-b.y)>.0001f
            ||std::abs(a.z-b.z)>.0001f)return true;
    }
    return current.chaingunTurretRotation.valid
        &&(!factory.chaingunTurretRotation.valid
            ||std::abs(current.chaingunTurretRotation.x-factory.chaingunTurretRotation.x)>.0001f
            ||std::abs(current.chaingunTurretRotation.y-factory.chaingunTurretRotation.y)>.0001f
            ||std::abs(current.chaingunTurretRotation.z-factory.chaingunTurretRotation.z)>.0001f
            ||std::abs(current.chaingunTurretRotation.w-factory.chaingunTurretRotation.w)>.0001f);
}
inline bool hasSavedVrSupport(const std::string&path,const std::string&weapon){
    std::ifstream input(path);std::string row;
    while(std::getline(input,row)){
        std::istringstream record(row);std::string version,key;
        if(record>>version>>key&&version=="2"&&key==weapon)return true;
    }
    return false;
}
// Write one per-weapon reset, retaining every unrelated line and an adjacent
// copy for recovery. The legacy heavy weapon GUN anchor is not a visual hand
// calibration and must never be deleted or rewritten by this operation.
inline bool resetVrWeaponCalibrationFile(const std::string&path,
    const std::string&weapon,bool rightHandFile,std::string*why=nullptr){
    if(!validVrCalibrationWeapon(weapon)){
        if(why)*why="unsupported weapon";return false;
    }
    namespace fs=std::filesystem;
    std::error_code ec;
    if(!fs::exists(path,ec))return true;
    if(ec||fs::file_size(path,ec)>(rightHandFile?16384u:65536u)){
        if(why)*why="calibration file too large or inaccessible";return false;
    }
    std::ifstream input(path);
    if(!input){if(why)*why="cannot open calibration file";return false;}
    std::vector<std::string> remaining;
    std::string line;bool removed=false;
    const std::string alternate=weapon=="gauss_cannon"?weapon+"_siege"
        :weapon=="chaingun"?weapon+"_turret":"";
    while(std::getline(input,line)){
        bool matches=false;
        if(rightHandFile){
            const auto eq=line.find('=');
            const auto first=line.find_first_not_of(" \t");
            if(eq!=std::string::npos&&first!=std::string::npos&&line[first]!='#'){
                auto key=line.substr(first,eq-first);
                const auto end=key.find_last_not_of(" \t");
                if(end!=std::string::npos)key.erase(end+1);
                matches=key==weapon+"_hand_position"||key==weapon+"_rotation"
                    ||key==weapon+"_left_hand_position"||key==weapon+"_left_rotation"
                    ||key==weapon+"_finetune_offset"
                    ||key==weapon+"_left_finetune_offset"
                    ||(weapon=="chaingun"&&(
                        key=="chaingun_rotor_hand_position"
                        ||key=="chaingun_rotor_rotation"
                        ||key=="chaingun_rotor_left_hand_position"
                        ||key=="chaingun_rotor_left_rotation"
                        ||key=="chaingun_rotor_finetune_offset"
                        ||key=="chaingun_rotor_left_finetune_offset"
                        ||key=="chaingun_rotor_support_position"
                        ||key=="chaingun_rotor_left_primary_support_position"))
                    ||(!alternate.empty()&&(
                        key==alternate+"_hand_position"||key==alternate+"_rotation"
                        ||key==alternate+"_left_hand_position"
                        ||key==alternate+"_left_rotation"
                        ||key==alternate+"_finetune_offset"
                        ||key==alternate+"_left_finetune_offset"
                        ||key==alternate+"_support_position"
                        ||key==alternate+"_left_primary_support_position"));
            }
        }else{
            std::istringstream record(line);std::string version,key;
            if((record>>version>>key)&&version=="2"&&key==weapon)matches=true;
        }
        if(matches)removed=true;
        else remaining.push_back(line);
    }
    if(input.bad()){if(why)*why="failed reading calibration";return false;}
    if(!removed)return true;
    fs::copy_file(path,path+".before-vr-reset.bak",
        fs::copy_options::overwrite_existing,ec);
    if(ec){if(why)*why="could not back up existing calibration";return false;}
    const fs::path tmp(path+".vr-reset.tmp"),dest(path);
    {std::ofstream output(tmp,std::ios::trunc);
     if(!output){if(why)*why="failed opening reset staging file";return false;}
     for(const auto&row:remaining)output<<row<<'\n';
     output.flush();if(!output){if(why)*why="failed saving reset staging file";return false;}}
#ifdef _WIN32
    if(!MoveFileExW(tmp.wstring().c_str(),dest.wstring().c_str(),
        MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){
        fs::remove(tmp,ec);if(why)*why="could not install reset config";return false;}
#else
    fs::rename(tmp,dest,ec);
    if(ec){fs::remove(tmp);if(why)*why="could not install reset config";return false;}
#endif
    return true;
}
// Restore only one selected weapon from the user's PROVIDED factory preset.
// Unlike the legacy reset (which removes overrides), this reproduces the
// exact calibrated left/right primary and support values, including Siege /
// Turret, without touching other weapons or shared Gauss/Chaingun anchors.
// Validate the factory and stage everything BEFORE modifying any live file.
inline bool restoreVrWeaponCalibrationDefault(const std::string& path,
    const std::string& factoryPath,const std::string& weapon,
    bool primaryFile,std::string*why=nullptr) {
    namespace fs=std::filesystem;
    auto fail=[&](const char* message){
        if(why)*why=message;
        return false;
    };
    if(!validVrCalibrationWeapon(weapon))
        return fail("unsupported reset weapon");
    const std::string alternate=weapon=="gauss_cannon"?"gauss_cannon_siege"
        :weapon=="chaingun"?"chaingun_turret":"";
    const auto belongs=[&](const std::string& line)->bool {
        if(primaryFile){
            const auto first=line.find_first_not_of(" \t");
            const auto equal=line.find('=');
            if(first==std::string::npos||equal==std::string::npos
               ||line[first]=='#'||first>=equal)return false;
            auto key=line.substr(first,equal-first);
            const auto end=key.find_last_not_of(" \t");
            if(end!=std::string::npos)key.resize(end+1);
            const auto matches=[&](const std::string& name){
                return key==name+"_hand_position"||key==name+"_rotation"
                    ||key==name+"_left_hand_position"
                    ||key==name+"_left_rotation"
                    ||key==name+"_finetune_offset"
                    ||key==name+"_left_finetune_offset";
            };
            if(matches(weapon))return true;
            if(weapon=="chaingun"&&(matches("chaingun_rotor")
                ||key=="chaingun_rotor_support_position"
                ||key=="chaingun_rotor_left_primary_support_position"))
                return true;
            return !alternate.empty()
                &&(matches(alternate)||key==alternate+"_support_position"
                   ||key==alternate+"_left_primary_support_position");
        }
        std::istringstream row(line);
        std::string version,key;
        return (row>>version>>key)&&version=="2"&&key==weapon;
    };
    std::error_code ec;
    if(!fs::exists(factoryPath,ec)||ec)return fail("packaged factory calibration missing");
    if(fs::file_size(factoryPath,ec)>(primaryFile?16384u:65536u)||ec)
        return fail("factory calibration exceeds size limit");
    std::ifstream factory(factoryPath);
    if(!factory)return fail("cannot read factory calibration");
    std::vector<std::string> restore;
    std::string line;
    while(std::getline(factory,line))
        if(belongs(line))restore.push_back(line);
    if(factory.bad()||restore.empty())
        return fail("factory has no saved pose for this weapon");
    const bool exists=fs::exists(path,ec);
    if(ec)return fail("cannot access existing calibration");
    std::vector<std::string> outputLines;
    if(exists){
        if(fs::file_size(path,ec)>(primaryFile?16384u:65536u)||ec)
            return fail("existing calibration exceeds size limit");
        std::ifstream input(path);
        if(!input)return fail("cannot read existing calibration");
        while(std::getline(input,line))
            if(!belongs(line))outputLines.push_back(line);
        if(input.bad())return fail("failed reading existing calibration");
    }
    outputLines.insert(outputLines.end(),restore.begin(),restore.end());
    const fs::path dest(path),staging(path+".vr-default.tmp");
    // Backup is always left adjacent in portable UserData for recovery.
    if(exists){
        fs::copy_file(dest,path+".before-vr-reset.bak",
            fs::copy_options::overwrite_existing,ec);
        if(ec)return fail("cannot back up current weapon calibration");
    }
    {std::ofstream output(staging,std::ios::trunc);
     if(!output)return fail("cannot create reset staging file");
     for(const auto& record:outputLines)output<<record<<'\n';
     output.flush();
     if(!output){fs::remove(staging,ec);return fail("cannot save reset staging file");}}
#ifdef _WIN32
    if(!MoveFileExW(staging.wstring().c_str(),dest.wstring().c_str(),
        MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){
        fs::remove(staging,ec);
        return fail("cannot install factory calibration");
    }
#else
    fs::rename(staging,dest,ec);
    if(ec){fs::remove(staging);return fail("cannot install factory calibration");}
#endif
    return true;
}

}
