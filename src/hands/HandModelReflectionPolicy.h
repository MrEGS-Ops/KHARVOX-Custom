#pragma once

namespace kharvox::hands {

// Mirror the RIGHT hand's controller-local orientation basis to the LEFT:
// R_left = Mx * R_right * Mx. The two reflections cancel handedness, so this
// remains a proper rotation (determinant +1) and does not flip the model
// inside-out. The LEFT fist GLB uses a separately mirrored RIGHT fist mesh;
// the LEFT gun-holding GLB is already a native left-handed model.
//
// Importantly, a fixed 180-degree Z or Y turn is NOT a mirror of a general
// 3D wrist rotation. It made the left palm/knuckles face the wrong way.
// Positions and the tracked physical controller remain independent.
template<class Matrix,class Multiply>
inline Matrix handModelReferenceRotation(bool physicalLeft,
    const Matrix& rightReference, const Matrix& mirrorX, Multiply multiply) {
    return physicalLeft
        ? multiply(mirrorX,multiply(rightReference,mirrorX))
        : rightReference;
}

} // namespace kharvox::hands
