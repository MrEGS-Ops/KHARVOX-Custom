#include "../src/openxr/ChainsawMotionSpeedPolicy.h"
#include <cmath>
#include <limits>
#define CHECK(x) do {if(!(x)) return __LINE__;} while(false)

int main(){
 using kharvox::ChainsawMotionSpeedState;
 using kharvox::updateChainsawMotionSpeed;
 ChainsawMotionSpeedState state{};
 CHECK(updateChainsawMotionSpeed(state,false,true,1,0,0,1000).targetHz==200);
 auto start=updateChainsawMotionSpeed(state,true,true,2,0,0,1000);
 CHECK(start.started&&start.targetHz==200); // start stationary without discontinuity
 auto idle=updateChainsawMotionSpeed(state,true,true,0,0,0,1020);
 CHECK(idle.targetHz==200);
 int fastHz=200;
 for(int i=0;i<20;i++)
     fastHz=updateChainsawMotionSpeed(state,true,true,2.3f,0,0,1040+i*20).targetHz;
 CHECK(fastHz>=30&&fastHz<50); // fast cutting hand is fast animation / low tick target
 int slowHz=fastHz;
 for(int i=0;i<35;i++)
     slowHz=updateChainsawMotionSpeed(state,true,true,0,0,0,1500+i*20).targetHz;
 CHECK(slowHz>fastHz&&slowHz>=195); // stops MID execution, returns to ~200Hz
 const auto missing=updateChainsawMotionSpeed(state,true,false,3,0,0,2250);
 CHECK(missing.targetHz==200);
 state.reset();
 CHECK(updateChainsawMotionSpeed(state,true,true,0,0,0,2300).targetHz==200);
 int sensitive=200,insensitive=200;
 ChainsawMotionSpeedState a{},b{};
 updateChainsawMotionSpeed(a,true,true,0,0,0,3000,90);
 updateChainsawMotionSpeed(b,true,true,0,0,0,3000,300);
 for(int i=1;i<=18;i++){
     sensitive=updateChainsawMotionSpeed(a,true,true,1.2f,0,0,3000+i*30,90).targetHz;
     insensitive=updateChainsawMotionSpeed(b,true,true,1.2f,0,0,3000+i*30,300).targetHz;
 }
 CHECK(sensitive<insensitive&&sensitive<=50);
 CHECK(insensitive>90);
 auto corrupt=updateChainsawMotionSpeed(a,true,true,
     std::numeric_limits<float>::quiet_NaN(),0,0,3600,90);
 CHECK(corrupt.targetHz>=30&&corrupt.targetHz<=200);
 CHECK(updateChainsawMotionSpeed(state,false,false,0,0,0,3700).targetHz==200);
 CHECK(!state.active&&state.appliedHz==200);
 return 0;
}
