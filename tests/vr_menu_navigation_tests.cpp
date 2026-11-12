#include "../src/openxr/VrHandMenuPolicy.h"
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
    assert(vrMenuGripGamepadTarget(nav,6,false)==95);
    assert(vrMenuGripGamepadTarget(nav,6,true)==31);
    nav.actionIndex=5;
    assert(vrMenuGripGamepadTarget(nav,6,true)==95);
    nav.settingsColumn=false;
    assert(vrMenuGripGamepadTarget(nav,8,true)==8);
    return 0;
}
