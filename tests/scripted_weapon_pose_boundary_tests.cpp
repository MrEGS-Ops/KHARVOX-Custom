#include "../src/openxr/ScriptedWeaponPoseBoundary.h"
#include <cassert>
int main(){
    using B=kharvox::ScriptedWeaponPoseBoundary;
    B b{};
    assert(b.update(true,false)==B::Change::None);
    assert(b.update(true,true)==B::Change::None);
    assert(b.update(true,false)==B::Change::None);
    assert(b.update(true,true)==B::Change::None);
    assert(b.update(true,false)==B::Change::None);
    assert(b.update(true,false)==B::Change::None);
    assert(b.update(true,false)==B::Change::Suspend);
    assert(b.suspended);
    assert(b.update(true,false)==B::Change::None);
    assert(b.update(false,false)==B::Change::None);
    for(int i=0;i<4;++i)assert(b.update(true,true)==B::Change::None);
    assert(b.update(true,true)==B::Change::Restore);
    assert(!b.suspended);
    assert(b.update(true,true)==B::Change::None);
    // A radial weapon wheel disables some native combat inputs but must
    // not be interpreted as a scripted terminal or release the VR gun.
    for(int i=0;i<20;++i)
        assert(b.update(true,false,true)==B::Change::None);
    assert(!b.suspended);
    // If opened during a pending scripted classification, cancel that hold.
    assert(b.update(true,false)==B::Change::None);
    assert(b.update(true,false)==B::Change::None);
    assert(b.update(true,false)==B::Change::Suspend);
    assert(b.update(true,false,true)==B::Change::Restore);
    assert(!b.suspended);
    assert(b.update(true,true)==B::Change::None);
    return 0;
}
