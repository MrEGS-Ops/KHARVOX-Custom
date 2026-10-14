#pragma once

namespace kharvox {

// The physical exit of a behind-head zone closes its wheel.
// Releasing Grip while still in-zone does NOT close or confirm a selection.
enum class GestureWheelExit { None, Confirm, Cancel };

inline GestureWheelExit classifyGestureWheelExit(
    bool previouslyOpen, bool stillInZone, bool gripHeld) {
    if (!previouslyOpen || stillInZone) return GestureWheelExit::None;
    return gripHeld ? GestureWheelExit::Confirm : GestureWheelExit::Cancel;
}

// On a cancelled exit, keep DOOM's native wheel button pressed while sending
// a neutral selection stick before releasing the button. This ensures native
// input observes a neutral wheel update prior to its button-release handler.
// Does not claim to override DOOM's retained slot if it ignores centering.
struct GestureWheelCancelDrain {
    int neutralFramesRemaining{};
    void cancel() { neutralFramesRemaining = 3; }
    void reset() { neutralFramesRemaining = 0; }
    bool active() const { return neutralFramesRemaining > 0; }
    void frameSubmitted() {
        if (neutralFramesRemaining > 0) --neutralFramesRemaining;
    }
};

} // namespace kharvox
