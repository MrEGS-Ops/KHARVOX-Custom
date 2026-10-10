#include "../src/visual/VisualTonePolicy.h"
#include <cmath>
#include <limits>

static bool near(float a, float b) { return std::fabs(a - b) < 0.0001f; }
int main() {
    const kharvox::LinearRgb sample{0.2f, 0.4f, 0.6f};
    const auto same = kharvox::applyVisualTone(sample, {});
    if (!near(same.r, sample.r) || !near(same.g, sample.g) || !near(same.b, sample.b)) return 1;
    const auto gray = kharvox::applyVisualTone(sample, {0.0f,1.0f,0.0f});
    if (!near(gray.r,gray.g) || !near(gray.g,gray.b)) return 2;
    const auto bright = kharvox::applyVisualTone(sample, {1.0f,1.0f,1.0f});
    if (!near(bright.r,0.4f) || !near(bright.g,0.8f) || !near(bright.b,1.0f)) return 3;
    const auto left = kharvox::applyVisualTone(sample, {0.25f,1.1f,0.9f});
    const auto right = kharvox::applyVisualTone(sample, {0.25f,1.1f,0.9f});
    if (!near(left.r,right.r) || !near(left.g,right.g) || !near(left.b,right.b)) return 4;
    const auto nan = std::numeric_limits<float>::quiet_NaN();
    const auto safe = kharvox::applyVisualTone({nan,0.4f,0.6f},{nan,nan,nan});
    if (!std::isfinite(safe.r) || !std::isfinite(safe.g) || !std::isfinite(safe.b)) return 5;
    const auto clamped = kharvox::validateVisualTone({8.0f,20.0f,-30.0f});
    if (!near(clamped.exposureStops,1.0f) || !near(clamped.contrast,1.35f)
        || !near(clamped.saturation,0.0f)) return 6;
    return 0;
}
