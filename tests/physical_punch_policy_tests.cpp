#include "../src/openxr/PhysicalPunchPolicy.h"
#include <cassert>
#include <limits>

int main() {
    using kharvox::classifyPhysicalPunch;
    const float t = 2.0f;
    const auto jab = classifyPhysicalPunch(2.2f, .2f, .1f, t);
    assert(jab.legalDirection && jab.mostlyHorizontal
        && jab.horizontalSpeed >= t);
    const auto hook = classifyPhysicalPunch(.1f, 2.1f, .15f, t);
    assert(hook.legalDirection && hook.mostlyHorizontal
        && hook.horizontalSpeed >= t);
    const auto raise = classifyPhysicalPunch(.1f, .15f, 3.f, t);
    assert(!raise.legalDirection || !raise.mostlyHorizontal);
    const auto diagonalUp = classifyPhysicalPunch(2.f, .3f, 2.f, t);
    assert(!diagonalUp.mostlyHorizontal);
    const auto backward = classifyPhysicalPunch(-2.3f, .1f, .05f, t);
    assert(!backward.legalDirection);
    const auto invalid = classifyPhysicalPunch(
        std::numeric_limits<float>::quiet_NaN(), 2.f, 0.f, t);
    assert(!invalid.legalDirection);
    return 0;
}
