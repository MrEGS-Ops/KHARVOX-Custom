#include "../src/openxr/WorldGrabMotionPolicy.h"
#include <cassert>
#include <cmath>
using namespace kharvox;
int main(){
    WorldGrabController controller{};
    WorldGrabInput in{};
    in.enabled=in.gameplay=in.controllerTracked=in.freeHand=true;
    in.grip=true;in.levelGeneration=5;
    in.hit={42,420,5,.6f,true,true,true,true,true};
    in.deltaSeconds=1.f/90.f;
    in.handForward={1.f,0.f,0.f};
    in.handPosition={0.f,0.f,0.f};in.objectPosition={.6f,0.f,0.f};
    // Grip already down on enable: initialization must not auto-grab.
    auto& left=controller.hands[0];
    assert(controller.update(0,in).command==WorldGrabCommand::None);
    in.grip=false;controller.update(0,in);
    in.grip=true;
    auto acquired=controller.update(0,in);
    assert(acquired.command==WorldGrabCommand::Acquire&&acquired.entityId==42
        &&acquired.physicsBodyId==420);
    assert(left.entityId==42);
    auto follow=controller.update(0,in);
    assert(follow.command==WorldGrabCommand::Follow);
    assert(follow.velocity.x<0.f);
    assert(grabLength(follow.velocity)<=8.001f);
    // Same object cannot be owned by two hands
    WorldGrabInput right=in;
    assert(controller.update(1,right).command==WorldGrabCommand::None);
    // Other objects can be held independently
    right.hit.entityId=73;right.hit.physicsBodyId=730;
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
    // Invalid/missing physics, targets behind the hand, and targets outside
    // reach are rejected; direct contact at zero distance is permitted.
    controller.reset();in={};
    in.enabled=in.gameplay=in.controllerTracked=in.freeHand=true;
    in.grip=true;in.levelGeneration=8;
    in.hit={42,420,8,.6f,true,true,true,true,true};
    in.deltaSeconds=1.f/90.f;
    assert(controller.update(0,in).command==WorldGrabCommand::None);
    in.grip=false;controller.update(0,in);in.grip=true;
    in.hit.distanceMeters=-.1f;
    assert(controller.update(0,in).command==WorldGrabCommand::None);
    in.grip=false;controller.update(0,in);in.grip=true;
    in.hit.distanceMeters=.95f;
    assert(controller.update(0,in).command==WorldGrabCommand::None);
    in.grip=false;controller.update(0,in);in.grip=true;
    in.hit.distanceMeters=.5f;in.hit.dynamicBody=false;
    assert(controller.update(0,in).command==WorldGrabCommand::None);
    in.hit.dynamicBody=true;
    // Static, kinematic, actor and unverified collision objects are excluded,
    // even when they have a physics/collision component.
    in.grip=false;controller.update(0,in);in.grip=true;in.hit.staticBody=true;
    assert(controller.update(0,in).command==WorldGrabCommand::None);
    in.grip=false;controller.update(0,in);in.grip=true;
    in.hit.staticBody=false;in.hit.kinematic=true;
    assert(controller.update(0,in).command==WorldGrabCommand::None);
    in.grip=false;controller.update(0,in);in.grip=true;
    in.hit.kinematic=false;in.hit.character=true;
    assert(controller.update(0,in).command==WorldGrabCommand::None);
    in.grip=false;controller.update(0,in);in.grip=true;
    in.hit.character=false;in.hit.traceResolved=false;
    assert(controller.update(0,in).command==WorldGrabCommand::None);
    in.grip=false;controller.update(0,in);in.grip=true;
    in.hit.traceResolved=true;in.hit.distanceMeters=0.f;
    assert(controller.update(0,in).command==WorldGrabCommand::Acquire);
    in.grip=false;
    assert(controller.update(0,in).command==WorldGrabCommand::Drop);
    // Tracking loss and level transitions force release, not physics writes.
    in.grip=false;controller.update(0,in);in.grip=true;
    in.hit.distanceMeters=.5f;in.hit.dynamicBody=true;
    assert(controller.update(0,in).command==WorldGrabCommand::Acquire);
    in.levelGeneration=9;
    assert(controller.update(0,in).command==WorldGrabCommand::Drop);
    // Critical: a long physical separation releases before any follow force.
    in.grip=false;controller.update(0,in);
    in.grip=true;in.hit.levelGeneration=9;in.objectPosition={.5f,0.f,0.f};
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
