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
// The native Vacuum destination remains exactly DOOM's original pickup
// destination; the VR hand selects one object and never becomes a pull target.
// No physics-gun tether, spring, or directional Vacuum is implemented.
enum class WorldGrabCommand {None,Acquire,VacuumAttract,Drop,Throw};
struct WorldGrabInput {
    bool enabled{},gameplay{},controllerTracked{},freeHand{},grip{},trigger{};
    bool weaponSupportBusy{},wheelBusy{},holsterBusy{};
    // Generic verified native physics hit: no class-name or mesh whitelist.
    WorldGrabPhysicsHit hit{};
    std::uint64_t levelGeneration{};
    // Actual native grenade launch data only. The Vacuum target uses DOOM's
    // original pickup destination: controller position is NOT used to pull.
    WorldGrabGrenadeBallistics nativeGrenade{};
};
struct WorldGrabOutput {
    WorldGrabCommand command{WorldGrabCommand::None};
    std::uint64_t entityId{},physicsBodyId{};
    // VACUUM_ATTRACT: apply the selected object's *temporary*, explicitly
    // verified native item-attraction behaviour to DOOM's original pickup
    // destination. Never change the entity class or allow consumption.
    // THROW: copy of native grenade ballistic parameters. A native adapter
    // must restore original object properties after throw/drop; no grenade
    // entity or explosive flags are copied.
    WorldGrabGrenadeBallistics nativeGrenade{};
};
struct WorldGrabHand {
    // A controller touching an object may grab it at zero ray distance.
    static constexpr float kMinimumAcquireMeters=0.f;
    static constexpr float kMaximumAcquireMeters=.90f;
    std::uint64_t entityId{},physicsBodyId{},levelGeneration{};
    bool previousGrip{},previousTrigger{},observedGrip{};

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
                entityId=physicsBodyId=levelGeneration=0;
                return out;
            }
            if(newTrigger&&worldGrabGrenadeBallisticsReady(
                    in.nativeGrenade,in.levelGeneration)) {
                // DOOM's native equipment launch velocity / gravity / drag
                // pass through unmodified. First clear temporary Vacuum
                // eligibility, then launch the SAME existing entity.
                // This is intent only; never spawn a grenade projectile.
                out.command=WorldGrabCommand::Throw;
                out.nativeGrenade=in.nativeGrenade;
                entityId=physicsBodyId=levelGeneration=0;
                return out;
            }
            // Keep the original DOOM item-Vacuum destination, not a moving
            // controller/hand target. A native adapter is REQUIRED: until
            // verified, this command changes no entity or pickup properties.
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
