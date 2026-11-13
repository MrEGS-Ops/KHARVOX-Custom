#pragma once

#include "HandVisibilityPolicy.h"

namespace kharvox::hands {

// The left empty-fist and left gun-holding meshes are upside-down relative
// to their physical controller. Roll their models around the controller's
// local forward (-Z) axis, not about up (Y), so pointing is unchanged.
// Applied in controller space before each model's baked/calibrated rotation.
// Never change the independently tracked controller or right-hand models.
inline constexpr float handModelControllerForwardRollDegrees(
    bool physicalLeft, HandModelKind model) noexcept {
    return physicalLeft&&(model==HandModelKind::Fist||
        model==HandModelKind::GunHolding) ? 180.f : 0.f;
}

} // namespace kharvox::hands
