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
    // True 1 m cube with measured 0.5 m half extent along each axis.
    // DOOM's original Vacuum destination is intentionally NOT an input.
    in.catchGeometry={42,420,5,true,true,
        {0.f,0.f,0.f},{.5f,.5f,.5f},{1.5f,0.f,0.f}};

    // A held Grip on enable must not auto-acquire.
    in.grip=true;
    assert(controller.update(0,in).command==WorldGrabCommand::None);
    in.grip=false;
    controller.update(0,in);
    in.grip=true;
    const auto acquired=controller.update(0,in);
    assert(acquired.command==WorldGrabCommand::Acquire);
    assert(acquired.entityId==42&&acquired.physicsBodyId==420);

    // Hand is only a catching/interception position. Vacuum continues
    // toward the native DOOM pickup destination while the object is away.
    const auto vacuum=controller.update(0,in);
    assert(vacuum.command==WorldGrabCommand::VacuumAttract);
    assert(vacuum.entityId==42&&vacuum.physicsBodyId==420);
    assert(!controller.hands[0].caught);
    // Contact is object-specific: a 1 m cube face reaches a hand at x=.5
    // when its centre is x=0. Object centre must never touch the hand.
    assert(std::abs(grabHandToColliderGap(in.catchGeometry)-1.f)<.00001f);
    in.catchGeometry.handPositionMeters={.52f,0.f,0.f};
    assert(grabHandToColliderGap(in.catchGeometry)>.019f);
    assert(controller.update(0,in).command==WorldGrabCommand::VacuumAttract);

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
    right.catchGeometry.entityId=73;
    right.catchGeometry.physicsBodyId=730;
    right.grip=true;
    assert(controller.update(1,right).command==WorldGrabCommand::Acquire);
    assert(controller.update(1,right).command==WorldGrabCommand::VacuumAttract);

    // Trigger before a CATCH cannot throw even with verified launch data.
    // The box must physically arrive at the hand first.
    in.trigger=true;
    auto unverified=controller.update(0,in);
    assert(unverified.command==WorldGrabCommand::VacuumAttract);
    assert(controller.hands[0].entityId==42);
    in.trigger=false;
    controller.update(0,in);
    in.catchGeometry.handPositionMeters={.5f,0.f,0.f};
    assert(grabHandToColliderGap(in.catchGeometry)==0.f);
    const auto caught=controller.update(0,in);
    assert(caught.command==WorldGrabCommand::Catch);
    assert(controller.hands[0].caught);
    in.catchGeometry.handPositionMeters={2.f,0.f,0.f};
    assert(controller.update(0,in).command==WorldGrabCommand::CaughtHold);

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

    // A separate catch test: unverified collider or active pickup
    // consumption MUST NEVER cause attraction/catch.
    WorldGrabInput unguarded=in;
    controller.reset();
    unguarded.grip=true;
    unguarded.trigger=false;
    unguarded.catchGeometry.handPositionMeters={.5f,0.f,0.f};
    controller.update(0,unguarded);
    unguarded.grip=false;
    controller.update(0,unguarded);
    unguarded.grip=true;
    assert(controller.update(0,unguarded).command==WorldGrabCommand::Acquire);
    unguarded.catchGeometry.nativePickupConsumptionBlocked=false;
    assert(controller.update(0,unguarded).command==WorldGrabCommand::None);
    unguarded.catchGeometry.nativePickupConsumptionBlocked=true;
    unguarded.catchGeometry.verifiedCollisionBounds=false;
    assert(controller.update(0,unguarded).command==WorldGrabCommand::None);
    unguarded.catchGeometry.verifiedCollisionBounds=true;
    assert(controller.update(0,unguarded).command==WorldGrabCommand::Catch);
    controller.reset();


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

    // Real OBB maths: a 1x1x1 item catches at 0.5 m FACE distance,
    // not when its centre reaches the hand; a rotated thin item uses
    // the oriented collider and not a guessed universal 50 cm.
    WorldGrabCatchGeometry sample{42,420,8,true,true,
        {0.f,0.f,0.f},{.5f,.5f,.5f},{.5f,0.f,0.f}};
    assert(worldGrabCatchAtHand(sample,42,420,8));
    sample.handPositionMeters={.7f,0.f,0.f};
    assert(!worldGrabCatchAtHand(sample,42,420,8));
    assert(!worldGrabCatchAtHand(sample,42,420,7)); // stale generation
    sample.halfExtentsMeters={.5f,.1f,.2f};
    sample.handPositionMeters={.0f,.1f,0.f};
    assert(worldGrabCatchAtHand(sample,42,420,8));
    sample.localAxes={{{0.f,1.f,0.f},{-1.f,0.f,0.f},{0.f,0.f,1.f}}};
    sample.handPositionMeters={.1f,0.f,0.f};
    assert(worldGrabCatchAtHand(sample,42,420,8));
    sample.halfExtentsMeters={0.f,.1f,.2f};
    assert(!worldGrabCatchAtHand(sample,42,420,8));
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
