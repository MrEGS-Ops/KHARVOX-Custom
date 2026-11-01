#include "../src/openxr/PhysicalGlorykillMeleePolicy.h"
#include <cassert>
int main(){
    using namespace kharvox::physicalgk;
    assert(routeStickMelee(false,true,true));
    assert(!routeStickMelee(false,true,false));
    assert(!routeStickMelee(true,true,true));
    assert(!routeStickMelee(true,true,false));
    assert(routeStickMelee(true,false,true));
    assert(routePhysicalPunch(true,true,true));
    assert(!routePhysicalPunch(false,true,true));
    assert(!routePhysicalPunch(true,true,false));
    return 0;
}
