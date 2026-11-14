#pragma once
namespace kharvox {
// Hand Focus is a persistent aim mode, not a toggle tied to the USE button.
// Interaction presses (RS / keyboard E) must never change it. Native authored
// actions own the head aim while they run; ordinary DOOM gameplay uses the
// tracked CURRENT weapon hand (including Gunslinger left-hand ownership).
inline constexpr bool useWeaponHandFocus(bool handFocusEnabled,
    bool gameplay,bool syncGloryKill,bool chainsawExecution,
    bool ledgeTransition,bool bossSequence) noexcept {
    return handFocusEnabled && gameplay
        && !syncGloryKill && !chainsawExecution
        && !ledgeTransition && !bossSequence;
}

// Native FocusTracker works in all playable WORLD views, including
// first-person interactable transitions whose player-physics snapshot can be
// older than a render frame. Prefer a valid controller ray without requiring
// an HMD render-pose sample first. Scripted executions remain fully native.
enum class WorldFocusRaySource { Native, Hand, Head };
inline constexpr WorldFocusRaySource chooseWorldFocusRaySource(
    bool worldActive,bool cinematic,bool bossSequence,
    bool handRayValid,bool headRayValid) noexcept {
    if(!worldActive||cinematic||bossSequence)
        return WorldFocusRaySource::Native;
    if(handRayValid)return WorldFocusRaySource::Hand;
    return headRayValid?WorldFocusRaySource::Head:WorldFocusRaySource::Native;
}
} // namespace kharvox
