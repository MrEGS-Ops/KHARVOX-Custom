#pragma once

#include <cmath>

namespace kharvox::hands {

// Quest/OpenXR supplies independent grip and aim action poses. A glove's
// grip-space orientation is not necessarily aimed along the controller ray.
// Left fist visuals must use the aim-axis correction without moving their
// tracked grip POSITION or changing calibrated weapon-holding gloves.
struct HandAimQuaternion {
    float x{},y{},z{},w{1.f};
};

inline bool validHandAimQuaternion(HandAimQuaternion q) noexcept {
    const auto n=q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w;
    return std::isfinite(n)&&n>.01f&&n<4.f;
}

inline HandAimQuaternion multiplyHandAimQuaternion(
    HandAimQuaternion a,HandAimQuaternion b) noexcept {
    return {a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y,
            a.w*b.y-a.x*b.z+a.y*b.w+a.z*b.x,
            a.w*b.z+a.x*b.y-a.y*b.x+a.z*b.w,
            a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z};
}

// Use a controller-local offset, not a live world-space correction. This
// allows an older native/AER frame to use the SAME grip->aim relationship
// without mixing a newly predicted aim pose into its completed eye pair.
inline bool alignFreeHandGripToAim(HandAimQuaternion recordedGrip,
    HandAimQuaternion referenceGrip,HandAimQuaternion referenceAim,
    HandAimQuaternion& aligned) noexcept {
    if(!validHandAimQuaternion(recordedGrip)
        ||!validHandAimQuaternion(referenceGrip)
        ||!validHandAimQuaternion(referenceAim))return false;
    const auto normal=[](HandAimQuaternion q){
        const float inverse=1.f/std::sqrt(q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w);
        return HandAimQuaternion{q.x*inverse,q.y*inverse,q.z*inverse,q.w*inverse};
    };
    const auto grip=normal(referenceGrip);
    const auto aim=normal(referenceAim);
    const auto old=normal(recordedGrip);
    const auto local=multiplyHandAimQuaternion(
        {-grip.x,-grip.y,-grip.z,grip.w},aim);
    aligned=normal(multiplyHandAimQuaternion(old,local));
    return true;
}

} // namespace kharvox::hands
