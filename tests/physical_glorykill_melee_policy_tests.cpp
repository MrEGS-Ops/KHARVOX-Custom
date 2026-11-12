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
    // Regression: Hand Focus must still open doors/use stations when
    // Physical GK disables the original RS->USE+melee sequence.
    assert(routeNativeUse(true,true,false,true));
    assert(routeNativeUse(true,true,true,true));
    assert(routeNativeUse(false,true,false,true));
    assert(!routeNativeUse(true,false,false,true));
    assert(!routeNativeUse(false,true,false,false));
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
