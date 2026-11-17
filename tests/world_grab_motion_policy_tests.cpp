#include "../src/openxr/WorldGrabMotionPolicy.h"
#include <cassert>
#include <cmath>
using namespace kharvox;
int main(){
    WorldGrabController controller{};
    WorldGrabInput in{};
    in.enabled=in.gameplay=in.controllerTracked=in.freeHand=true;
    in.verifiedBarrel=true;in.grip=true;in.levelGeneration=5;in.entityId=42;
    in.targetDistanceMeters=.6f;in.deltaSeconds=1.f/90.f;
    in.handForward={1.f,0.f,0.f};
    in.handPosition={0.f,0.f,0.f};in.objectPosition={.6f,0.f,0.f};
    // Grip already down on enable: initialization must not auto-grab.
    auto& left=controller.hands[0];
    assert(controller.update(0,in).command==WorldGrabCommand::None);
    in.grip=false;controller.update(0,in);
    in.grip=true;
    auto acquired=controller.update(0,in);
    assert(acquired.command==WorldGrabCommand::Acquire&&acquired.entityId==42);
    assert(left.entityId==42);
    auto follow=controller.update(0,in);
    assert(follow.command==WorldGrabCommand::Follow);
    assert(follow.velocity.x<0.f);
    assert(grabLength(follow.velocity)<=8.001f);
    // Same object cannot be owned by two hands
    WorldGrabInput right=in;
    assert(controller.update(1,right).command==WorldGrabCommand::None);
    // Other objects can be held independently
    right.entityId=73;
    right.grip=false;controller.update(1,right);
    right.grip=true;
    assert(controller.update(1,right).command==WorldGrabCommand::Acquire);
    assert(controller.hands[1].entityId==73);
    // Trigger fires a forward/hand-velocity bounded throw and releases.
    in.handPosition={.08f,0.f,0.f};
    controller.update(0,in);
    in.trigger=true;
    auto thrown=controller.update(0,in);
    assert(thrown.command==WorldGrabCommand::Throw&&thrown.entityId==42);
    assert(thrown.velocity.x>0.f&&grabLength(thrown.velocity)<=14.001f);
    assert(left.entityId==0);
    // A tracked grip cannot automatically reattach after throwing.
    in.trigger=false;
    assert(controller.update(0,in).command==WorldGrabCommand::None);
    // Disable while holding drops, never throws.
    right.enabled=false;
    assert(controller.update(1,right).command==WorldGrabCommand::Drop);
    assert(controller.hands[1].entityId==0);
    // Invalid target class, too close and too far are all rejected.
    controller.reset();in={};
    in.enabled=in.gameplay=in.controllerTracked=in.freeHand=true;
    in.grip=true;in.entityId=42;in.levelGeneration=8;
    in.targetDistanceMeters=.6f;in.deltaSeconds=1.f/90.f;
    assert(controller.update(0,in).command==WorldGrabCommand::None);
    in.grip=false;controller.update(0,in);in.grip=true;
    in.verifiedBarrel=true;in.targetDistanceMeters=.1f;
    assert(controller.update(0,in).command==WorldGrabCommand::None);
    in.grip=false;controller.update(0,in);in.grip=true;
    in.targetDistanceMeters=.95f;
    assert(controller.update(0,in).command==WorldGrabCommand::None);
    in.grip=false;controller.update(0,in);in.grip=true;
    in.targetDistanceMeters=.5f;in.verifiedBarrel=false;
    assert(controller.update(0,in).command==WorldGrabCommand::None);
    // Tracking loss and level transitions force release, not physics writes.
    in.grip=false;controller.update(0,in);in.grip=true;in.verifiedBarrel=true;
    assert(controller.update(0,in).command==WorldGrabCommand::Acquire);
    in.levelGeneration=9;
    assert(controller.update(0,in).command==WorldGrabCommand::Drop);
    // Critical: a long physical separation releases before any follow force.
    in.grip=false;controller.update(0,in);
    in.grip=true;in.objectPosition={.5f,0.f,0.f};
    assert(controller.update(0,in).command==WorldGrabCommand::Acquire);
    in.objectPosition={2.f,0.f,0.f};
    assert(controller.update(0,in).command==WorldGrabCommand::Drop);
    // Weapon bracing and wheel have ownership priority over grabbing.
    in.grip=false;controller.update(0,in);
    in.grip=true;in.weaponSupportBusy=true;
    assert(controller.update(0,in).command==WorldGrabCommand::None);
    assert(grabLength(grabClampLength({30.f,0.f,0.f},8.f))<=8.0001f);
    return 0;
}
