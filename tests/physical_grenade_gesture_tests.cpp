#include "../src/openxr/PhysicalGrenadeGesturePolicy.h"
#include <cstdlib>
int main() {
    using namespace kharvox;
    PhysicalGrenadeGestureState s{};
    auto step=[&](bool down,bool tracked,bool chest,bool play=true){
        return updateChestGrenadeGesture(s,play,down,tracked,chest);
    };
    if(step(true,true,true)!=ChestGrenadeAction::Cycle) return 1;
    if(step(true,true,true)!=ChestGrenadeAction::None) return 2;
    if(step(false,true,true)!=ChestGrenadeAction::None) return 3; // cancel inside
    if(step(true,true,true)!=ChestGrenadeAction::Cycle) return 4;
    if(step(true,true,false)!=ChestGrenadeAction::None) return 5; // pull pin
    if(step(false,true,false)!=ChestGrenadeAction::Throw) return 6;
    if(step(false,true,false)!=ChestGrenadeAction::None) return 7;
    if(step(true,true,false)!=ChestGrenadeAction::None) return 8; // grab outside
    if(step(false,true,false)!=ChestGrenadeAction::None) return 9;
    if(step(true,true,true)!=ChestGrenadeAction::Cycle) return 10;
    if(step(true,false,false)!=ChestGrenadeAction::None) return 11; // tracking loss
    if(step(false,true,false)!=ChestGrenadeAction::None) return 12;
    if(step(true,true,true)!=ChestGrenadeAction::Cycle) return 13;
    if(step(true,true,false)!=ChestGrenadeAction::None) return 14;
    if(step(true,true,false,false)!=ChestGrenadeAction::None) return 15; // menu cancels
    if(step(false,true,false)!=ChestGrenadeAction::None) return 16;
    return 0;
}
