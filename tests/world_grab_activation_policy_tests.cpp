#include "../src/openxr/WorldGrabActivationTestPolicy.h"
#include "../src/openxr/WorldGrabSyncPolicy.h"
#include <cassert>
using namespace kharvox;
int main(){
    assert(worldGrabStrongAnchor(6)&&worldGrabAnchorMode(6));
    for(int mode:{-1,0,1,2,3,4,5,7,8}){
        assert(!worldGrabAnchorMode(mode));
        assert(!worldGrabAnchorFollowReady(mode,true,1000));
    }
    assert(worldGrabStageWindowMs(1)==200);
    assert(worldGrabStageWindowMs(2)==500);
    assert(worldGrabStageWindowMs(3)==500);
    assert(worldGrabStageWindowMs(4)==350);
    assert(worldGrabStageWindowMs(5)==12000);
    assert(worldGrabStageWindowMs(6)==0);
    assert(worldGrabStageNativeEvidence(5,100,110,true,0,120));
    assert(!worldGrabStageNativeEvidence(3,100,105,true,101,120));
    assert(!worldGrabStageNativeEvidence(3,100,105,true,101,451));
    assert(!worldGrabStageNativeEvidence(3,100,99,true,0,120));
    assert(worldGrabStageNativeEvidence(3,100,500,true,101,500));
    assert(worldGrabAnchorFollowReady(6,true,0));
    assert(!worldGrabAnchorFollowReady(6,false,12000));
    assert(worldGrabNativeWakeCandidate(true,1,true,-1,false,false));
    assert(!worldGrabNativeWakeCandidate(true,1,true,-1,true,false));
    assert(worldGrabPassiveMotionCandidate(1.0f,false,false));
    assert(!worldGrabPassiveMotionCandidate(1.0f,false,true));
    return 0;
}
