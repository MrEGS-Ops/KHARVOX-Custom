#pragma once
#include <cstdint>
#include <cmath>

// Generic, mesh-agnostic DOOM physics target validation. A verified native
// collision trace must provide a real entity and a dynamic physics body.
// A rendered laser or focusable UI target is NEVER proof of physical contact.
namespace kharvox {
struct WorldGrabPhysicsHit {
    std::uint64_t entityId{},physicsBodyId{},levelGeneration{};
    float distanceMeters{};
    bool traceResolved{},entityHit{},hasPhysicsBody{},dynamicBody{},movable{};
    bool staticBody{},kinematic{},player{},character{},projectile{},scripted{};
};
// Positive capability checks are mandatory; a collision model by itself is
// not a movable rigid body. Unknown/unverified engine properties fail closed.
// Sleeping dynamic bodies may still qualify (and be awakened by native code).
inline bool worldGrabPhysicsEligible(const WorldGrabPhysicsHit& hit,
    std::uint64_t currentLevelGeneration,float maxReachMeters=.90f) noexcept {
    return hit.traceResolved&&hit.entityHit
        &&hit.entityId!=0&&hit.physicsBodyId!=0
        &&hit.levelGeneration!=0
        &&hit.levelGeneration==currentLevelGeneration
        &&hit.hasPhysicsBody&&hit.dynamicBody&&hit.movable
        &&!hit.staticBody&&!hit.kinematic&&!hit.player
        &&!hit.character&&!hit.projectile&&!hit.scripted
        &&std::isfinite(hit.distanceMeters)
        &&hit.distanceMeters>=0.f
        &&std::isfinite(maxReachMeters)&&maxReachMeters>0.f
        &&hit.distanceMeters<=maxReachMeters;
}
enum class WorldGrabLaserFeedback {Hidden,CyanAim,RedMovable};
inline WorldGrabLaserFeedback worldGrabLaserFeedback(bool laserVisible,
    const WorldGrabPhysicsHit& hit,std::uint64_t currentLevelGeneration,
    float maxReachMeters=.90f) noexcept {
    if(!laserVisible)return WorldGrabLaserFeedback::Hidden;
    return worldGrabPhysicsEligible(hit,currentLevelGeneration,maxReachMeters)
        ?WorldGrabLaserFeedback::RedMovable
        :WorldGrabLaserFeedback::CyanAim;
}
} // namespace kharvox
