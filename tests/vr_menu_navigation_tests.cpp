#include "../src/openxr/VrHandMenuPolicy.h"
#include "../src/openxr/ManualCrouchPolicy.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
int main(){
    using namespace kharvox;
    VrMenuNavigation nav{};
    int selected=0;
    // LEFT stick moves through rows only on a deliberate edge.
    vrMenuNavigateStick(nav,0.f,-.90f,5,selected,2);
    assert(selected==1&&nav.usingStick&&!nav.settingsColumn);
    vrMenuNavigateStick(nav,0.f,-.90f,5,selected,2);
    assert(selected==1); // no runaway scrolling from a held stick
    vrMenuNavigateStick(nav,0.f,0.f,5,selected,2);
    vrMenuNavigateStick(nav,0.f,-.90f,5,selected,2);
    assert(selected==2);
    // RIGHT and LEFT move between mod list and settings column.
    vrMenuNavigateStick(nav,0.f,0.f,5,selected,2);
    vrMenuNavigateStick(nav,.90f,0.f,5,selected,2);
    assert(nav.settingsColumn&&nav.actionIndex==0);
    assert(vrMenuModsGamepadTarget(nav,selected,5,2)==110);
    vrMenuNavigateStick(nav,0.f,-.90f,5,selected,2);
    assert(nav.actionIndex==1);
    assert(vrMenuModsGamepadTarget(nav,selected,5,2)==130);
    vrMenuNavigateStick(nav,0.f,0.f,5,selected,2);
    vrMenuNavigateStick(nav,-.90f,0.f,5,selected,2);
    assert(!nav.settingsColumn);
    assert(vrMenuModsGamepadTarget(nav,selected,5,2)==1002);
    // GRIP RELEASED -> live free controller, every frame.
    // GRIP HELD -> freeze the valid pose for 1cm fine tune only. If
    // no preview yet exists, a first held frame can capture the pose.
    assert(vrWeaponManualCalibrationLive(
        VrHandMenuMode::Calibrating,false,true));
    assert(vrWeaponManualCalibrationLive(
        VrHandMenuMode::Calibrating,false,false));
    assert(!vrWeaponManualCalibrationLive(
        VrHandMenuMode::Calibrating,true,true));
    assert(vrWeaponManualCalibrationLive(
        VrHandMenuMode::Calibrating,true,false));
    assert(!vrWeaponManualCalibrationLive(
        VrHandMenuMode::Menu,true,true));
    assert(!vrWeaponManualCalibrationLive(
        VrHandMenuMode::Closed,false,false));
    // A single unsaved calibration can switch between GUN fine-tune
    // (Grip held) and GLOVE rotation (Grip released) repeatedly. Each
    // release starts from the current visual wrist angle WITHOUT a jump.
    const RightHandGripRotation id{0,0,0,1,true};
    const RightHandGripRotation yaw90{0,0.707106781f,0,0.707106781f,true};
    const RightHandGripRotation yaw180{0,1,0,0,true};
    const auto rotationClose=[](RightHandGripRotation a,
        RightHandGripRotation b){
        const float dot=a.x*b.x+a.y*b.y+a.z*b.z+a.w*b.w;
        return std::abs(std::abs(dot)-1.f)<.0002f;
    };
    VrPrimaryRotationEdit edit{};
    RightHandGripRotation wrist=id;
    RightHandGripOffset gunDraft{.02f,-.03f,-.04f};
    const auto originalDraft=gunDraft;
    // Enter rotation mode from a held Grip: NO jump to free controller.
    wrist=vrCalibrationRelativeRotation(edit,false,yaw90,wrist);
    assert(rotationClose(wrist,id)&&edit.referenceValid);
    // Rotate the free controller another 90 deg: glove rotates, gun holds.
    wrist=vrCalibrationRelativeRotation(edit,false,yaw180,wrist);
    assert(rotationClose(wrist,yaw90));
    assert(gunDraft.x==originalDraft.x
        &&gunDraft.y==originalDraft.y&&gunDraft.z==originalDraft.z);
    // Hold Grip: rotation freezes while gun is moved by one centimeter.
    wrist=vrCalibrationRelativeRotation(edit,true,id,wrist);
    assert(rotationClose(wrist,yaw90)&&!edit.referenceValid);
    gunDraft.y+=.01f;
    // Release Grip with the free controller at an unrelated angle:
    // the GUN draft and glove rotation stay exactly where they were.
    wrist=vrCalibrationRelativeRotation(edit,false,id,wrist);
    assert(rotationClose(wrist,yaw90));
    assert(std::abs(gunDraft.y+.02f)<.000001f);
    // Free-hand motion adjusts only the glove's rotation from this point.
    wrist=vrCalibrationRelativeRotation(edit,false,yaw90,wrist);
    assert(rotationClose(wrist,yaw180));
    assert(std::abs(gunDraft.y+.02f)<.000001f);
    // Re-grip: freeze yaw180 and preserve all positional adjustments.
    wrist=vrCalibrationRelativeRotation(edit,true,id,wrist);
    assert(rotationClose(wrist,yaw180)&&!edit.referenceValid);
    gunDraft.x-=.01f;
    wrist=vrCalibrationRelativeRotation(edit,false,yaw180,wrist);
    assert(rotationClose(wrist,yaw180));
    assert(std::abs(gunDraft.x-.01f)<.000001f);
    assert(std::abs(gunDraft.y+.02f)<.000001f);
    assert(std::abs(gunDraft.z+.04f)<.000001f);
    // Cancel discards unsaved changes by leaving the draft unwritten.
    const auto storedGun=originalDraft;
    const auto storedWrist=id;
    edit={};
    assert(!edit.referenceValid&&rotationClose(storedWrist,id));
    assert(std::abs(storedGun.x-.02f)<.000001f);

    // If the Quest runtime provides a valid but non-TRACKED GRIP pose,
    // use a tracked AIM laser pose. If neither is tracked, POSITION_VALID
    // still suffices; don't leave the glove stuck at the old saved pose.
    struct FakeTrackedPose {bool valid,positionTracked;int id;};
    const FakeTrackedPose trackedGrip{true,true,1};
    const FakeTrackedPose trackedAim{true,true,2};
    const FakeTrackedPose validPredictedGrip{true,false,3};
    const FakeTrackedPose validPredictedAim{true,false,4};
    const FakeTrackedPose unavailable{false,false,5};
    assert(&vrCalibrationFreeControllerPose(trackedGrip,trackedAim)==&trackedGrip);
    assert(&vrCalibrationFreeControllerPose(validPredictedGrip,trackedAim)==&trackedAim);
    assert(&vrCalibrationFreeControllerPose(validPredictedGrip,validPredictedAim)
        ==&validPredictedGrip);
    assert(&vrCalibrationFreeControllerPose(unavailable,validPredictedAim)
        ==&validPredictedAim);
    assert(&vrCalibrationFreeControllerPose(unavailable,trackedAim)==&trackedAim);
    // SUPPORT CALIBRATION is NOT the primary glove's free-controller
    // rotation probe. It must use the actual support-hand GRIP position,
    // even if an AIM laser is tracked farther forward.
    const auto& supportTracked=kharvox::vrSupportCalibrationPhysicalGrip(
        trackedGrip,trackedAim);
    assert(&supportTracked==&trackedGrip);
    const auto& supportPredicted=kharvox::vrSupportCalibrationPhysicalGrip(
        validPredictedGrip,trackedAim);
    assert(&supportPredicted==&validPredictedGrip);
    // No AIM fallback: do not save wrong support position if GRIP is lost.
    const auto& supportUnavailable=kharvox::vrSupportCalibrationPhysicalGrip(
        unavailable,trackedAim);
    assert(&supportUnavailable==&unavailable);
    assert(!supportUnavailable.valid);
    // Quest can report a valid GRIP rotation with an unchanged/predicted
    // position while its tracked AIM laser translates freely.
    struct FakeController {
        bool valid,positionTracked;
        RightHandGripOffset position;
        RightHandGripRotation orientation;
    };
    const FakeController staticGrip{
        true,true,{0.f,0.f,0.f},{0.f,.4f,0.f,.9165f,true}};
    const FakeController movingAim{
        true,true,{.12f,-.20f,-.32f},{0.f,0.f,0.f,1.f,true}};
    const auto live=vrCalibrationLivePose(staticGrip,movingAim);
    assert(std::abs(live.position.x-.12f)<.00001f);
    assert(std::abs(live.position.y+.20f)<.00001f);
    assert(std::abs(live.position.z+.32f)<.00001f);
    assert(std::abs(live.orientation.y-.4f)<.00001f);
    // Primary Grip: quick flick is a single 1cm step. Holding the same
    // direction then auto-repeats in 1cm increments after 340ms, every
    // 175ms. The speed is clock-driven, not headset frame-rate-driven.
    VrWeaponFineTuneEdges fine{};
    std::uint64_t fineClock=1000;
    auto nudge=[&](bool allowed,bool grip,float lx,float ly,float ry,
                   std::uint64_t ms=16){
        fineClock+=ms;
        return vrWeaponFineTuneCentimeterStep(
            fine,allowed,grip,lx,ly,ry,fineClock);
    };
    const auto zeroNudge=[&](RightHandGripOffset p){
        assert(p.x==0.f&&p.y==0.f&&p.z==0.f);
    };
    const auto sameCm=[](float got,float expected){
        return std::abs(got-expected)<1e-6f;
    };
    zeroNudge(nudge(true,true,0,0,0)); // Grip enters, arm neutral
    auto move=nudge(true,true,1.f,0.f,0.f);
    assert(sameCm(move.x,.01f)&&move.y==0&&move.z==0);
    const auto firstAt=fineClock;
    zeroNudge(nudge(true,true,1.f,0.f,0.f,
        kVrWeaponFineTuneRepeatDelayMs-1)); // not even 1ms early
    assert(fineClock==firstAt+kVrWeaponFineTuneRepeatDelayMs-1);
    move=nudge(true,true,1.f,0.f,0.f,1);
    assert(sameCm(move.x,.01f)); // first held auto-repeat
    zeroNudge(nudge(true,true,1.f,0.f,0.f,
        kVrWeaponFineTuneRepeatIntervalMs-1));
    move=nudge(true,true,1.f,0.f,0.f,1);
    assert(sameCm(move.x,.01f)); // steady 175ms repeat
    // A frame stall must emit only ONE step, never a 10cm catch-up jump.
    move=nudge(true,true,1.f,0.f,0.f,1000);
    assert(sameCm(move.x,.01f)&&move.y==0&&move.z==0);
    // Recenter resets the countdown and permits a new instant 1cm flick.
    zeroNudge(nudge(true,true,0,0,0));
    move=nudge(true,true,-1.f,0.f,0.f);
    assert(sameCm(move.x,-.01f)&&move.y==0&&move.z==0);
    zeroNudge(nudge(true,true,0,0,0));
    move=nudge(true,true,0.f,1.f,0.f); // left up -> local -Z
    assert(move.x==0&&move.y==0&&sameCm(move.z,-.01f));
    zeroNudge(nudge(true,true,0,0,0));
    move=nudge(true,true,0.f,-1.f,0.f); // left down -> +Z
    assert(move.x==0&&move.y==0&&sameCm(move.z,.01f));
    zeroNudge(nudge(true,true,0,0,0));
    move=nudge(true,true,0,0,1.f); // right up -> +Y
    assert(move.x==0&&sameCm(move.y,.01f)&&move.z==0);
    const auto rightAt=fineClock;
    zeroNudge(nudge(true,true,0,0,1.f,
        kVrWeaponFineTuneRepeatDelayMs-1));
    move=nudge(true,true,0,0,1.f,1);
    assert(sameCm(move.y,.01f));
    assert(fineClock==rightAt+kVrWeaponFineTuneRepeatDelayMs);
    zeroNudge(nudge(true,true,0,0,0));
    move=nudge(true,true,0,0,-1.f); // right down -> -Y
    assert(move.x==0&&sameCm(move.y,-.01f)&&move.z==0);
    zeroNudge(nudge(true,true,0,0,0));
    move=nudge(true,true,.9f,.8f,0); // diagonal: one axis
    assert(sameCm(move.x,.01f)&&move.y==0&&move.z==0);
    // No automatic direction reversal (X, Z or sign) while stick
    // remains displaced: users must recenter between opposing nudges.
    zeroNudge(nudge(true,true,-1.f,0,0,2000));
    zeroNudge(nudge(true,true,0,-1.f,0,2000));
    zeroNudge(nudge(true,true,0,0,0));
    move=nudge(true,true,0,-1.f,0);
    assert(sameCm(move.z,.01f));

    // Grip RELEASE switches to rotation mode and cancels repeat timers,
    // but runtime retains the unsaved GUN offset in fineTuneAccumulated.
    zeroNudge(nudge(true,false,0,0,-1.f,2000));
    zeroNudge(nudge(true,true,1.f,0,0)); // held at entry: inert
    zeroNudge(nudge(true,true,1.f,0,0,2000)); // stays inert until centered
    zeroNudge(nudge(true,true,0,0,0));
    move=nudge(true,true,1.f,0,0);
    assert(sameCm(move.x,.01f));
    zeroNudge(nudge(false,true,-1.f,0,0,2000)); // menu/support: no motion
    zeroNudge(nudge(true,true,-1.f,0,0)); // no inherited repeat timer
    zeroNudge(nudge(true,true,0,0,0));
    move=nudge(true,true,0,0,-1.f);
    assert(sameCm(move.y,-.01f));

    // Two sticks retain independent repeat clocks; left X and right Y can
    // both step once in the same frame without rotating the weapon.
    VrWeaponFineTuneEdges independent{};
    const auto twoSticks=[&](std::uint64_t at,float lx,float ly,float ry){
        return vrWeaponFineTuneCentimeterStep(
            independent,true,true,lx,ly,ry,at);
    };
    zeroNudge(twoSticks(100,0,0,0));
    move=twoSticks(110,1.f,0,1.f);
    assert(sameCm(move.x,.01f)&&sameCm(move.y,.01f)&&move.z==0);
    move=twoSticks(110+kVrWeaponFineTuneRepeatDelayMs,1.f,0,1.f);
    assert(sameCm(move.x,.01f)&&sameCm(move.y,.01f)&&move.z==0);

    // A selects exactly once per press, B same, no latent double events.
    bool a=false,b=false;
    assert(vrMenuButtonEdge(true,a));
    assert(!vrMenuButtonEdge(true,a));
    assert(!vrMenuButtonEdge(false,a));
    assert(vrMenuButtonEdge(true,a));
    assert(vrMenuButtonEdge(true,b));
    assert(!vrMenuButtonEdge(true,b));
    // Weapon calibration layout maps to the existing pointer actions.
    nav.settingsColumn=true;nav.actionIndex=0;
    assert(vrMenuGripGamepadTarget(nav,3,false)==20);
    nav.actionIndex=1;
    assert(vrMenuGripGamepadTarget(nav,3,false)==21);
    nav.actionIndex=2;
    assert(vrMenuGripGamepadTarget(nav,3,false)==30);
    nav.actionIndex=3;
    assert(vrMenuGripGamepadTarget(nav,3,true)==30);
    assert(vrMenuGripGamepadTarget(nav,6,true)==30);
    nav.actionIndex=2;
    assert(vrMenuGripGamepadTarget(nav,6,true)==32);
    nav.actionIndex=4;
    assert(vrMenuGripGamepadTarget(nav,6,false)==95);
    assert(vrMenuGripGamepadTarget(nav,6,true)==31);
    nav.actionIndex=5;
    assert(vrMenuGripGamepadTarget(nav,6,true)==95);
    assert(vrMenuGripGamepadTarget(nav,6,false)==-1);
    nav.actionIndex=6;
    assert(vrMenuGripGamepadTarget(nav,6,true)==-1);
    // Copy RIGHT -> LEFT has been removed: all left-side presets are
    // calibrated independently. The old button code 33 is unreachable.
    for(int i=0;i<7;++i){
        nav.actionIndex=i;
        assert(vrMenuGripGamepadTarget(nav,6,true)!=33);
        assert(vrMenuGripGamepadTarget(nav,6,false)!=33);
    }
    nav.settingsColumn=false;
    assert(vrMenuGripGamepadTarget(nav,8,true)==8);
    // Mods page starts in overview mode. Scrolling through the six
    // configurable entries must NOT enter settings or activate a toggle.
    VrMenuNavigation browse{};
    browse.settingsColumn=false;
    int browsingRow=0;
    vrMenuNavigateStick(browse,0.f,-.9f,6,browsingRow,5);
    assert(browsingRow==1&&!browse.settingsColumn);
    assert(vrMenuModsGamepadTarget(browse,browsingRow,6,5,false)==1001);
    vrMenuNavigateStick(browse,0.f,0.f,6,browsingRow,5); // re-arm
    vrMenuNavigateStick(browse,0.f,-.9f,6,browsingRow,5);
    assert(browsingRow==2&&!browse.settingsColumn);
    vrMenuNavigateStick(browse,0.f,0.f,6,browsingRow,5);
    for(int i=0;i<4;++i){
        vrMenuNavigateStick(browse,0.f,-.9f,6,browsingRow,5);
        vrMenuNavigateStick(browse,0.f,0.f,6,browsingRow,5);
    }
    assert(browsingRow==0&&!browse.settingsColumn);
    // Only explicit A / CONFIGURE moves focus into live options.
    browse.settingsColumn=true;
    browse.actionIndex=0;
    assert(vrMenuModsGamepadTarget(browse,0,6,5,false)==130);
    browse.settingsColumn=false; // B: back to overview
    assert(vrMenuModsGamepadTarget(browse,0,6,5,false)==1000);

    // Core mods have settings but no enable checkbox. First action is
    // OPEN CALIBRATION / SET HIP / SLOWER etc, not toggle.
    nav.settingsColumn=true;nav.actionIndex=0;
    assert(vrMenuModsGamepadTarget(nav,0,7,5,false)==130);
    nav.actionIndex=1;
    assert(vrMenuModsGamepadTarget(nav,0,7,5,false)==131);
    // Only the Physical Crouch and PAINSAW rows preserve toggle first.
    nav.actionIndex=0;
    assert(vrMenuModsGamepadTarget(nav,2,7,4,true)==110);
    assert(vrMenuModsGamepadTarget(nav,5,7,1,true)==110);
    nav.actionIndex=1;
    assert(vrMenuModsGamepadTarget(nav,2,7,4,true)==130);
    // Legacy wheel remap is retired, but Gunslinger's gesture wheel frees
    // right-stick DOWN for crouch when Physical Crouch is OFF.
    assert(manualStickCrouchAllowed(false,true,false));
    assert(manualStickCrouchAllowed(false,false,true));
    assert(!manualStickCrouchAllowed(true,true,true));
    assert(!manualStickCrouchAllowed(false,false,false));
    assert(manualStickCrouchDirection(0.f,-1.f));
    assert(manualStickCrouchDirection(.24f,-.95f));
    assert(!manualStickCrouchDirection(.7f,-1.f)); // turning diagonally
    assert(!manualStickCrouchDirection(0.f,-.5f)); // no accidental crouch
    assert(!manualStickCrouchDirection(0.f,.9f)); // chainsaw UP unchanged
    // No Weapon Wheel cannot resurrect the retired stick-down wheel route.
    assert(!vrScopedWeaponSelectDown(true,true,true,false,true));
    assert(!vrScopedWeaponSelectDown(true,false,true,false,true));
    assert(vrScopedWeaponSelectDown(true,false,true,true,true));
    assert(vrScopedWeaponSelectDown(true,false,false,false,true));
    assert(!vrScopedWeaponSelectDown(false,false,true,true,true));
    // Native 90 ms edge pulse, both on crouch and on stand; never leak a
    // manual pulse into Physical Crouch, menus or the old remap input path.
    assert(manualStickCrouchPulseActive(true,false,false,100,100+90000000));
    assert(!manualStickCrouchPulseActive(true,false,false,100+90000000,100+90000000));
    assert(!manualStickCrouchPulseActive(false,false,false,100,100+90000000));
    assert(!manualStickCrouchPulseActive(true,true,false,100,100+90000000));
    assert(!manualStickCrouchPulseActive(true,false,true,100,100+90000000));
    return 0;
}
