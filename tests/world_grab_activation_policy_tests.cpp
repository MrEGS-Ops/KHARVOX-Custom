#include "../src/openxr/WorldGrabActivationTestPolicy.h"
#include <cassert>
using namespace kharvox;
int main(){
    // A: immediate acquisition without relying on a pre-existing kick.
    assert(worldGrabActivationTestValid(1));
    assert(worldGrabActivationFollowReady(1,false,0));
    assert(!worldGrabActivationNeedsWake(1));
    // B: exact native wake is requested, with the same hand follow.
    assert(worldGrabActivationNeedsWake(2));
    assert(worldGrabActivationFollowReady(2,false,0));
    // C: contact proxy is a small position write, not a collision callback.
    assert(worldGrabActivationContactProxy(3));
    assert(worldGrabActivationNeedsWake(3));
    assert(!worldGrabActivationMicroMove(3));
    // D: distinct micro vertical movement; no contact proxy.
    assert(worldGrabActivationMicroMove(4));
    assert(worldGrabActivationNeedsWake(4));
    assert(!worldGrabActivationContactProxy(4));
    // E: observe native movement for 350 ms before manual-follow fallback.
    assert(worldGrabActivationOwnerTickProbe(5));
    assert(!worldGrabActivationFollowReady(5,false,0));
    assert(!worldGrabActivationFollowReady(5,false,349));
    assert(worldGrabActivationFollowReady(5,true,20));
    assert(worldGrabActivationFollowReady(5,false,350));
    // Retired methods cannot be selected from the controller or native bridge.
    for(int mode:{-1,0,6,7,8,100})
        assert(!worldGrabActivationTestValid(mode));
    return 0;
}
