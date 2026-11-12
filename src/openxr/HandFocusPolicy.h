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
} // namespace kharvox
