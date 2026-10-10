#pragma once
#include <algorithm>
#include <cmath>

namespace kharvox {
// CPU reference for a future identically applied per-eye linear-colour pass.
// This policy does not hook or alter any renderer on its own.
struct LinearRgb { float r{}, g{}, b{}; };
struct VisualToneSettings {
    float exposureStops{};
    float contrast{1.0f};
    float saturation{1.0f};
};
inline VisualToneSettings validateVisualTone(VisualToneSettings v) noexcept {
    if (!std::isfinite(v.exposureStops)) v.exposureStops = 0.0f;
    if (!std::isfinite(v.contrast)) v.contrast = 1.0f;
    if (!std::isfinite(v.saturation)) v.saturation = 1.0f;
    v.exposureStops = std::clamp(v.exposureStops, -1.0f, 1.0f);
    v.contrast = std::clamp(v.contrast, 0.65f, 1.35f);
    v.saturation = std::clamp(v.saturation, 0.0f, 1.5f);
    return v;
}
inline LinearRgb applyVisualTone(LinearRgb input, VisualToneSettings values) noexcept {
    const auto v = validateVisualTone(values);
    const float exposure = std::exp2(v.exposureStops);
    float r = std::isfinite(input.r) ? input.r * exposure : 0.0f;
    float g = std::isfinite(input.g) ? input.g * exposure : 0.0f;
    float b = std::isfinite(input.b) ? input.b * exposure : 0.0f;
    // Keep identical settings for both eyes; no depth or screen-coordinate dependency.
    constexpr float pivot = 0.18f;
    r = (r - pivot) * v.contrast + pivot;
    g = (g - pivot) * v.contrast + pivot;
    b = (b - pivot) * v.contrast + pivot;
    const float luma = r * .2126f + g * .7152f + b * .0722f;
    return {
        std::clamp(luma + (r - luma) * v.saturation, 0.0f, 1.0f),
        std::clamp(luma + (g - luma) * v.saturation, 0.0f, 1.0f),
        std::clamp(luma + (b - luma) * v.saturation, 0.0f, 1.0f)
    };
}
} // namespace kharvox
