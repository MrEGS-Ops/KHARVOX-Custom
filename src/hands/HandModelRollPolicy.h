#pragma once

#include "HandVisibilityPolicy.h"
#include <cstddef>

namespace kharvox::hands {

// The right bare fist and the right equipped glove were confirmed to be
// upright in the headset. Render the matching LEFT variant from that same
// baked RIGHT mesh mirrored ONLY in local X. Do NOT also roll it 180 degrees:
// the extra controller-space half-turn was inverting the left-hand visual
// orientation despite its otherwise correctly aligned right-hand reference.
// Mirroring applies only to the visible mesh; the left tracked controller,
// primary weapon pose, and depth/compositor remain completely untouched.
inline constexpr std::size_t handReferenceAssetIndex(
    bool physicalLeft, HandModelKind kind) noexcept {
    (void)physicalLeft;
    return kind==HandModelKind::GunHolding?3u:1u;
}
inline constexpr bool handMeshMirroredFromRight(
    bool physicalLeft, HandModelKind kind) noexcept {
    return physicalLeft&&(kind==HandModelKind::Fist
        ||kind==HandModelKind::GunHolding);
}
inline constexpr float handModelControllerForwardRollDegrees(
    bool, HandModelKind) noexcept {
    // No second left-only half-turn. The local-X mirror already produces
    // the anatomically left counterpart of the verified upright right mesh.
    return 0.f;
}

} // namespace kharvox::hands
