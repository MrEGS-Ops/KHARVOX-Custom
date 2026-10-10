#include "../src/openxr/PhysicalGrenadeGesturePolicy.h"
#include <cstdlib>
int main() {
    using namespace kharvox;
    PhysicalGrenadeGestureState s{};
    auto tick=[&](bool held,bool tracked,float speed,std::int64_t ns) {
        return updatePhysicalGrenadeGesture(s,true,held,tracked,speed,ns);
    };
    if(tick(true,true,0.f,0)) return 1;
    if(tick(true,true,1.5f,100000000LL)) return 2;
    if(tick(true,true,0.3f,200000000LL)!=true) return 3;
    if(tick(true,true,1.8f,300000000LL)) return 4;
    if(tick(true,true,0.2f,450000000LL)) return 5;
    tick(false,true,0.f,500000000LL);
    tick(true,false,0.f,600000000LL);
    tick(true,true,1.8f,650000000LL);
    if(tick(true,true,0.2f,750000000LL)) return 6;
    tick(false,true,0.f,800000000LL);
    tick(true,true,0.f,900000000LL);
    tick(true,true,0.8f,920000000LL);
    if(tick(true,true,0.2f,1000000000LL)) return 7;
    tick(true,true,1.2f,1100000000LL);
    if(tick(true,true,0.2f,1800000000LL)) return 8;
    tick(false,true,0.f,1900000000LL);
    tick(true,true,0.f,2000000000LL);
    tick(true,true,1.2f,2050000000LL);
    if(!tick(true,true,0.2f,2150000000LL)) return 9;
    return 0;
}
