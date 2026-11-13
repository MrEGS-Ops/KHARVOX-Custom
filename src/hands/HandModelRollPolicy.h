#pragma once

#include "HandVisibilityPolicy.h"
#include <cstddef>

namespace kharvox::hands {

// Restore shipped KHARVOX left and right GLBs unmodified.
// Each physical hand loads its OWN mesh and its own calibration.
inline constexpr std::size_t handReferenceAssetIndex(
    bool physicalLeft, HandModelKind kind) noexcept {
    return kind==HandModelKind::GunHolding
        ?(physicalLeft?2u:3u):(physicalLeft?0u:1u);
}
inline constexpr bool handMeshMirroredFromRight(
    bool, HandModelKind) noexcept {
    return false; // use native GLB, no added mirror
}
inline constexpr float handModelControllerForwardRollDegrees(
    bool, HandModelKind) noexcept {
    // No second left-only half-turn. The local-X mirror already produces
    // the anatomically left counterpart of the verified upright right mesh.
    return 0.f;
}

} // namespace kharvox::hands
