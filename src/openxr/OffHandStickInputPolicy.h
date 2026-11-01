#pragma once
#include <cstdint>

namespace kharvox {
// Both thumbstick clicks are a dedicated calibration chord. Never emit
// regular right-stick melee or left-stick pause while EITHER stick is held:
// an arbitrary interval can elapse before the second click arrives.
// Solo actions are emitted once, ON RELEASE, only if the other stick was
// never down at any time during that hold. This is necessary to guarantee
// calibration cannot trigger a normal melee animation or a pause.
// With Off-Hand Grip Capture disabled, the normal DOOM controls are used
// instead of this policy, and remain unchanged.
struct OffHandDelayedClick {
    bool held{},blocked{};
    bool update(bool down,bool other,std::uint64_t ms) noexcept {
        (void)ms;
        if(!down){
            const bool singleClick=held&&!blocked;
            held=false;
            blocked=false;
            return singleClick;
        }
        if(!held){
            held=true;
            blocked=other;
        }
        if(other)blocked=true;
        return false;
    }
};
struct OffHandStickOutput {bool rightMelee{},leftPause{};};
struct OffHandStickInput {
    OffHandDelayedClick right{},left{};
    OffHandStickOutput update(bool rightPressed,bool leftPressed,
                              bool enabled,std::uint64_t ms) noexcept {
        if(!enabled){right={};left={};return {};}
        return {right.update(rightPressed,leftPressed,ms),
                left.update(leftPressed,rightPressed,ms)};
    }
};
} // namespace kharvox
