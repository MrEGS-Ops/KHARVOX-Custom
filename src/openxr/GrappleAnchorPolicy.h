#pragma once
// RIPSHIFT experimental laser grappling. This version uses the controller ray
// and the target rigid body's origin to estimate a virtual hit point. It DOES
// NOT claim DOOM's true collision/mesh impact location.
#include "../camera/ObjectGripRotationPolicy.h"
#include <array>
#include <cmath>
#include <fstream>
#include <string>
namespace kharvox {
struct GrappleAnchorSettings { bool enabled{}; };
inline bool grappleAnchorSave(const std::string& path,GrappleAnchorSettings s) {
    std::ofstream f(path,std::ios::trunc);
    if(!f)return false;
    f<<"grapple_anchor_v1\n"<<"enabled="<<s.enabled<<"\n";
    f.flush();return f.good();
}
inline bool grappleAnchorLoad(const std::string& path,GrappleAnchorSettings& s){
    std::ifstream f(path);if(!f)return false;
    std::string version,line,extra;
    if(!std::getline(f,version)||version!="grapple_anchor_v1"
       ||!std::getline(f,line)||std::getline(f,extra))return false;
    if(line=="enabled=0"){s.enabled=false;return true;}
    if(line=="enabled=1"){s.enabled=true;return true;}
    return false;
}
struct GrappleRayAnchor {
    std::array<float,3> worldPoint{};
    float rayDistance{},missDistance{};
    // True collision impact is NOT exposed by the existing FocusTracker hook.
    bool exactCollisionHit{}; // stays false until an independently verified hook exists
};
inline bool grappleRayEstimate(
    const float rayOrigin[3],const float rayDirection[3],
    const float bodyOrigin[3],float reachUnits,float maxMissUnits,
    GrappleRayAnchor& result) noexcept {
    result={};
    if(!rayOrigin||!rayDirection||!bodyOrigin
        ||!std::isfinite(reachUnits)||!std::isfinite(maxMissUnits)
        ||reachUnits<=0.f||maxMissUnits<=0.f)return false;
    float unit[3]{},norm2{},forward{};
    for(int i=0;i<3;++i){
        if(!std::isfinite(rayOrigin[i])||!std::isfinite(rayDirection[i])
            ||!std::isfinite(bodyOrigin[i]))return false;
        norm2+=rayDirection[i]*rayDirection[i];
    }
    if(!std::isfinite(norm2)||norm2<.25f||norm2>4.f)return false;
    const float invNorm=1.f/std::sqrt(norm2);
    for(int i=0;i<3;++i){
        unit[i]=rayDirection[i]*invNorm;
        forward+=(bodyOrigin[i]-rayOrigin[i])*unit[i];
    }
    if(!std::isfinite(forward)||forward<=2.f
        ||forward>reachUnits)return false;
    float miss2{};
    for(int i=0;i<3;++i){
        const float perpendicular=
            bodyOrigin[i]-rayOrigin[i]-unit[i]*forward;
        miss2+=perpendicular*perpendicular;
    }
    if(!std::isfinite(miss2)||miss2>maxMissUnits*maxMissUnits)
        return false;
    // Unverified proxy: stop ~20 native units short of the centre-depth
    // plane. A ray aimed at a corner retains its own lateral offset.
    // No mesh/bounds or idClipModel contact is inferred here.
    const float depth=std::min(20.f,forward-1.f);
    const float pointDistance=forward-depth;
    for(int i=0;i<3;++i)
        result.worldPoint[i]=rayOrigin[i]+unit[i]*pointDistance;
    result.rayDistance=pointDistance;
    result.missDistance=std::sqrt(miss2);
    return true;
}
inline bool grapplePickupPose(
    const float handOrigin[3],const float handAxis[9],
    const float rayOrigin[3],const float aim[3],const float bodyOrigin[3],
    const float bodyAxis[9],float reachUnits,
    std::array<float,3>& localOffset,
    std::array<float,9>& localAxis,GrappleRayAnchor& anchor) noexcept {
    if(!handOrigin||!handAxis||!bodyAxis
        ||!grappleRayEstimate(rayOrigin,aim,bodyOrigin,reachUnits,
             55.f,anchor))return false;
    // Preserve the object's initial rotation, and move the laser's anchor
    // towards the hand. Store the resulting six-DOF pose in hand space.
    float snappedOrigin[3]{};
    for(int i=0;i<3;++i)
        snappedOrigin[i]=bodyOrigin[i]+handOrigin[i]-anchor.worldPoint[i];
    float offset[3]{},orientation[9]{};
    objectGripToHandLocalPose(handOrigin,handAxis,
        snappedOrigin,bodyAxis,offset,orientation);
    for(int i=0;i<3;++i){
        if(!std::isfinite(offset[i]))return false;
        localOffset[i]=offset[i];
    }
    for(int i=0;i<9;++i){
        if(!std::isfinite(orientation[i]))return false;
        localAxis[i]=orientation[i];
    }
    return true;
}
} // namespace kharvox
