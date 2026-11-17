#include "../src/openxr/WorldGrabTargetPolicy.h"
#include <cassert>
#include <limits>

int main() {
    using namespace kharvox;
    WorldGrabPhysicsHit hit{42,420,5,.6f,true,true,true,true,true};
    assert(worldGrabPhysicsEligible(hit,5));
    assert(worldGrabLaserFeedback(true,hit,5)==WorldGrabLaserFeedback::RedMovable);
    assert(worldGrabLaserFeedback(false,hit,5)==WorldGrabLaserFeedback::Hidden);
    hit.distanceMeters=0.f; // physical hand contact also qualifies
    assert(worldGrabPhysicsEligible(hit,5));
    hit.distanceMeters=.901f;assert(!worldGrabPhysicsEligible(hit,5));
    hit.distanceMeters=std::numeric_limits<float>::quiet_NaN();
    assert(!worldGrabPhysicsEligible(hit,5));
    hit.distanceMeters=.6f;
    assert(!worldGrabPhysicsEligible(hit,6)); // stale level handle
    hit.traceResolved=false;assert(!worldGrabPhysicsEligible(hit,5));
    assert(worldGrabLaserFeedback(true,hit,5)==WorldGrabLaserFeedback::CyanAim);
    hit.traceResolved=true;
    hit.physicsBodyId=0;assert(!worldGrabPhysicsEligible(hit,5));
    hit.physicsBodyId=420;
    hit.hasPhysicsBody=false;assert(!worldGrabPhysicsEligible(hit,5));
    hit.hasPhysicsBody=true;
    hit.dynamicBody=false;assert(!worldGrabPhysicsEligible(hit,5));
    hit.dynamicBody=true;
    hit.movable=false;assert(!worldGrabPhysicsEligible(hit,5));
    hit.movable=true;
    hit.staticBody=true;assert(!worldGrabPhysicsEligible(hit,5));
    hit.staticBody=false;
    hit.kinematic=true;assert(!worldGrabPhysicsEligible(hit,5));
    hit.kinematic=false;
    hit.player=true;assert(!worldGrabPhysicsEligible(hit,5));
    hit.player=false;
    hit.character=true;assert(!worldGrabPhysicsEligible(hit,5));
    hit.character=false;
    hit.projectile=true;assert(!worldGrabPhysicsEligible(hit,5));
    hit.projectile=false;
    hit.scripted=true;assert(!worldGrabPhysicsEligible(hit,5));
    hit.scripted=false;
    assert(worldGrabPhysicsEligible(hit,5)); // name/mesh irrelevant
}
