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
    return 0;
}
