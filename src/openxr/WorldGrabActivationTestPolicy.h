#pragma once
#include <cstdint>

// Experimental-only World Grab activation comparison. These modes do not
// change native DOOM gameplay, inventory or weapon hooks. The game-thread
// adapter performs every memory validation before touching a rigid body.
namespace kharvox {
inline constexpr bool worldGrabActivationTestValid(int mode) noexcept {
    return mode >= 1 && mode <= 5;
}
inline constexpr bool worldGrabActivationNeedsWake(int mode) noexcept {
    return mode >= 2 && mode <= 5;
}
inline constexpr bool worldGrabActivationContactProxy(int mode) noexcept {
    return mode == 3; // short position proxy, not a native collision callback
}
inline constexpr bool worldGrabActivationMicroMove(int mode) noexcept {
    return mode == 4; // 0.20 native unit upward offset
}
inline constexpr bool worldGrabActivationOwnerTickProbe(int mode) noexcept {
    return mode == 5; // observe native motion before hand override
}
inline constexpr bool worldGrabActivationFollowReady(
    int mode, bool nativeMotionObserved, std::uint64_t heldMs) noexcept {
    return worldGrabActivationTestValid(mode)
        && (mode != 5 || nativeMotionObserved || heldMs >= 350);
}
static_assert(!worldGrabActivationTestValid(0)
    && worldGrabActivationTestValid(1)
    && worldGrabActivationTestValid(5)
    && !worldGrabActivationTestValid(6));
static_assert(!worldGrabActivationNeedsWake(1)
    && worldGrabActivationNeedsWake(2)
    && worldGrabActivationNeedsWake(5));
static_assert(worldGrabActivationContactProxy(3)
    && !worldGrabActivationMicroMove(3)
    && worldGrabActivationMicroMove(4));
static_assert(worldGrabActivationFollowReady(1,false,0)
    && !worldGrabActivationFollowReady(5,false,349)
    && worldGrabActivationFollowReady(5,false,350)
    && worldGrabActivationFollowReady(5,true,0));
}
