#include "../src/openxr/WorldGrabMotionPolicy.h"
#include <cassert>
#include <cmath>

using namespace kharvox;

int main() {
    WorldGrabController controller{};
    WorldGrabInput in{};
    in.enabled=in.gameplay=in.controllerTracked=in.freeHand=true;
    in.levelGeneration=5;
    in.hit={42,420,5,.6f,true,true,true,true,true};

    // A held Grip on enable must not auto-acquire.
    in.grip=true;
    assert(controller.update(0,in).command==WorldGrabCommand::None);
    in.grip=false;
    controller.update(0,in);
    in.grip=true;
    const auto acquired=controller.update(0,in);
    assert(acquired.command==WorldGrabCommand::Acquire);
    assert(acquired.entityId==42&&acquired.physicsBodyId==420);

    // The laser/hand is ONLY a selector. Once acquired, no hand-position,
    // direction, frame-speed, or object-to-hand tether is required.
    const auto vacuum=controller.update(0,in);
    assert(vacuum.command==WorldGrabCommand::VacuumAttract);
    assert(vacuum.entityId==42&&vacuum.physicsBodyId==420);
    assert(vacuum.nativeGrenade.initialVelocityNative.x==0.f);

    // One object cannot be assigned to both hands simultaneously.
    WorldGrabInput right=in;
    assert(controller.update(1,right).command==WorldGrabCommand::None);
    right.grip=false;
    controller.update(1,right);
    right.grip=true;
    assert(controller.update(1,right).command==WorldGrabCommand::None);
    // Another verified dynamic body can be selected independently.
    right.grip=false;
    controller.update(1,right);
    right.hit.entityId=73;
    right.hit.physicsBodyId=730;
    right.grip=true;
    assert(controller.update(1,right).command==WorldGrabCommand::Acquire);
    assert(controller.update(1,right).command==WorldGrabCommand::VacuumAttract);

    // Trigger must not synthesize a grenade velocity: without authentic
    // ballistics, continue native Vacuum-attraction intent.
    in.trigger=true;
    auto unverified=controller.update(0,in);
    assert(unverified.command==WorldGrabCommand::VacuumAttract);
    assert(controller.hands[0].entityId==42);
    in.trigger=false;
    controller.update(0,in);

    const WorldGrabGrenadeBallistics grenade{
        5,{123.f,17.f,-4.f},.75f,.035f,true,true
    };
    assert(worldGrabGrenadeBallisticsReady(grenade,5));
    in.nativeGrenade=grenade;
    in.trigger=true;
    const auto thrown=controller.update(0,in);
    assert(thrown.command==WorldGrabCommand::Throw);
    assert(thrown.entityId==42&&thrown.physicsBodyId==420);
    assert(thrown.nativeGrenade.initialVelocityNative.x==123.f);
    assert(thrown.nativeGrenade.initialVelocityNative.y==17.f);
    assert(thrown.nativeGrenade.initialVelocityNative.z==-4.f);
    assert(thrown.nativeGrenade.gravityScale==.75f);
    assert(thrown.nativeGrenade.linearDamping==.035f);
    assert(controller.hands[0].entityId==0);
    // Trigger cannot auto-acquire while Grip remains down.
    in.trigger=false;
    assert(controller.update(0,in).command==WorldGrabCommand::None);

    // Release without throw drops the selected entity.
    right.grip=false;
    const auto dropped=controller.update(1,right);
    assert(dropped.command==WorldGrabCommand::Drop);
    assert(dropped.entityId==73);
    assert(controller.hands[1].entityId==0);

    // Tracking loss and level transitions revoke temporary Vacuum intent.
    controller.reset();
    in={};
    in.enabled=in.gameplay=in.controllerTracked=in.freeHand=true;
    in.levelGeneration=8;
    in.hit={42,420,8,.5f,true,true,true,true,true};
    in.grip=true;
    assert(controller.update(0,in).command==WorldGrabCommand::None);
    in.grip=false;
    controller.update(0,in);
    in.grip=true;
    assert(controller.update(0,in).command==WorldGrabCommand::Acquire);
    in.controllerTracked=false;
    assert(controller.update(0,in).command==WorldGrabCommand::Drop);
    in.controllerTracked=true;
    in.grip=false;
    controller.update(0,in);
    in.grip=true;
    assert(controller.update(0,in).command==WorldGrabCommand::Acquire);
    in.levelGeneration=9;
    assert(controller.update(0,in).command==WorldGrabCommand::Drop);

    // Native Vacuum cannot be granted to arbitrary or stale hit handles.
    controller.reset();
    in={};
    in.enabled=in.gameplay=in.controllerTracked=in.freeHand=true;
    in.levelGeneration=8;
    in.hit={42,420,8,.5f,true,true,true,true,true};
    in.grip=true;
    assert(controller.update(0,in).command==WorldGrabCommand::None);
    in.grip=false;
    controller.update(0,in);
    in.grip=true;
    in.hit.staticBody=true;
    assert(controller.update(0,in).command==WorldGrabCommand::None);
    in.grip=false;
    controller.update(0,in);
    in.grip=true;
    in.hit.staticBody=false;
    in.hit.kinematic=true;
    assert(controller.update(0,in).command==WorldGrabCommand::None);
    in.grip=false;
    controller.update(0,in);
    in.grip=true;
    in.hit.kinematic=false;
    in.hit.character=true;
    assert(controller.update(0,in).command==WorldGrabCommand::None);
    in.grip=false;
    controller.update(0,in);
    in.grip=true;
    in.hit.character=false;
    in.hit.traceResolved=false;
    assert(controller.update(0,in).command==WorldGrabCommand::None);
    in.grip=false;
    controller.update(0,in);
    in.grip=true;
    in.hit.traceResolved=true;
    in.hit.distanceMeters=.95f;
    assert(controller.update(0,in).command==WorldGrabCommand::None);
    in.grip=false;
    controller.update(0,in);
    in.grip=true;
    in.hit.distanceMeters=0.f;
    assert(controller.update(0,in).command==WorldGrabCommand::Acquire);
    in.grip=false;
    assert(controller.update(0,in).command==WorldGrabCommand::Drop);

    // Weapon bracing and wheel state take priority.
    controller.reset();
    in={};
    in.enabled=in.gameplay=in.controllerTracked=in.freeHand=true;
    in.levelGeneration=8;
    in.hit={42,420,8,.5f,true,true,true,true,true};
    in.grip=true;
    controller.update(0,in);
    in.grip=false;
    controller.update(0,in);
    in.grip=true;
    in.weaponSupportBusy=true;
    assert(controller.update(0,in).command==WorldGrabCommand::None);

    // Only native, finite grenade ballistics may enable trigger launch.
    auto invalid=grenade;
    invalid.levelGeneration=9;
    assert(!worldGrabGrenadeBallisticsReady(invalid,8));
    invalid=grenade;invalid.verifiedNativeLaunch=false;
    assert(!worldGrabGrenadeBallisticsReady(invalid,5));
    invalid=grenade;invalid.verifiedFlightProperties=false;
    assert(!worldGrabGrenadeBallisticsReady(invalid,5));
    invalid=grenade;invalid.initialVelocityNative={};
    assert(!worldGrabGrenadeBallisticsReady(invalid,5));
    invalid=grenade;invalid.gravityScale=NAN;
    assert(!worldGrabGrenadeBallisticsReady(invalid,5));
    invalid=grenade;invalid.linearDamping=-.01f;
    assert(!worldGrabGrenadeBallisticsReady(invalid,5));
    return 0;
}
