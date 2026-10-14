#pragma once
#include <cstdint>

namespace kharvox {

// Quest Touch's capacitive controls observe index and thumb contact, while
// analogue trigger and squeeze approximate the remaining finger closure.
// This does NOT claim full five-finger optical tracking.
struct OpenHandContacts {
    bool valid{}; // Every required capacitive action must be active.
    bool indexTouch{}, thumbstickTouch{}, thumbrestTouch{};
    bool faceLowerTouch{}, faceUpperTouch{};
    float trigger{}, grip{};
};

struct OpenHandReleaseState {
    std::uint64_t openSinceNanoseconds{};
    std::uint64_t cooldownUntilNanoseconds{};
    bool armed{true};
};

inline bool handFullyReleased(const OpenHandContacts& c) {
    return c.valid && !c.indexTouch && !c.thumbstickTouch
        && !c.thumbrestTouch && !c.faceLowerTouch && !c.faceUpperTouch
        && c.trigger<0.10f && c.grip<0.10f;
}

// Open all monitored contacts continuously for 450 ms to unequip once.
// Invalid sensor data must NEVER be interpreted as an open hand.
inline bool updateOpenHandRelease(OpenHandReleaseState& state,
    const OpenHandContacts& contacts, bool eligible,
    std::uint64_t nowNanoseconds) {
    if(!eligible || !contacts.valid){
        state.openSinceNanoseconds=0;
        return false;
    }
    if(!handFullyReleased(contacts)){
        state.openSinceNanoseconds=0;
        state.armed=true;
        return false;
    }
    if(!state.armed || nowNanoseconds<state.cooldownUntilNanoseconds)
        return false;
    if(!state.openSinceNanoseconds){
        state.openSinceNanoseconds=nowNanoseconds ? nowNanoseconds : 1;
        return false;
    }
    if(nowNanoseconds-state.openSinceNanoseconds<450000000ull)
        return false;
    state.openSinceNanoseconds=0;
    state.armed=false;
    return true;
}

inline void delayOpenHandRelease(OpenHandReleaseState& state,
    std::uint64_t nowNanoseconds) {
    state.openSinceNanoseconds=0;
    state.cooldownUntilNanoseconds=nowNanoseconds+650000000ull;
}
}
