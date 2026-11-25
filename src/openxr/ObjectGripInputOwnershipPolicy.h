#pragma once

namespace kharvox {
// A native movable object is attached to the free hand. Ownership is
// independent of which hand is the weapon hand and whether calibration is
// currently visible. Any attached object blocks native equipment/grenade
// input so the same support trigger cannot spawn a grenade inside the prop.
inline constexpr bool objectGripBlocksOffhandEquipment(
    bool gameplay,bool discoveryEnabled,bool xrAttachmentHeld,
    bool nativeAttachmentHeld) noexcept {
    return gameplay&&discoveryEnabled
        &&(xrAttachmentHeld||nativeAttachmentHeld);
}
// In calibration the trigger alone selects 5cm positioning; the chord
// deliberately resets to the centered upright starting pose on its PRESS
// edge, even when the object already has a persisted profile.
inline constexpr bool objectGripNeutralResetChordEdge(
    bool objectHeld,bool objectMode,bool supportGripDown,
    bool offhandTriggerDown,bool wasDown) noexcept {
    return objectHeld&&objectMode&&supportGripDown
        &&offhandTriggerDown&&!wasDown;
}
