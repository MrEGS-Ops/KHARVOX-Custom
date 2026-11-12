#include "../src/openxr/PhysicalGlorykillMeleePolicy.h"
#include <cassert>
int main(){
    using namespace kharvox::physicalgk;
    assert(routeStickMelee(false,true,true));
    assert(!routeStickMelee(false,true,false));
    assert(!routeStickMelee(true,true,true));
    assert(!routeStickMelee(true,true,false));
    assert(routeStickMelee(true,false,true));
    // No RS->USE, RS->JOY8, or legacy melee queue when physical GK is on.
    assert(!routeNativeUse(true,true,true));
    // D-pad Right is equipment, not Use. When RS melee is disabled,
    // send independent keyboard Use on a confirmed isolated RS tap.
    assert(routeKeyboardWorldUse(true,true,true,true));  // both mods
    assert(routeKeyboardWorldUse(true,true,false,true)); // Hand Focus alone
    assert(routeKeyboardWorldUse(true,false,true,true)); // Physical GK alone
    assert(!routeKeyboardWorldUse(true,false,false,true));
    assert(!routeKeyboardWorldUse(true,true,true,false)); // no click
    assert(!routeKeyboardWorldUse(false,true,true,true)); // not gameplay
    assert(!routeNativeUse(true,false,false));
    assert(!routeNativeUse(false,true,false));
    assert(!routeNativeMelee(true,true,true,true,false,true));
    assert(!routeNativeMelee(true,true,true,true,false,false));
    assert(!routeNativeMelee(true,true,false,true,false,true));
    // Only physical controller-motion punches trigger native melee/GK.
    assert(routeNativeMelee(true,true,true,true,true,true));
    assert(routeNativeMelee(true,true,false,false,true,false));
    assert(!routeNativeMelee(true,false,true,true,true,true));
    assert(routePhysicalPunch(true,true,true));
    assert(!routePhysicalPunch(false,true,true));
    assert(!routePhysicalPunch(true,true,false));
    // Physical GK OFF: keep native original RS melee and USE behaviour.
    assert(routeNativeUse(false,true,true));
    assert(!routeNativeUse(false,false,true));
    assert(routeNativeMelee(false,true,true,false,false,true));
    assert(routeNativeMelee(false,true,false,true,false,true));
    assert(!routeNativeMelee(false,true,true,true,false,false));
    assert(!routeNativeMelee(false,false,true,true,true,true));
    return 0;
}
