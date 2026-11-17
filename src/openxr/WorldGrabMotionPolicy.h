#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include "WorldGrabTargetPolicy.h"

// Pure VR-to-physics *intent* stage. No DOOM pointers, CVar changes, save writes,
// entity scans or physics calls are permitted here. A separately verified
// game-thread native adapter must resolve the target and execute each command.
namespace kharvox {
struct GrabVec3 {
    float x{}, y{}, z{};
};
inline GrabVec3 grabAdd(GrabVec3 a, GrabVec3 b) noexcept {
    return {a.x+b.x,a.y+b.y,a.z+b.z};
}
inline GrabVec3 grabSub(GrabVec3 a, GrabVec3 b) noexcept {
    return {a.x-b.x,a.y-b.y,a.z-b.z};
}
inline GrabVec3 grabScale(GrabVec3 a,float scale) noexcept {
    return {a.x*scale,a.y*scale,a.z*scale};
}
inline float grabLength(GrabVec3 a) noexcept {
    return std::sqrt(a.x*a.x+a.y*a.y+a.z*a.z);
}
inline bool grabFinite(GrabVec3 a) noexcept {
    return std::isfinite(a.x)&&std::isfinite(a.y)&&std::isfinite(a.z);
}
inline GrabVec3 grabClampLength(GrabVec3 v,float maxMagnitude) noexcept {
    const float length=grabLength(v);
    if(!std::isfinite(length)||length<=0.f)return {};
    if(length<=maxMagnitude)return v;
    return grabScale(v,maxMagnitude/length);
}

// A validated native grenade-launch snapshot, supplied by a future
// game-thread adapter from DOOM's equipment projectile launch path.
// Values are in DOOM *native* units, not OpenXR tracking metres.
// This deliberately contains BALLISTICS only: never transfer a grenade's
// class, damage, fuse, explosion, scripting, ownership or inventory effects.
struct WorldGrabGrenadeBallistics {
    std::uint64_t levelGeneration{};
    GrabVec3 initialVelocityNative{};
    float gravityScale{}, linearDamping{};
    bool verifiedNativeLaunch{},verifiedFlightProperties{};
};
inline bool worldGrabGrenadeBallisticsReady(
    const WorldGrabGrenadeBallistics& grenade,
    std::uint64_t levelGeneration) noexcept {
    return levelGeneration!=0&&grenade.levelGeneration==levelGeneration
        &&grenade.verifiedNativeLaunch&&grenade.verifiedFlightProperties
        &&grabFinite(grenade.initialVelocityNative)
        &&std::isfinite(grabLength(grenade.initialVelocityNative))
        &&grabLength(grenade.initialVelocityNative)>0.f
        &&std::isfinite(grenade.gravityScale)
        &&grenade.gravityScale>=0.f
        &&std::isfinite(grenade.linearDamping)
        &&grenade.linearDamping>=0.f;
}
// Measured collision bounds for the selected native movable object.
// The game-thread adapter must convert DOOM's body collider into the SAME
// metre-space as the controller. These are the ITEM'S real half sizes and
// orientation, never a fixed-size sphere, guessed barrel radius, or hand
// suction target. The original Vacuum destination stays inside DOOM.
struct WorldGrabCatchGeometry {
    std::uint64_t entityId{},physicsBodyId{},levelGeneration{};
    bool verifiedCollisionBounds{},nativePickupConsumptionBlocked{};
    GrabVec3 centreMeters{},halfExtentsMeters{},handPositionMeters{};
    std::array<GrabVec3,3> localAxes{{
        {1.f,0.f,0.f},{0.f,1.f,0.f},{0.f,0.f,1.f}
    }};
};
inline float grabDot(GrabVec3 a,GrabVec3 b) noexcept {
    return a.x*b.x+a.y*b.y+a.z*b.z;
}
// Distance from hand to *surface of the item's oriented collider*.
// For a 1 m cube contacted face-on this reaches zero at exactly 0.5 m
// from its centre, not at the centre itself.
inline float grabHandToColliderGap(const WorldGrabCatchGeometry& shape) noexcept {
    const auto& e=shape.halfExtentsMeters;
    if(!shape.verifiedCollisionBounds
        ||!grabFinite(shape.centreMeters)||!grabFinite(shape.handPositionMeters)
        ||!grabFinite(e)||e.x<=0.f||e.y<=0.f||e.z<=0.f)
        return INFINITY;
    for(unsigned i=0;i<3;++i) {
        const auto a=shape.localAxes[i];
        if(!grabFinite(a)||std::abs(grabDot(a,a)-1.f)>.02f)
            return INFINITY;
        for(unsigned j=0;j<i;++j)
            if(std::abs(grabDot(a,shape.localAxes[j]))>.02f)
                return INFINITY;
    }
    const auto offset=grabSub(shape.handPositionMeters,shape.centreMeters);
    GrabVec3 nearest=shape.centreMeters;
    const float halfSizes[3]{e.x,e.y,e.z};
    for(unsigned i=0;i<3;++i) {
        const auto a=shape.localAxes[i];
        const auto displacement=std::max(-halfSizes[i],std::min(halfSizes[i],grabDot(offset,a)));
        nearest=grabAdd(nearest,grabScale(a,displacement));
    }
    return grabLength(grabSub(shape.handPositionMeters,nearest));
}
inline bool worldGrabCatchAtHand(const WorldGrabCatchGeometry& shape,
    std::uint64_t entityId,std::uint64_t physicsBodyId,
    std::uint64_t generation,float contactToleranceMeters=.015f) noexcept {
    if(!entityId||!physicsBodyId||!generation
        ||shape.entityId!=entityId||shape.physicsBodyId!=physicsBodyId
        ||shape.levelGeneration!=generation
        ||!shape.nativePickupConsumptionBlocked
        ||!std::isfinite(contactToleranceMeters)
        ||contactToleranceMeters<0.f)return false;
    return grabHandToColliderGap(shape)<=contactToleranceMeters;
}
// The ray SELECTS an object. Grip enables native-Vacuum-style travel to
// DOOM's ORIGINAL target. The gripping hand is an interception point only.
// Catch stops attraction before item collection; never pull toward the hand.
enum class WorldGrabCommand {None,Acquire,VacuumAttract,Catch,CaughtHold,Drop,Throw};
struct WorldGrabInput {
    bool enabled{},gameplay{},controllerTracked{},freeHand{},grip{},trigger{};
    bool weaponSupportBusy{},wheelBusy{},holsterBusy{};
    // Generic verified native physics hit: no class-name or mesh whitelist.
    WorldGrabPhysicsHit hit{};
    std::uint64_t levelGeneration{};
    // Geometric CONTACT only: the hand is NOT the Vacuum destination.
    // Unverified/mismatched bounds must never cause a catch.
    WorldGrabCatchGeometry catchGeometry{};
    // Actual native grenade launch data only, never hand-swing strength.
    WorldGrabGrenadeBallistics nativeGrenade{};
};
struct WorldGrabOutput {
    WorldGrabCommand command{WorldGrabCommand::None};
    std::uint64_t entityId{},physicsBodyId{};
    // VACUUM_ATTRACT: let the selected object travel toward DOOM's existing
    // pickup destination, never the hand. CATCH: stop that native motion
    // when the object's measured collider contacts the gripping hand.
    // CAUGHT_HOLD: keep the intercepted object available for trigger launch.
    // All states must suppress native pickup consumption and never reclassify
    // the entity. THROW: copy native grenade ballistics to the SAME body and
    // restore its original physics after drop/throw. No grenade is spawned.
    WorldGrabGrenadeBallistics nativeGrenade{};
};
struct WorldGrabHand {
    // A controller touching an object may grab it at zero ray distance.
    static constexpr float kMinimumAcquireMeters=0.f;
    static constexpr float kMaximumAcquireMeters=.90f;
    std::uint64_t entityId{},physicsBodyId{},levelGeneration{};
    bool previousGrip{},previousTrigger{},observedGrip{},caught{};

    WorldGrabOutput update(const WorldGrabInput& in,
                           std::uint64_t entityHeldByOtherHand=0) noexcept {
        WorldGrabOutput out{};
        const bool allowed=in.enabled&&in.gameplay&&in.controllerTracked
            &&in.freeHand&&!in.weaponSupportBusy&&!in.wheelBusy
            &&!in.holsterBusy;
        // A newly enabled/reconnected controller must first report a Grip
        // release before its next press can acquire an entity.
        const bool newGrip=observedGrip&&in.grip&&!previousGrip;
        observedGrip=true;
        const bool newTrigger=in.trigger&&!previousTrigger;
        previousGrip=in.grip;previousTrigger=in.trigger;
        if(entityId) {
            out.entityId=entityId;
            out.physicsBodyId=physicsBodyId;
            if(!allowed||!in.grip||in.levelGeneration!=levelGeneration) {
                out.command=WorldGrabCommand::Drop;
                caught=false;
                entityId=physicsBodyId=levelGeneration=0;
                return out;
            }
            if(caught&&newTrigger&&worldGrabGrenadeBallisticsReady(
                    in.nativeGrenade,in.levelGeneration)) {
                // DOOM's native equipment launch velocity / gravity / drag
                // pass through unmodified. First clear temporary Vacuum
                // eligibility, then launch the SAME existing entity.
                // This is intent only; never spawn a grenade projectile.
                out.command=WorldGrabCommand::Throw;
                out.nativeGrenade=in.nativeGrenade;
                caught=false;
                entityId=physicsBodyId=levelGeneration=0;
                return out;
            }
            if(caught){
                out.command=WorldGrabCommand::CaughtHold;
                return out;
            }
            // Measured real body bounds prove physical hand interception.
            // Do not redirect Vacuum toward the hand. A native adapter is
            // REQUIRED to enforce the stop while preserving the entity.
            if(worldGrabCatchAtHand(in.catchGeometry,
                    entityId,physicsBodyId,levelGeneration)){
                caught=true;
                out.command=WorldGrabCommand::Catch;
                return out;
            }
            // No verified collider/consumption guard -> no Vacuum activation:
            // otherwise a real engine pickup might swallow the barrel.
            const auto& geom=in.catchGeometry;
            if(geom.entityId!=entityId||geom.physicsBodyId!=physicsBodyId
                ||geom.levelGeneration!=levelGeneration
                ||!geom.nativePickupConsumptionBlocked
                ||!std::isfinite(grabHandToColliderGap(geom)))
                return out;
            out.command=WorldGrabCommand::VacuumAttract;
            return out;
        }
        if(!allowed||!newGrip
            ||!worldGrabPhysicsEligible(in.hit,in.levelGeneration,
                kMaximumAcquireMeters)
            ||in.hit.distanceMeters<kMinimumAcquireMeters
            ||in.hit.entityId==entityHeldByOtherHand) return out;
        entityId=in.hit.entityId;
        physicsBodyId=in.hit.physicsBodyId;
        levelGeneration=in.levelGeneration;
        caught=false;
        out.command=WorldGrabCommand::Acquire;
        out.entityId=entityId;
        out.physicsBodyId=physicsBodyId;
        return out;
    }

    void reset() noexcept { *this={}; }
};
struct WorldGrabController {
    std::array<WorldGrabHand,2> hands{}; // LEFT, RIGHT
    WorldGrabOutput update(unsigned hand,const WorldGrabInput& input) noexcept {
        if(hand>=hands.size())return {};
        return hands[hand].update(input,hands[1u-hand].entityId);
    }
    void reset() noexcept {for(auto& hand:hands)hand.reset();}
};
} // namespace kharvox
