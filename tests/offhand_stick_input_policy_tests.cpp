#include "../src/openxr/OffHandStickInputPolicy.h"
#include <cassert>
int main(){
    kharvox::OffHandStickInput input{};
    auto a=input.update(true,true,true,1000);
    assert(!a.rightPause&&!a.leftMelee);
    a=input.update(true,true,true,1160);assert(!a.rightPause&&!a.leftMelee);
    a=input.update(true,false,true,1170);assert(!a.rightPause&&!a.leftMelee);
    a=input.update(false,false,true,1180);assert(!a.rightPause&&!a.leftMelee);
    a=input.update(true,false,true,1200);assert(!a.rightPause);
    a=input.update(true,false,true,1310);assert(a.rightPause&&!a.leftMelee);
    a=input.update(false,false,true,1320);assert(!a.rightPause);
    a=input.update(true,false,true,1500);
    a=input.update(false,false,true,1530);assert(a.rightPause); // short tap
    a=input.update(false,false,true,1540);assert(!a.rightPause);
    a=input.update(false,true,true,1600);assert(!a.leftMelee);
    a=input.update(false,false,true,1610);assert(a.leftMelee);
    a=input.update(true,false,true,1700);
    a=input.update(false,false,false,1710);assert(!a.rightPause);
    return 0;
}
