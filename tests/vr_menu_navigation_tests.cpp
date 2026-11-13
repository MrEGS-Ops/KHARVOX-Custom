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
