#pragma once
#include <algorithm>
#include <cmath>

namespace kharvox {

// Planar strikes include forward jabs and sideways hooks. Reject upward
// guard/wheel gestures, backward pulls and invalid OpenXR velocity samples.
struct PhysicalPunchMotion {
    float horizontalSpeed{};
    bool legalDirection{};
    bool mostlyHorizontal{};
};

inline PhysicalPunchMotion classifyPhysicalPunch(
    float forwardSpeed, float lateralSpeed, float verticalSpeed,
    float threshold) {
    PhysicalPunchMotion out{};
    if (!std::isfinite(forwardSpeed) || !std::isfinite(lateralSpeed)
        || !std::isfinite(verticalSpeed) || !std::isfinite(threshold)
        || threshold <= 0.f) return out;
    const float lateral = std::abs(lateralSpeed);
    out.horizontalSpeed = std::hypot(std::max(0.f, forwardSpeed), lateral);
    out.legalDirection = forwardSpeed >= threshold * .20f
        || (lateral >= threshold * .80f
            && forwardSpeed > -threshold * .15f);
    out.mostlyHorizontal = std::abs(verticalSpeed)
        <= std::max(.65f, out.horizontalSpeed * .60f);
    return out;
}

} // namespace kharvox
