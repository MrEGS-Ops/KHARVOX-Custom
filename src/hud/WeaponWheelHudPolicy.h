#pragma once
#include <cstdint>
#include <cmath>

namespace kharvox {
// Steam DOOMx64vk RTTI: idMenuManager_WeaponSelect and its Framing manager.
inline bool ownedWeaponWheel(uintptr_t vtable) {
    return vtable==0x22456f8 || vtable==0x2245608;
}
// Original approved weapon-wheel presentation size and distance.
// Position drift must be fixed by resetting the transform lifecycle, NOT
// by resizing or shifting its visible appearance.
inline constexpr float weaponWheelDistanceMeters=3.0f;
inline constexpr float weaponWheelWidthMeters=3.0f;

inline bool weaponWheelPose(const float* eye,const float* head,float units,float* center) {
    if(!eye||!head||!center||!std::isfinite(units)||units<=0)return false;
    for(int i=0;i<3;++i){
        if(!std::isfinite(eye[i])||!std::isfinite(head[i]))return false;
        center[i]=eye[i]+head[i]*weaponWheelDistanceMeters*units;
        if(!std::isfinite(center[i]))return false;
    }
    return true;
}

// The wheel should always face the HMD, with horizontal / vertical axes
// locked to the headset. The old "closest native axis" heuristic could assign
// two different native rows to one HMD basis direction after a turn, then
// reject the transformation and briefly show DOOM's angled/distant plane.
// Native per-row lengths are retained so its surface scale stays stable.
inline bool flatWeaponWheelAxis(const float* native,const float* head,float* out) {
    if(!native||!head||!out)return false;
    constexpr int basisRow[3]{1,2,0}; // DOOM camera: forward, right, up
    // The native wheel texture was rolled 180 degrees. Flip both in-plane
    // axes together, retaining its plane normal and headset-facing direction.
    // This is a 180-degree roll, NOT a mirror or a turn away from the player.
    constexpr float inPlaneSign[3]{-1.f,-1.f,1.f};
    for(int row=0;row<3;++row){
        float squared=0.f,headSquared=0.f;
        for(int i=0;i<3;++i){
            const float n=native[row*3+i];
            const float h=head[basisRow[row]*3+i];
            if(!std::isfinite(n)||!std::isfinite(h))return false;
            squared+=n*n;headSquared+=h*h;
        }
        if(squared<1.e-10f||headSquared<1.e-10f)return false;
        const float length=std::sqrt(squared/headSquared);
        for(int i=0;i<3;++i)
            out[row*3+i]=head[basisRow[row]*3+i]*length*inPlaneSign[row];
    }
    return true;
}
}
