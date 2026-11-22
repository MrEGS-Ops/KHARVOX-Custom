#include "../src/openxr/ObjectDiscoverySettingsPolicy.h"
#include <cassert>
#include <limits>
#include <cstdio>
#include <string>

int main() {
    using namespace kharvox;
    const auto defaults = sanitizedObjectDiscoverySettings({});
    assert(!defaults.enabled);
    assert(defaults.scanRadiusMetres == 5.0f);
    assert(defaults.grabReachMetres == 2.0f);

    auto low = sanitizedObjectDiscoverySettings({true, -50.0f, -1.0f});
    assert(low.enabled);
    assert(low.scanRadiusMetres == 1.0f);
    assert(low.grabReachMetres == 1.0f);

    auto high = sanitizedObjectDiscoverySettings({true, 150.0f, 100.0f});
    assert(high.scanRadiusMetres == 15.0f);
    assert(high.grabReachMetres == 5.0f);

    auto stepped = sanitizedObjectDiscoverySettings({true, 6.6f, 3.3f});
    assert(stepped.scanRadiusMetres == 7.0f);
    assert(stepped.grabReachMetres == 3.5f);

    auto invalid = sanitizedObjectDiscoverySettings({true,
        std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity()});
    assert(invalid.scanRadiusMetres == 5.0f);
    assert(invalid.grabReachMetres == 2.0f);
    const std::string path="object_discovery_policy_test.cfg";
    assert(objectDiscoverySave(path,{true,12.f,4.5f}));
    ObjectDiscoverySettings loaded{};
    assert(objectDiscoveryLoad(path,loaded));
    assert(loaded.enabled && loaded.scanRadiusMetres==12.f
           && loaded.grabReachMetres==4.5f);
    std::remove(path.c_str());
}
