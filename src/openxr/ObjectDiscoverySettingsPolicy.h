#pragma once
#include <algorithm>
#include <cmath>
#include <fstream>
#include <string>
#include <iomanip>

namespace kharvox {
// Pure settings policy. This does not enumerate or modify DOOM entities.
// The actual scanner/calibration runtime will consume these values later.
inline constexpr float kObjectDiscoveryMinScanRadiusM = 1.0f;
inline constexpr float kObjectDiscoveryMaxScanRadiusM = 15.0f;
inline constexpr float kObjectDiscoveryDefaultScanRadiusM = 5.0f;
inline constexpr float kObjectDiscoveryScanRadiusStepM = 1.0f;

inline constexpr float kObjectDiscoveryMinGrabReachM = 1.0f;
inline constexpr float kObjectDiscoveryMaxGrabReachM = 5.0f;
inline constexpr float kObjectDiscoveryDefaultGrabReachM = 2.0f;
inline constexpr float kObjectDiscoveryGrabReachStepM = 0.5f;

struct ObjectDiscoverySettings {
    bool enabled{false}; // Opt-in.
    bool grabReachLaserEnabled{false}; // Visual aid only; OFF by default.
    float scanRadiusMetres{kObjectDiscoveryDefaultScanRadiusM};
    float grabReachMetres{kObjectDiscoveryDefaultGrabReachM};
};

inline float objectDiscoverySnap(float value, float minValue, float maxValue,
                                 float step, float fallback) noexcept {
    if (!std::isfinite(value)) return fallback;
    const float bounded = std::clamp(value, minValue, maxValue);
    const float ticks = std::round((bounded - minValue) / step);
    return std::clamp(minValue + ticks * step, minValue, maxValue);
}

inline ObjectDiscoverySettings sanitizedObjectDiscoverySettings(
    ObjectDiscoverySettings requested) noexcept {
    requested.scanRadiusMetres = objectDiscoverySnap(
        requested.scanRadiusMetres,
        kObjectDiscoveryMinScanRadiusM, kObjectDiscoveryMaxScanRadiusM,
        kObjectDiscoveryScanRadiusStepM, kObjectDiscoveryDefaultScanRadiusM);
    requested.grabReachMetres = objectDiscoverySnap(
        requested.grabReachMetres,
        kObjectDiscoveryMinGrabReachM, kObjectDiscoveryMaxGrabReachM,
        kObjectDiscoveryGrabReachStepM, kObjectDiscoveryDefaultGrabReachM);
    return requested;
}

inline bool objectDiscoverySave(const std::string& path,
                                ObjectDiscoverySettings settings) {
    settings = sanitizedObjectDiscoverySettings(settings);
    std::ofstream output(path, std::ios::trunc);
    if (!output) return false;
    output << "object_discovery_v2\n"
           << "enabled=" << (settings.enabled ? 1 : 0) << "\n"
           << "scan_radius_m=" << std::fixed << std::setprecision(1)
           << settings.scanRadiusMetres << "\n"
           << "grab_reach_m=" << settings.grabReachMetres << "\n"
           << "grab_reach_laser=" << (settings.grabReachLaserEnabled ? 1 : 0) << "\n";
    output.flush();
    return output.good();
}
inline bool objectDiscoveryLoad(const std::string& path,
                                ObjectDiscoverySettings& result) {
    std::ifstream input(path);
    if (!input) return false;
    std::string header, enabled, scan, reach, laser, extra;
    if (!std::getline(input,header)
        || (header!="object_discovery_v1"&&header!="object_discovery_v2")
        || !std::getline(input,enabled) || !std::getline(input,scan)
        || !std::getline(input,reach))return false;
    if (header=="object_discovery_v2"
        &&(!std::getline(input,laser)
            ||(laser!="grab_reach_laser=0"&&laser!="grab_reach_laser=1")))
        return false;
    if (std::getline(input,extra))return false;
    if (enabled!="enabled=0" && enabled!="enabled=1") return false;
    const auto parse = [](const std::string& item, const char* key, float& out) {
        const std::string prefix(key);
        if (item.compare(0,prefix.size(),prefix)!=0) return false;
        const std::string value=item.substr(prefix.size());
        if (value.empty()) return false;
        try {
            std::size_t consumed{};
            out=std::stof(value,&consumed);
            return consumed==value.size()&&std::isfinite(out);
        } catch (...) {return false;}
    };
    ObjectDiscoverySettings parsed{};
    parsed.enabled=enabled=="enabled=1";
    parsed.grabReachLaserEnabled=laser=="grab_reach_laser=1";
    if (!parse(scan,"scan_radius_m=",parsed.scanRadiusMetres)
        || !parse(reach,"grab_reach_m=",parsed.grabReachMetres))
        return false;
    result=sanitizedObjectDiscoverySettings(parsed);
    return true;
}
} // namespace kharvox
