#include "../src/openxr/ObjectDiscoverySettingsPolicy.h"
#include "../src/openxr/GrappleAnchorPolicy.h"
#include <cassert>
#include <limits>
#include <cstdio>
int main(){
    using namespace kharvox;
    static_assert(kObjectDiscoveryGrabReachM==2.f);
    const auto defaults=sanitizedObjectDiscoverySettings({});
    assert(!defaults.enabled&&!defaults.grabReachLaserEnabled&&defaults.scanRadiusMetres==5.f);
    assert(sanitizedObjectDiscoverySettings({true,-50.f,false}).scanRadiusMetres==1.f);
    assert(sanitizedObjectDiscoverySettings({true,150.f,false}).scanRadiusMetres==50.f);
    assert(sanitizedObjectDiscoverySettings({true,6.6f,false}).scanRadiusMetres==7.f);
    assert(sanitizedObjectDiscoverySettings({true,std::numeric_limits<float>::quiet_NaN(),false}).scanRadiusMetres==5.f);
    assert(objectDiscoveryHalfRingState(true,true)==2);
    assert(objectDiscoveryHalfRingState(true,false)==2);
    assert(objectDiscoveryHalfRingState(false,true)==1);
    assert(objectDiscoveryHalfRingState(false,false)==0);
    assert(objectDiscoveryRingPrefer(4.f,2,2,25.f,1,1,9.f));
    assert(objectDiscoveryRingPrefer(4.f,1,2,4.f,2,2,9.f));
    assert(!objectDiscoveryRingPrefer(25.f,1,1,4.f,2,2,9.f));
    const std::string path="object_discovery_policy_test.cfg";
    ObjectDiscoverySettings loaded{};
    assert(objectDiscoverySave(path,{true,12.f,true}));
    assert(objectDiscoveryLoad(path,loaded)&&loaded.enabled&&loaded.scanRadiusMetres==12.f&&loaded.grabReachLaserEnabled);
    {std::ofstream f(path);f<<"object_discovery_v1\nenabled=1\nscan_radius_m=8.0\ngrab_reach_m=3.5\n";}
    assert(objectDiscoveryLoad(path,loaded)&&loaded.scanRadiusMetres==8.f&&!loaded.grabReachLaserEnabled);
    {std::ofstream f(path);f<<"object_discovery_v2\nenabled=1\nscan_radius_m=8.0\ngrab_reach_m=5.0\ngrab_reach_laser=1\n";}
    assert(objectDiscoveryLoad(path,loaded)&&loaded.grabReachLaserEnabled);
    {std::ofstream f(path);f<<"object_discovery_v3\nenabled=1\nscan_radius_m=8.0\ngrab_reach_laser=bogus\n";}
    assert(!objectDiscoveryLoad(path,loaded)&&loaded.grabReachLaserEnabled);
    std::remove(path.c_str());
    // Rotated/off-centre bounds, nearest front face and strict reach.
    const float rayO[3]{0,0,0},rayD[3]{1,0,0},modelO[3]{30,0,0};
    const float I[9]{1,0,0,0,1,0,0,0,1};
    const float R[9]{0,1,0,-1,0,0,0,0,1};
    const float bounds[6]{-2,-5,-1,4,6,7};
    GrappleRayAnchor hit{};
    assert(grappleSurfaceBoundsHit(rayO,rayD,modelO,I,bounds,40,hit));
    assert(std::abs(hit.rayDistance-28.f)<.001f&&!hit.exactVisualMeshHit);
    assert(grappleSurfaceBoundsHit(rayO,rayD,modelO,R,bounds,40,hit));
    assert(std::abs(hit.rayDistance-24.f)<.001f);
    assert(!grappleSurfaceBoundsHit(rayO,rayD,modelO,R,bounds,23,hit));
    const float miss[3]{0,1,0};
    assert(!grappleSurfaceBoundsHit(rayO,miss,modelO,R,bounds,40,hit));
    const float bad[6]{0,0,0,0,1,1};
    assert(!grappleSurfaceBoundsHit(rayO,rayD,modelO,I,bad,40,hit));
    const float invalidAxis[9]{1,0,0,1,0,0,0,0,1};
    assert(!grappleSurfaceBoundsHit(rayO,rayD,modelO,invalidAxis,bounds,40,hit));
}
