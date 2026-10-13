#include "../src/openxr/PhysicalGrenadeGesturePolicy.h"
int main(){
    using namespace kharvox;
    PhysicalGrenadeGestureState s{};
    auto tick=[&](bool grip,bool tracked,bool chest,long long ns,bool gameplay=true){
        return updateChestGrenadeGesture(s,gameplay,grip,tracked,chest,ns);
    };
    if(tick(true,true,true,100000000)!=ChestGrenadeAction::None)return 1;
    if(tick(false,true,true,230000000)!=ChestGrenadeAction::Cycle)return 2;
    if(tick(true,true,true,300000000)!=ChestGrenadeAction::None)return 3;
    if(tick(true,true,false,700000000)!=ChestGrenadeAction::None)return 4;
    if(tick(false,true,false,950000000)!=ChestGrenadeAction::Throw)return 5;
    if(tick(false,true,false,980000000)!=ChestGrenadeAction::None)return 6;
    if(tick(true,true,true,1000000000)!=ChestGrenadeAction::None)return 7;
    if(tick(false,true,true,1400000000)!=ChestGrenadeAction::None)return 8; // long hold inside
    if(tick(true,true,false,1500000000)!=ChestGrenadeAction::None)return 9;
    if(tick(false,true,false,1550000000)!=ChestGrenadeAction::None)return 10; // no arm outside
    if(tick(true,true,true,2000000000)!=ChestGrenadeAction::None)return 11;
    if(tick(true,false,false,2050000000)!=ChestGrenadeAction::None)return 12; // tracker lost
    if(tick(false,true,false,2150000000)!=ChestGrenadeAction::None)return 13;
    if(tick(true,true,true,2200000000)!=ChestGrenadeAction::None)return 14;
    if(tick(true,true,false,2300000000)!=ChestGrenadeAction::None)return 15;
    if(tick(true,true,false,2350000000,false)!=ChestGrenadeAction::None)return 16;
    if(tick(false,true,false,2500000000)!=ChestGrenadeAction::None)return 17;
    if(!insideChestGrenadeZone(0.f,-.43f,.02f))return 18;
    if(insideChestGrenadeZone(.12f,-.43f,.02f))return 19; // too far to side
    if(insideChestGrenadeZone(0.f,-.25f,.02f))return 20; // face, not chest
    if(insideChestGrenadeZone(0.f,-.68f,.02f))return 21; // abdomen
    if(insideChestGrenadeZone(0.f,-.43f,-.28f))return 22; // too far from chest
    return 0;
}
