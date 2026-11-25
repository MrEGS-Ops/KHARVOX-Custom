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
struct GrappleAnchorSettings { bool enabled{},barrelSurfacePreview{}; };
inline bool grappleAnchorSave(const std::string& path,GrappleAnchorSettings s) {
    std::ofstream f(path,std::ios::trunc);
    if(!f)return false;
    f<<"grapple_anchor_v2\n"<<"enabled="<<s.enabled<<"\n"
     <<"barrel_surface_preview="<<s.barrelSurfacePreview<<"\n";
    f.flush();return f.good();
}
inline bool grappleAnchorLoad(const std::string& path,GrappleAnchorSettings& s){
    std::ifstream f(path);if(!f)return false;
    std::string version,line,extra;
    if(!std::getline(f,version)
        ||(version!="grapple_anchor_v1"&&version!="grapple_anchor_v2")
        ||!std::getline(f,line))return false;
    if(line=="enabled=0")s.enabled=false;
    else if(line=="enabled=1")s.enabled=true;
    else return false;
    s.barrelSurfacePreview=false;
    if(version=="grapple_anchor_v2"){
        if(!std::getline(f,line))return false;
        if(line=="barrel_surface_preview=0")s.barrelSurfacePreview=false;
        else if(line=="barrel_surface_preview=1")s.barrelSurfacePreview=true;
        else return false;
    }
    return !std::getline(f,extra);
}
enum class GrappleContactSource { RayOriginProxy, ApproximateBarrel, VerifiedRenderTriangles };
struct GrappleRayAnchor {
    std::array<float,3> worldPoint{};
    float rayDistance{},missDistance{};
    GrappleContactSource source{GrappleContactSource::RayOriginProxy};
    // A true triangle hit requires decoded, asset-verified RENDER geometry.
    // Native collision meshes and heuristic shape dimensions cannot set this.
    bool exactCollisionHit{};
    bool exactVisualMeshHit{};
};
inline bool grappleIdentifiedTechBarrel(const char* modelKey) noexcept {
    if(!modelKey)return false;
    std::string text(modelKey);
    if(text.size()>192)return false;
    const auto slash=text.find_last_of("/\\");
    text=text.substr(slash==std::string::npos?0:slash+1);
    const auto dot=text.find_last_of('.');
    if(dot!=std::string::npos)text.resize(dot);
    for(char& c:text){
        if(c>='A'&&c<='Z')c=static_cast<char>(c-'A'+'a');
        if(c=='-'||c==' ')c='_';
    }
    return text=="uac_barrel_tech_red";
}
// Model-specific visual SILHOUETTE approximation, not mesh data:
// radius/half-height are experimental and are NEVER described as measured.
// This is isolated behind a separate OFF-by-default toggle.
inline bool grappleBarrelOutlineHit(
    const float rayOrigin[3],const float rayDirection[3],
    const float bodyOrigin[3],const float bodyAxis[9],
    float reachUnits,GrappleRayAnchor& out) noexcept {
    out={};
    if(!rayOrigin||!rayDirection||!bodyOrigin||!bodyAxis
        ||!std::isfinite(reachUnits)||reachUnits<1.f)return false;
    float norm2{};
    for(int i=0;i<3;++i){
        if(!std::isfinite(rayOrigin[i])||!std::isfinite(rayDirection[i])
            ||!std::isfinite(bodyOrigin[i]))return false;
        norm2+=rayDirection[i]*rayDirection[i];
    }
    for(int i=0;i<9;++i)if(!std::isfinite(bodyAxis[i]))return false;
    if(!std::isfinite(norm2)||norm2<.25f||norm2>4.f)return false;
    float origin[3]{},dir[3]{};
    const float inv=1.f/std::sqrt(norm2);
    for(int local=0;local<3;++local){
        for(int world=0;world<3;++world){
            origin[local]+=bodyAxis[local*3+world]
                *(rayOrigin[world]-bodyOrigin[world]);
            dir[local]+=bodyAxis[local*3+world]*rayDirection[world]*inv;
        }
    }
    // Never silently substitute the physics collision radius. These are
    // provisional visible-barrel dimensions in native units, NOT a mesh.
    constexpr float radius=26.f,halfHeight=37.f;
    float t=reachUnits+1.f;
    const auto consider=[&](float proposed,float localZ,float radial2){
        if(std::isfinite(proposed)&&proposed>.01f&&proposed<t
            &&proposed<=reachUnits&&std::abs(localZ)<=halfHeight+.01f
            &&radial2<=radius*radius+.05f)t=proposed;
    };
    const float a=dir[0]*dir[0]+dir[1]*dir[1];
    if(a>1e-8f){
        const float bb=2.f*(origin[0]*dir[0]+origin[1]*dir[1]);
        const float c=origin[0]*origin[0]+origin[1]*origin[1]
            -radius*radius;
        const float disc=bb*bb-4.f*a*c;
        if(disc>=0.f&&std::isfinite(disc)){
            const float root=std::sqrt(disc);
            for(float v:{(-bb-root)/(2.f*a),(-bb+root)/(2.f*a)})
                consider(v,origin[2]+v*dir[2],
                    radius*radius);
        }
    }
    if(std::abs(dir[2])>1e-6f){
        for(float height:{-halfHeight,halfHeight}){
            const float v=(height-origin[2])/dir[2];
            const float x=origin[0]+v*dir[0];
            const float y=origin[1]+v*dir[1];
            consider(v,height,x*x+y*y);
        }
    }
    if(t>reachUnits)return false;
    for(int i=0;i<3;++i)
        out.worldPoint[i]=rayOrigin[i]+rayDirection[i]*inv*t;
    out.rayDistance=t;
    out.source=GrappleContactSource::ApproximateBarrel;
    return true;
}
// Moller-Trumbore against real static-render triangles (not physics).
// A verified renderer vertex provider can supply this without changing
// grip pose mathematics. This helper does not pretend that the provider
// has already been found in DOOM's native executable.
inline bool grappleRenderTriangleHit(
    const float rayOrigin[3],const float rayDirection[3],
    const float* triangleVertices,std::size_t triangleCount,
    float reachUnits,GrappleRayAnchor& out) noexcept {
    out={};
    if(!rayOrigin||!rayDirection||!triangleVertices
        ||triangleCount==0||triangleCount>100000
        ||!std::isfinite(reachUnits)||reachUnits<=0.f)return false;
    float norm2{};
    for(int i=0;i<3;++i){
        if(!std::isfinite(rayOrigin[i])||!std::isfinite(rayDirection[i]))
            return false;
        norm2+=rayDirection[i]*rayDirection[i];
    }
    if(!std::isfinite(norm2)||norm2<.25f||norm2>4.f)return false;
    float direction[3]{};
    for(int i=0;i<3;++i)direction[i]=rayDirection[i]/std::sqrt(norm2);
    float best=reachUnits+1.f;
    for(std::size_t ti=0;ti<triangleCount;++ti){
        const auto* v=triangleVertices+ti*9;
        bool valid=true;
        for(int j=0;j<9;++j)if(!std::isfinite(v[j]))valid=false;
        if(!valid)continue;
        float a[3]{},bb[3]{},cross[3]{},tvec[3]{},q[3]{};
        for(int j=0;j<3;++j){
            a[j]=v[3+j]-v[j];bb[j]=v[6+j]-v[j];
            tvec[j]=rayOrigin[j]-v[j];
        }
        cross[0]=direction[1]*bb[2]-direction[2]*bb[1];
        cross[1]=direction[2]*bb[0]-direction[0]*bb[2];
        cross[2]=direction[0]*bb[1]-direction[1]*bb[0];
        const float det=a[0]*cross[0]+a[1]*cross[1]+a[2]*cross[2];
        if(std::abs(det)<1e-6f)continue;
        const float invDet=1.f/det;
        const float u=(tvec[0]*cross[0]+tvec[1]*cross[1]
            +tvec[2]*cross[2])*invDet;
        if(u<0.f||u>1.f)continue;
        q[0]=tvec[1]*a[2]-tvec[2]*a[1];
        q[1]=tvec[2]*a[0]-tvec[0]*a[2];
        q[2]=tvec[0]*a[1]-tvec[1]*a[0];
        const float vBary=(direction[0]*q[0]+direction[1]*q[1]
            +direction[2]*q[2])*invDet;
        if(vBary<0.f||u+vBary>1.f)continue;
        const float t=(bb[0]*q[0]+bb[1]*q[1]+bb[2]*q[2])
            *invDet;
        if(std::isfinite(t)&&t>.001f&&t<best)best=t;
    }
    if(best>reachUnits)return false;
    for(int i=0;i<3;++i)out.worldPoint[i]=rayOrigin[i]
        +direction[i]*best;
    out.rayDistance=best;
    out.source=GrappleContactSource::VerifiedRenderTriangles;
    out.exactVisualMeshHit=true;
    return true;
}
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
inline bool grapplePickupPoseFromHit(
    const float handOrigin[3],const float handAxis[9],
    const float bodyOrigin[3],const float bodyAxis[9],
    const GrappleRayAnchor& anchor,
    std::array<float,3>& localOffset,
    std::array<float,9>& localAxis) noexcept {
    if(!handOrigin||!handAxis||!bodyOrigin||!bodyAxis)return false;
    for(float v:anchor.worldPoint)if(!std::isfinite(v))return false;
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
inline bool grapplePickupPose(
    const float handOrigin[3],const float handAxis[9],
    const float rayOrigin[3],const float aim[3],const float bodyOrigin[3],
    const float bodyAxis[9],float reachUnits,
    std::array<float,3>& localOffset,
    std::array<float,9>& localAxis,GrappleRayAnchor& anchor) noexcept {
    if(!handOrigin||!handAxis||!bodyAxis
        ||!grappleRayEstimate(rayOrigin,aim,bodyOrigin,reachUnits,
             55.f,anchor))return false;
    return grapplePickupPoseFromHit(handOrigin,handAxis,
        bodyOrigin,bodyAxis,anchor,localOffset,localAxis);
}
} // namespace kharvox
