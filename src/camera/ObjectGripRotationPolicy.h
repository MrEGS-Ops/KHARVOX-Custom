#pragma once
#include "../openxr/ObjectGripProfileStore.h"
#include <array>
#include <algorithm>
#include <cmath>

namespace kharvox {
// DOOM native rigid body and OpenXR hand axes use column-major 3x3
// matrices (column*3 + world coordinate). This is the only frame transform
// used both for neutral pickup and for grip-locked editing.
//
// NORMAL HOLD: world pose = hand pose * stored local pose.
// ROTATION EDIT: a press of offhand GRIP temporarily fixes the object's
// entire world pose; each XR hand update recomputes only its LOCAL pose,
// including its local offset. Release GRIP commits that new LOCAL pose
// without moving the object. Never read the physics body pose as the lock
// reference, because it can be a frame behind the visual target.
inline void objectGripToWorldPose(
    const float handOrigin[3],const float handAxis[9],
    const float localOffset[3],const float localAxis[9],
    float outOrigin[3],float outAxis[9]) noexcept {
    for(int world=0;world<3;++world){
        outOrigin[world]=handOrigin[world];
        for(int local=0;local<3;++local)
            outOrigin[world]+=handAxis[local*3+world]
                *localOffset[local];
        for(int column=0;column<3;++column){
            float value{};
            for(int local=0;local<3;++local)
                value+=handAxis[local*3+world]
                    *localAxis[column*3+local];
            outAxis[column*3+world]=value;
        }
    }
}
inline void objectGripToHandLocalPose(
    const float handOrigin[3],const float handAxis[9],
    const float worldOrigin[3],const float worldAxis[9],
    float outOffset[3],float outAxis[9]) noexcept {
    for(int local=0;local<3;++local){
        outOffset[local]=0.f;
        for(int world=0;world<3;++world)
            outOffset[local]+=handAxis[local*3+world]
                *(worldOrigin[world]-handOrigin[world]);
        for(int column=0;column<3;++column){
            float value{};
            for(int world=0;world<3;++world)
                value+=handAxis[local*3+world]
                    *worldAxis[column*3+world];
            outAxis[column*3+local]=value;
        }
    }
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

} // namespace kharvox
