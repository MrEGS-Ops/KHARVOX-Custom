#pragma once
#include "../openxr/ObjectGripProfileStore.h"
#include <array>
#include <algorithm>
#include <cmath>

namespace kharvox {
// All DOOM native axes are column-major (axis*3 + world coordinate).
// Held props must use a fixed hand-local orientation while the player
// moves/tilts/turns. Grip-held editing changes ONLY the additional local
// spin about the model's Z, controlled by right-stick X. Relative pitch,
// roll AND yaw remain fixed to the tracked physical controller, rather than
// recalculating world-up against the physics body's previous orientation.
inline std::array<float,4> objectGripQuatMultiply(
    const std::array<float,4>& a,const std::array<float,4>& b) noexcept {
    return {
        a[3]*b[0]+a[0]*b[3]+a[1]*b[2]-a[2]*b[1],
        a[3]*b[1]-a[0]*b[2]+a[1]*b[3]+a[2]*b[0],
        a[3]*b[2]+a[0]*b[1]-a[1]*b[0]+a[2]*b[3],
        a[3]*b[3]-a[0]*b[0]-a[1]*b[1]-a[2]*b[2]
    };
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
// Compute the final LOCAL orientation from an immutable baseline captured
// on grip press and a continuous joystick spin. The object world transform
// is always nativeHandBasis * this local transform; tracking the hand never
// adds a second relative rotation or feeds the DOOM physics pose back in.
inline std::array<float,9> objectGripSpinLocalAxis(
    const float baseLocalAxis[9],float spinDegrees) noexcept {
    if(!std::isfinite(spinDegrees))
        return {baseLocalAxis[0],baseLocalAxis[1],baseLocalAxis[2],
            baseLocalAxis[3],baseLocalAxis[4],baseLocalAxis[5],
            baseLocalAxis[6],baseLocalAxis[7],baseLocalAxis[8]};
    constexpr float radiansPerDegree=.017453292519943295769f;
    const float halfAngle=.5f*spinDegrees*radiansPerDegree;
    const std::array<float,4> localSpin{
        0.f,0.f,std::sin(halfAngle),std::cos(halfAngle)};
    const auto base=objectGripAxisToQuaternion(baseLocalAxis);
    const auto rotated=objectGripQuatNormalize(
        objectGripQuatMultiply(base,localSpin));
    return objectGripQuaternionToAxis(rotated);
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
// Neutral pickup is a single, invariant SIX-DEGREE-OF-FREEDOM pose in
// the PHYSICAL FREE HAND'S local coordinate system. It must not be computed
// from the barrel's old physics orientation or from the player's world-up
// direction: otherwise the same grab gesture gives a different result as
// the hand moves/turns. The authored model's local Z (usual barrel axis)
// maps onto the controller's local Y (up while aiming like a gun).
//
// Native column-major basis: object X -> hand X, object Y -> -hand Z,
// object Z -> hand Y. This is a fixed -90 degree rotation around hand X.
// A mesh authored with a different intrinsic vertical still needs manual
// calibration, but its starting relative orientation is deterministic.
inline constexpr std::array<float,9> kObjectGripNeutralLocalAxis{
    1.f,0.f,0.f,
    0.f,0.f,-1.f,
    0.f,1.f,0.f};
inline constexpr std::array<float,9> objectGripNeutralLocalAxis() noexcept {
    return kObjectGripNeutralLocalAxis;
}
// Position in the SAME physical free-hand coordinate frame:
// centred horizontally, 4 native units below grip and 20 ahead.
// The native rigid-body follow cap bounds the PHYSICAL transition to
// this attachment target without leaving the target at the old grab pose.
inline constexpr std::array<float,3> kObjectGripNeutralOffsetUnits{
    0.f,-4.f,-20.f};

inline float objectGripSpinAxis(float rightStickX) noexcept {
    if(!std::isfinite(rightStickX)||std::abs(rightStickX)<.28f)return 0.f;
    return std::clamp(rightStickX,-1.f,1.f);
}
} // namespace kharvox
