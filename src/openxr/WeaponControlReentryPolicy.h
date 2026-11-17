#pragma once

namespace kharvox {

// DOOM can keep the ordinary world camera during interactive terminal
// minigames while temporarily inhibiting first-person weapon controls.
// Reacquiring the game camera alone is not sufficient to detect that seam.
enum class WeaponControlTransition { None, Suspended, Resumed };

struct WeaponControlReentryPolicy {
    bool suspended{};

    WeaponControlTransition update(bool classifierVerified,
                                   bool worldGameplay,
                                   bool nativeWeaponControl) {
        // Never alter the proven 6DoF path on an unrecognized game build.
        if (!classifierVerified) {
            suspended = false;
            return WeaponControlTransition::None;
        }
        if (worldGameplay && !nativeWeaponControl) {
            if (!suspended) {
                suspended = true;
                return WeaponControlTransition::Suspended;
            }
            return WeaponControlTransition::None;
        }
        if (worldGameplay && nativeWeaponControl && suspended) {
            suspended = false;
            return WeaponControlTransition::Resumed;
        }
        // Keep the suspension across menus/camera changes; the next
        // genuine weapon-control frame must reacquire the controller anchor.
        return WeaponControlTransition::None;
    }
};
} // namespace kharvox
