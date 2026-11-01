#include "../src/openxr/OffHandStickInputPolicy.h"
#include <cassert>
int main(){
    kharvox::OffHandStickInput input{};
    auto a=input.update(true,true,true,1000);
    assert(!a.rightMelee&&!a.leftPause);
    a=input.update(true,true,true,6000);
    assert(!a.rightMelee&&!a.leftPause);
    a=input.update(true,false,true,6100);
    assert(!a.rightMelee&&!a.leftPause);
    a=input.update(false,false,true,6200);
    assert(!a.rightMelee&&!a.leftPause);
    // Right click held ALONE for any duration must not animate melee on press.
    // Even if left is pressed much later, both actions remain fully blocked.
    a=input.update(true,false,true,7000);
    assert(!a.rightMelee&&!a.leftPause);
    a=input.update(true,false,true,11000);
    assert(!a.rightMelee&&!a.leftPause);
    a=input.update(true,true,true,11001);
    assert(!a.rightMelee&&!a.leftPause);
    a=input.update(false,true,true,11002);
    assert(!a.rightMelee&&!a.leftPause);
    a=input.update(false,false,true,11003);
    assert(!a.rightMelee&&!a.leftPause);
    // Same protection when LEFT pause is pressed first.
    a=input.update(false,true,true,12000);
    assert(!a.rightMelee&&!a.leftPause);
    a=input.update(false,true,true,15000);
    assert(!a.rightMelee&&!a.leftPause);
    a=input.update(true,true,true,15001);
    assert(!a.rightMelee&&!a.leftPause);
    a=input.update(false,false,true,15002);
    assert(!a.rightMelee&&!a.leftPause);
    // Single right-click: exactly ONE melee pulse on release, never on hold.
    a=input.update(true,false,true,16000);
    assert(!a.rightMelee&&!a.leftPause);
    a=input.update(true,false,true,19000);
    assert(!a.rightMelee&&!a.leftPause);
    a=input.update(false,false,true,19001);
    assert(a.rightMelee&&!a.leftPause);
    a=input.update(false,false,true,19002);
    assert(!a.rightMelee&&!a.leftPause);
    // Short single tap is not lost, and never doubles.
    a=input.update(true,false,true,20000);
    assert(!a.rightMelee);
    a=input.update(false,false,true,20015);
    assert(a.rightMelee&&!a.leftPause);
    a=input.update(false,false,true,20016);
    assert(!a.rightMelee);
    // Single left-click pauses once ON RELEASE.
    a=input.update(false,true,true,21000);
    assert(!a.leftPause&&!a.rightMelee);
    a=input.update(false,true,true,26000);
    assert(!a.leftPause&&!a.rightMelee);
    a=input.update(false,false,true,26001);
    assert(a.leftPause&&!a.rightMelee);
    a=input.update(false,false,true,26002);
    assert(!a.leftPause&&!a.rightMelee);
    // Disabling capture midway through a hold does not emit deferred actions.
    a=input.update(true,false,true,27000);
    a=input.update(false,false,false,27001);
    assert(!a.rightMelee&&!a.leftPause);
    a=input.update(false,false,true,27002);
    assert(!a.rightMelee&&!a.leftPause);
    return 0;
}
