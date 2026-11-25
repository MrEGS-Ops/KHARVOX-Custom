#pragma once
#include "../openxr/ObjectGripProfileStore.h"
#include <array>
#include <algorithm>
#include <cmath>

namespace kharvox {
// All DOOM native axes are column-major (axis*3 + world coordinate).
// Rotation calibration is a two-input tool:
//   - physical GRIP: relative hand pitch/roll only, no yaw/twist;
//   - right-stick X: explicit smooth spin about the model's local upright Z.
// Take immutable references when GRIP is first pressed. Never read the
// current physics body back into the orientation target every frame.
inline std::array<float,4> objectGripQuatMultiply(
    const std::array<float,4>& a,const std::array<float,4>& b) noexcept {
    return {
        a[3]*b[0]+a[0]*b[3]+a[1]*b[2]-a[2]*b[1],
        a[3]*b[1]-a[0]*b[2]+a[1]*b[3]+a[2]*b[0],
        a[3]*b[2]+a[0]*b[1]-a[1]*b[0]+a[2]*b[3],
        a[3]*b[3]-a[0]*b[0]-a[1]*b[1]-a[2]*b[2]
    };
}
inline std::array<float,4> objectGripQuatConjugate(
    const std::array<float,4>& a) noexcept {
    return {-a[0],-a[1],-a[2],a[3]};
}
inline std::array<float,4> objectGripQuatNormalize(
    std::array<float,4> q) noexcept {
    float lengthSquared{};
    for(float value:q)lengthSquared+=value*value;
    if(!std::isfinite(lengthSquared)||lengthSquared<1e-9f)
        return {0.f,0.f,0.f,1.f};
    const float inverse=1.f/std::sqrt(lengthSquared);
    for(float& value:q)value*=inverse;
    return q;
}
inline std::array<float,4> objectGripTiltOnly(
    const float currentHand[9],const float initialHand[9]) noexcept {
    const auto current=objectGripAxisToQuaternion(currentHand);
    const auto initial=objectGripAxisToQuaternion(initialHand);
    const auto delta=objectGripQuatNormalize(objectGripQuatMultiply(
        current,objectGripQuatConjugate(initial)));
    // Decompose change into swing (pitch/roll) and world-up twist (yaw).
    // Remove twist; rotating a physical controller horizontally must not
    // make a bottle tumble or fight the explicit stick spin.
    const auto yaw=objectGripQuatNormalize(
        {0.f,0.f,delta[2],delta[3]});
    return objectGripQuatNormalize(objectGripQuatMultiply(
        delta,objectGripQuatConjugate(yaw)));
}
inline std::array<float,9> objectGripTiltAndSpinAxis(
    const float currentHand[9],const float initialHand[9],
    const float initialObject[9],float spinDegrees) noexcept {
    constexpr float radiansPerDegree=.017453292519943295769f;
    const float halfAngle=.5f*spinDegrees*radiansPerDegree;
    const std::array<float,4> localSpin{
        0.f,0.f,std::sin(halfAngle),std::cos(halfAngle)};
    const auto base=objectGripAxisToQuaternion(initialObject);
    const auto swing=objectGripTiltOnly(currentHand,initialHand);
    const auto desired=objectGripQuatNormalize(objectGripQuatMultiply(
        objectGripQuatMultiply(swing,base),localSpin));
    return objectGripQuaternionToAxis(desired);
}
inline std::array<float,9> objectGripToHandLocalAxis(
    const float hand[9],const float world[9]) noexcept {
    std::array<float,9> local{};
    for(int col=0;col<3;++col)
        for(int axis=0;axis<3;++axis)
            for(int component=0;component<3;++component)
                local[col*3+axis]+=
                    hand[axis*3+component]*world[col*3+component];
    return local;
}
// A newly grabbed, unconfigured rigid body should start from its authored
// model orientation (native world identity, local Z up), not the random angle
// it happened to land at. This uses no model heuristics or physics wakeups.
// Model-authored meshes whose canonical upright is not local +Z still need
// one deliberate user calibration; saved per-hand profiles remain authoritative.
inline std::array<float,9> objectGripNeutralLocalAxis(
    const float handAxis[9]) noexcept {
    constexpr float authoredUpright[9]{
        1.f,0.f,0.f,0.f,1.f,0.f,0.f,0.f,1.f};
    return objectGripToHandLocalAxis(handAxis,authoredUpright);
}
// DOOM native units: physically modest palm-forward starting point. The
// existing 28 native-unit/tick follow cap avoids giant one-frame teleports.
// Calibration nudges remain independent and measured in metres.
inline constexpr std::array<float,3> kObjectGripNeutralOffsetUnits{
    0.f,-4.f,-20.f};

inline float objectGripSpinAxis(float rightStickX) noexcept {
    if(!std::isfinite(rightStickX)||std::abs(rightStickX)<.28f)return 0.f;
    return std::clamp(rightStickX,-1.f,1.f);
}
} // namespace kharvox
