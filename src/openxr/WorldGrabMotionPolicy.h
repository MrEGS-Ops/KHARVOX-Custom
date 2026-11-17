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

enum class WorldGrabCommand {None,Acquire,Follow,Drop,Throw};
struct WorldGrabInput {
    bool enabled{},gameplay{},controllerTracked{},freeHand{},grip{},trigger{};
    bool weaponSupportBusy{},wheelBusy{},holsterBusy{};
    // Generic verified native physics hit: no class-name or mesh whitelist.
    WorldGrabPhysicsHit hit{};
    std::uint64_t levelGeneration{};
    GrabVec3 handPosition{},handForward{},objectPosition{},objectVelocity{};
    float deltaSeconds{};
};
struct WorldGrabOutput {
    WorldGrabCommand command{WorldGrabCommand::None};
    std::uint64_t entityId{},physicsBodyId{};
    GrabVec3 velocity{}; // desired object velocity in metres/sec, NOT a physics write
};
struct WorldGrabHand {
    // A controller touching an object may grab it at zero ray distance.
    static constexpr float kMinimumAcquireMeters=0.f;
    static constexpr float kMaximumAcquireMeters=.90f;
    static constexpr float kMaximumHoldSeparationMeters=1.40f;
    std::uint64_t entityId{},physicsBodyId{},levelGeneration{};
    bool previousGrip{},previousTrigger{},velocitySampleValid{},observedGrip{};
    GrabVec3 lastHandPosition{},smoothedHandVelocity{};

    WorldGrabOutput update(const WorldGrabInput& in,
                           std::uint64_t entityHeldByOtherHand=0) noexcept {
        WorldGrabOutput out{};
        const bool validPose=grabFinite(in.handPosition)
            &&grabFinite(in.handForward)&&grabFinite(in.objectPosition)
            &&grabFinite(in.objectVelocity);
        const bool allowed=in.enabled&&in.gameplay&&in.controllerTracked
            &&in.freeHand&&!in.weaponSupportBusy&&!in.wheelBusy
            &&!in.holsterBusy&&validPose;
        // A newly enabled/reconnected controller must first report a Grip
        // release before its next press can acquire an entity.
        const bool newGrip=observedGrip&&in.grip&&!previousGrip;
        observedGrip=true;
        const bool newTrigger=in.trigger&&!previousTrigger;
        previousGrip=in.grip;previousTrigger=in.trigger;
        const float dt=in.deltaSeconds;
        if(allowed&&std::isfinite(dt)&&dt>.001f&&dt<.20f) {
            if(velocitySampleValid) {
                const auto instantaneous=grabClampLength(
                    grabScale(grabSub(in.handPosition,lastHandPosition),1.f/dt),12.f);
                smoothedHandVelocity=grabAdd(
                    grabScale(smoothedHandVelocity,.7f),
                    grabScale(instantaneous,.3f));
            }
            lastHandPosition=in.handPosition;
            velocitySampleValid=true;
        } else {
            velocitySampleValid=false;
            smoothedHandVelocity={};
        }
        if(entityId) {
            out.entityId=entityId;
            out.physicsBodyId=physicsBodyId;
            if(!allowed||!in.grip||in.levelGeneration!=levelGeneration
                ||grabLength(grabSub(in.handPosition,in.objectPosition))
                    >kMaximumHoldSeparationMeters) {
                out.command=WorldGrabCommand::Drop;
                entityId=physicsBodyId=levelGeneration=0;
                return out;
            }
            if(newTrigger) {
                const auto forward=grabClampLength(in.handForward,1.f);
                out.command=WorldGrabCommand::Throw;
                out.velocity=grabClampLength(grabAdd(
                    grabScale(smoothedHandVelocity,1.7f),
                    grabScale(forward,6.f)),14.f);
                entityId=physicsBodyId=levelGeneration=0;
                return out;
            }
            out.command=WorldGrabCommand::Follow;
            // Spring follow remains physics-owned. The native adapter must
            // apply a validated impulse/force; never set a guessed vtable slot.
            const auto error=grabSub(in.handPosition,in.objectPosition);
            out.velocity=grabClampLength(
                grabAdd(grabScale(error,7.f),
                    grabScale(smoothedHandVelocity,.35f)),8.f);
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
