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
    // GRIP-held weapon fine-tune: 1 cm PER intentional flick, no auto
    // repeat when held, no steps outside primary calibration or Grip hold.
    VrWeaponFineTuneEdges fine{};
    auto nudge=[&](bool allowed,bool grip,float lx,float ly,float ry){
        return vrWeaponFineTuneCentimeterStep(
            fine,allowed,grip,lx,ly,ry);
    };
    const auto zeroNudge=[&](RightHandGripOffset p){
        assert(p.x==0.f&&p.y==0.f&&p.z==0.f);
    };
    zeroNudge(nudge(true,true,0,0,0)); // Grip enters, arms neutral sticks
    auto move=nudge(true,true,1.f,0.f,0.f);
    assert(std::abs(move.x-.01f)<1e-6f&&move.y==0&&move.z==0);
    zeroNudge(nudge(true,true,1.f,0.f,0.f)); // held never repeats
    zeroNudge(nudge(true,true,0,0,0)); // re-arm only when centered
    move=nudge(true,true,-1.f,0.f,0.f);
    assert(std::abs(move.x+.01f)<1e-6f&&move.y==0&&move.z==0);
    zeroNudge(nudge(true,true,0,0,0));
    move=nudge(true,true,0.f,1.f,0.f); // left up = forward = -Z
    assert(move.x==0&&move.y==0&&std::abs(move.z+.01f)<1e-6f);
    zeroNudge(nudge(true,true,0,0,0));
    move=nudge(true,true,0.f,-1.f,0.f);
    assert(move.x==0&&move.y==0&&std::abs(move.z-.01f)<1e-6f);
    zeroNudge(nudge(true,true,0,0,0));
    move=nudge(true,true,0,0,1.f); // right up = +Y
    assert(move.x==0&&std::abs(move.y-.01f)<1e-6f&&move.z==0);
    zeroNudge(nudge(true,true,0,0,0));
    move=nudge(true,true,0,0,-1.f); // right down = -Y
    assert(move.x==0&&std::abs(move.y+.01f)<1e-6f&&move.z==0);
    zeroNudge(nudge(true,true,0,0,0));
    move=nudge(true,true,.9f,.8f,0); // diagonal -> ONE axis, not two
    assert(std::abs(move.x-.01f)<1e-6f&&move.y==0&&move.z==0);
    zeroNudge(nudge(true,true,0,0,0));
    zeroNudge(nudge(true,false,1.f,0,0)); // release disables fine-tune
    zeroNudge(nudge(true,true,1.f,0,0)); // don't step at grip entry
    zeroNudge(nudge(true,true,0,0,0));
    move=nudge(true,true,1.f,0,0);
    assert(std::abs(move.x-.01f)<1e-6f);
    zeroNudge(nudge(false,true,-1.f,0,0)); // support / menu can't move
    zeroNudge(nudge(true,true,-1.f,0,0)); // re-enter held deflection inert
    zeroNudge(nudge(true,true,0,0,0));
    move=nudge(true,true,0,0,-1.f);
    assert(std::abs(move.y+.01f)<1e-6f);
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
    assert(vrMenuGripGamepadTarget(nav,6,false)==33);
    assert(vrMenuGripGamepadTarget(nav,6,true)==31);
    nav.actionIndex=5;
    assert(vrMenuGripGamepadTarget(nav,6,true)==33);
    nav.actionIndex=6;
    assert(vrMenuGripGamepadTarget(nav,6,true)==95);
    nav.actionIndex=5;
    assert(vrMenuGripGamepadTarget(nav,6,false)==95);
    nav.settingsColumn=false;
    assert(vrMenuGripGamepadTarget(nav,8,true)==8);
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
