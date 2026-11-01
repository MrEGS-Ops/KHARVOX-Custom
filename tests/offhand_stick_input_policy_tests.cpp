#include "../src/openxr/OffHandStickInputPolicy.h"
#include <cassert>
int main(){
    kharvox::OffHandStickInput input{};
    auto a=input.update(true,true,true,1000);
    assert(!a.rightMelee&&!a.leftPause);
    a=input.update(true,true,true,1160);assert(!a.rightMelee&&!a.leftPause);
    a=input.update(true,false,true,1170);assert(!a.rightMelee&&!a.leftPause);
    a=input.update(false,false,true,1180);assert(!a.rightMelee&&!a.leftPause);
    a=input.update(true,false,true,1200);assert(!a.rightMelee);
    a=input.update(true,false,true,1310);assert(a.rightMelee&&!a.leftPause);
    a=input.update(false,false,true,1320);assert(!a.rightMelee);
    a=input.update(true,false,true,1500);
    a=input.update(false,false,true,1530);assert(a.rightMelee); // short tap
    a=input.update(false,false,true,1540);assert(!a.rightMelee);
    a=input.update(false,true,true,1600);assert(!a.leftPause);
    a=input.update(false,false,true,1610);assert(a.leftPause);
    a=input.update(true,false,true,1700);
    a=input.update(false,false,false,1710);assert(!a.rightMelee);
    return 0;
}
