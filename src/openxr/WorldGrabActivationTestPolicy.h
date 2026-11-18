#pragma once
#include <cstdint>

// Experimental-only World Grab activation comparison. These modes do not
// change native DOOM gameplay, inventory or weapon hooks. The game-thread
// adapter performs every memory validation before touching a rigid body.
namespace kharvox {
inline constexpr bool worldGrabActivationTestValid(int mode) noexcept {
    return mode >= 1 && mode <= 5;
}
// Preserve the previously working gated, 28-unit Strong Anchor unchanged
// as its own DEV choice. It is NOT one of the five no-kick hypotheses.
inline constexpr bool worldGrabStrongAnchor(int mode) noexcept {
    return mode == 6;
}
inline constexpr bool worldGrabAnchorMode(int mode) noexcept {
    return worldGrabActivationTestValid(mode) || worldGrabStrongAnchor(mode);
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
    if(!worldGrabActivationTestValid(mode))return false;
    // Direct bypasses the artificial native-motion gate. Wake/proxy methods
    // get real game ticks before temporary held-body suspension begins.
    if(mode==1)return true;
    if(nativeMotionObserved)return true;
    return heldMs >= (mode==5?350u:80u);
}
inline constexpr bool worldGrabAnchorFollowReady(
    int mode, bool nativeMotionObserved, std::uint64_t heldMs) noexcept {
    // Original Strong Anchor waits for demonstrated DOOM-native movement;
    // all five new tests retain their independent activation policy.
    return worldGrabStrongAnchor(mode) ? nativeMotionObserved
        : worldGrabActivationFollowReady(mode,nativeMotionObserved,heldMs);
}
static_assert(worldGrabStrongAnchor(6) && worldGrabAnchorMode(6)
    && !worldGrabAnchorMode(7));
static_assert(!worldGrabAnchorFollowReady(6,false,1000)
    && worldGrabAnchorFollowReady(6,true,0));
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
    && !worldGrabActivationFollowReady(2,false,79)
    && worldGrabActivationFollowReady(2,false,80)
    && !worldGrabActivationFollowReady(5,false,349)
    && worldGrabActivationFollowReady(5,false,350)
    && worldGrabActivationFollowReady(5,true,0));
}
