#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace kharvox {
// Round, hollow, very faint tracking-space grab-zone debug marker.
// This is ALPHA, not a shader discard: the compositor must be told the
// pixels are straight RGBA (UNPREMULTIPLIED_ALPHA_BIT).
inline std::uint8_t gripZoneOutlineAlpha(float x,float y) noexcept {
    if(!std::isfinite(x)||!std::isfinite(y))return 0;
    const float r=std::sqrt(x*x+y*y);
    if(r>=.98f)return 0; // transparent corners and outside circle
    const float edge=(r-.78f)/.052f;
    const float alpha=.17f*std::exp(-.5f*edge*edge);
    return static_cast<std::uint8_t>(
        std::clamp(alpha*255.f,0.f,255.f));
}
} // namespace kharvox
