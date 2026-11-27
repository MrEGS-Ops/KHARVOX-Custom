#pragma once
// RIPSHIFT universal grapple contact policy. Any verified render triangles
// use exact first-hit intersection. Until DOOM render triangles are exposed,
// preserve the original ray proxy and experimental, explicitly unverified
// model-local shape approximations.
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
    if(!std::getline(f,version)
        ||(version!="grapple_anchor_v1"&&version!="grapple_anchor_v2")
        ||!std::getline(f,line))return false;
    if(line=="enabled=0")s.enabled=false;
    else if(line=="enabled=1")s.enabled=true;
    else return false;
    // Backward compatibility: ignore the retired, barrel-only preview flag
    // from existing v2 user configs. Never activate legacy shape assumptions.
    if(version=="grapple_anchor_v2"){
        if(!std::getline(f,line)
            ||(line!="barrel_surface_preview=0"
                &&line!="barrel_surface_preview=1"))return false;
    }
    return !std::getline(f,extra);
}
inline constexpr float kGrappleProxyMissUnits=25.f;
enum class GrappleContactSource { RayOriginProxy, VerifiedRenderTriangles, CandidateRenderBounds, CandidateCylinderSide };
struct GrappleRayAnchor {
    std::array<float,3> worldPoint{};
    float rayDistance{},missDistance{};
    GrappleContactSource source{GrappleContactSource::RayOriginProxy};
    // A true triangle hit requires decoded, asset-verified RENDER geometry.
    // Native collision meshes and heuristic shape dimensions cannot set this.
    bool exactCollisionHit{};
    bool exactVisualMeshHit{};
};
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
// Ray versus oriented model-local bounds. Candidate geometry, not a mesh or LOS query.
inline bool grappleSurfaceBoundsHit(const float rayOrigin[3],const float rayDir[3],
    const float modelOrigin[3],const float modelAxis[9],const float bounds[6],
    float reach,GrappleRayAnchor& out) noexcept {
    out={};
    if(!rayOrigin||!rayDir||!modelOrigin||!modelAxis||!bounds||!std::isfinite(reach)||reach<=0.f)return false;
    float n2{};
    for(int i=0;i<3;++i){
        if(!std::isfinite(rayOrigin[i])||!std::isfinite(rayDir[i])||!std::isfinite(modelOrigin[i])
            ||!std::isfinite(bounds[i])||!std::isfinite(bounds[i+3])
            ||bounds[i+3]-bounds[i]<.1f||bounds[i+3]-bounds[i]>300.f
            ||std::abs(bounds[i])>300.f||std::abs(bounds[i+3])>300.f)return false;
        n2+=rayDir[i]*rayDir[i];
        for(int j=0;j<3;++j)if(!std::isfinite(modelAxis[3*i+j]))return false;
        for(int j=0;j<=i;++j){
            float dot{};for(int k=0;k<3;++k)dot+=modelAxis[3*i+k]*modelAxis[3*j+k];
            if(std::abs(dot-(i==j?1.f:0.f))>.025f)return false;
        }
    }
    if(n2<.25f||n2>4.f)return false;
    const float inv=1.f/std::sqrt(n2);
    float lo=0.f,hi=reach;
    for(int i=0;i<3;++i){
        float o{},d{};
        for(int j=0;j<3;++j){
            o+=(rayOrigin[j]-modelOrigin[j])*modelAxis[3*i+j];
            d+=rayDir[j]*inv*modelAxis[3*i+j];
        }
        if(std::abs(d)<1e-6f){if(o<bounds[i]||o>bounds[i+3])return false;continue;}
        float a=(bounds[i]-o)/d,b=(bounds[i+3]-o)/d;
        if(a>b)std::swap(a,b);
        lo=std::max(lo,a);hi=std::min(hi,b);
        if(lo>hi)return false;
    }
    // Do not grab from inside the approximation or behind the controller.
    if(lo<=.001f||lo>reach)return false;
    for(int i=0;i<3;++i)out.worldPoint[i]=rayOrigin[i]+rayDir[i]*inv*lo;
    out.rayDistance=lo;out.source=GrappleContactSource::CandidateRenderBounds;
    return true;
}
// Refine a validated bounds hit on a known long cylinder's straight side.
// End caps, handles and fittings remain on the existing bounds fallback.
// This is an analytic estimate, NOT a render/collision mesh intersection.
inline bool grappleRefineCylinderSide(const float rayOrigin[3],const float rayDir[3],
    const float modelOrigin[3],const float modelAxis[9],const float bounds[6],
    float reach,GrappleRayAnchor& out) noexcept {
    if(!rayOrigin||!rayDir||!modelOrigin||!modelAxis||!bounds
        ||out.source!=GrappleContactSource::CandidateRenderBounds
        ||!std::isfinite(out.rayDistance)||!std::isfinite(reach)||reach<=0.f)return false;
    const float r=(bounds[4]-bounds[1])*.5f;
    const float cx=bounds[0]+r,cy=(bounds[1]+bounds[4])*.5f;
    const float z0=bounds[2]+r,z1=bounds[5]-2.f*r;
    // Require a long body with approximately centred circular section.
    // The extra positive-X extent may be a valve/fitting, so do not use it
    // as the diameter of the central cylinder.
    if(!std::isfinite(r)||r<.1f||!std::isfinite(cx)||!std::isfinite(cy)
        ||std::abs(cx)>.25f*r||std::abs(cy)>.25f*r
        ||!std::isfinite(z0)||!std::isfinite(z1)||z1<=z0
        ||bounds[3]<cx+r-.1f)return false;
    float n2{},o[3]{},d[3]{},boxZ{};
    for(int i=0;i<3;++i)n2+=rayDir[i]*rayDir[i];
    if(!std::isfinite(n2)||n2<.25f||n2>4.f)return false;
    const float inv=1.f/std::sqrt(n2);
    for(int i=0;i<3;++i){
        for(int j=0;j<3;++j){
            o[i]+=(rayOrigin[j]-modelOrigin[j])*modelAxis[3*i+j];
            d[i]+=rayDir[j]*inv*modelAxis[3*i+j];
        }
    }
    boxZ=o[2]+d[2]*out.rayDistance;
    if(!std::isfinite(boxZ)||boxZ<z0||boxZ>z1)return false;
    const float x=o[0]-cx,y=o[1]-cy;
    const float a=d[0]*d[0]+d[1]*d[1];
    const float b=x*d[0]+y*d[1],c=x*x+y*y-r*r;
    if(!std::isfinite(a)||a<1e-6f||!std::isfinite(c)||c<=0.f)return false;
    const float discriminant=b*b-a*c;
    if(!std::isfinite(discriminant)||discriminant<0.f)return false;
    const float t=(-b-std::sqrt(discriminant))/a,z=o[2]+d[2]*t;
    if(!std::isfinite(t)||t<=.001f||t>reach||t+0.05f<out.rayDistance
        ||!std::isfinite(z)||z<z0||z>z1)return false;
    for(int i=0;i<3;++i)out.worldPoint[i]=rayOrigin[i]+rayDir[i]*inv*t;
    out.rayDistance=t;out.source=GrappleContactSource::CandidateCylinderSide;
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
             kGrappleProxyMissUnits,anchor))return false;
    return grapplePickupPoseFromHit(handOrigin,handAxis,
        bodyOrigin,bodyAxis,anchor,localOffset,localAxis);
}
} // namespace kharvox
