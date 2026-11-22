#pragma once
#include <algorithm>
#include <cmath>

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
    bool enabled{false}; // Opt-in, no active scanner until implemented.
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
} // namespace kharvox
