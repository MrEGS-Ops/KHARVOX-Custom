#pragma once
#include <cstdint>

namespace kharvox {
// The VR menu owns BOTH thumbstick clicks before this helper runs.
// Never emit regular right-stick melee or left-stick pause while EITHER
// stick is held, so a staggered two-stick menu chord cannot cause a
// premature melee or pause action. Independent clicks fire on RELEASE
// only when the opposite stick was never held during that press.
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
