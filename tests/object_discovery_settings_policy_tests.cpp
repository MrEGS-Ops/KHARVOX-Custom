#include "../src/openxr/ObjectDiscoverySettingsPolicy.h"
#include <cassert>
#include <limits>
#include <cstdio>
#include <string>

int main() {
    using namespace kharvox;
    const auto defaults = sanitizedObjectDiscoverySettings({});
    assert(!defaults.enabled);
    assert(!defaults.grabReachLaserEnabled);
    assert(defaults.scanRadiusMetres == 5.0f);
    assert(defaults.grabReachMetres == 2.0f);

    auto low = sanitizedObjectDiscoverySettings({true, -50.0f, -1.0f});
    assert(low.enabled);
    assert(low.scanRadiusMetres == 1.0f);
    assert(low.grabReachMetres == 1.0f);

    auto high = sanitizedObjectDiscoverySettings({true, 150.0f, 100.0f});
    assert(high.scanRadiusMetres == 50.0f);
    assert(high.grabReachMetres == 5.0f);

    auto stepped = sanitizedObjectDiscoverySettings({true, 6.6f, 3.3f});
    assert(stepped.scanRadiusMetres == 7.0f);
    assert(stepped.grabReachMetres == 3.5f);
    assert(sanitizedObjectDiscoverySettings({true,49.8f,3.5f})
        .scanRadiusMetres==50.f);
    // Independent physical LEFT and RIGHT halves, not current weapon hand.
    // Green persists across the whole scan radius; grab reach is irrelevant
    // to the colour. One saved hand remains half green/half amber even
    // beyond 1m, 2m, or any other selected grab reach.
    assert(objectDiscoveryHalfRingState(true,true)==2);
    assert(objectDiscoveryHalfRingState(true,false)==2);
    assert(objectDiscoveryHalfRingState(false,true)==1);
    assert(objectDiscoveryHalfRingState(false,false)==0);
    assert(objectDiscoveryHalfRingState(true,true)==2
        &&objectDiscoveryHalfRingState(false,true)==1);
    // Near grabs get the 32 visible slots before distant unknowns;
    // incomplete calibration wins before fully calibrated at equal range.
    assert(objectDiscoveryRingPrefer(4.f,2,2,25.f,1,1,9.f));
    assert(objectDiscoveryRingPrefer(4.f,1,2,4.f,2,2,9.f));
    assert(!objectDiscoveryRingPrefer(25.f,1,1,4.f,2,2,9.f));

    auto invalid = sanitizedObjectDiscoverySettings({true,
        std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity()});
    assert(invalid.scanRadiusMetres == 5.0f);
    assert(invalid.grabReachMetres == 2.0f);
    const std::string path="object_discovery_policy_test.cfg";
    assert(objectDiscoverySave(path,{true,12.f,4.5f,true}));
    ObjectDiscoverySettings loaded{};
    assert(objectDiscoveryLoad(path,loaded));
    assert(loaded.enabled && loaded.scanRadiusMetres==12.f
           && loaded.grabReachMetres==4.5f && loaded.grabReachLaserEnabled);
    // Older settings under ObjectGrip remain readable; their new laser is OFF.
    {
        std::ofstream previous(path,std::ios::trunc);
        previous<<"object_discovery_v1\n"
                <<"enabled=1\nscan_radius_m=8.0\ngrab_reach_m=3.5\n";
    }
    loaded={};
    assert(objectDiscoveryLoad(path,loaded));
    assert(loaded.enabled && loaded.grabReachMetres==3.5f
           && !loaded.grabReachLaserEnabled);
    // Malformed toggle must not mutate active settings.
    {
        std::ofstream invalid(path,std::ios::trunc);
        invalid<<"object_discovery_v2\n"
               <<"enabled=1\nscan_radius_m=8.0\ngrab_reach_m=3.5\n"
               <<"grab_reach_laser=bogus\n";
    }
    assert(!objectDiscoveryLoad(path,loaded));
    assert(!loaded.grabReachLaserEnabled);
    std::remove(path.c_str());
}
