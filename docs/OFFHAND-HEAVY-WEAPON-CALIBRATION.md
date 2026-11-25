# RIPSHIFT heavy-weapon hand calibration — VR menu only
Active source: `feature/object-discovery-full-grip-calibration-20261123`. This uses RIPSHIFT's in-headset Mods menu, not a separate external Mod Manager or a replacement for the original KHARVOX installation.

## In-headset controls
1. During gameplay, **hold RIGHT thumbstick click for 1 second** to open the VR hand calibration panel. Short click still toggles Hand Focus.
2. Select **Gauss Cannon** or **Chaingun**; the menu detects which physical hand holds the gun. The hand selector chooses the physical glove to calibrate: the holding hand is PRIMARY and the other is SUPPORT.
3. Select **PROFILE: NORMAL** or **PROFILE: ALT FIRE** for both Gauss Cannon and Minigun, keeping primary and secondary-fire grip positions separate regardless of the equipped attachment.
4. Press **CALIBRATE**; the panel closes for live placement using the LEFT controller.
5. For the RIGHT hand, a virtual point **15 cm forward** of the LEFT controller prevents physical controller collisions. Position and rotation are saved together.
6. For the LEFT support hand, only the weapon-local position is saved. Rotation is sampled afresh at the moment of every normal grip.
7. Pull **RIGHT trigger** to save and automatically reopen the menu. **RESET THIS WEAPON** clears only that weapon's overrides, including its alternate slots.

## Retired shortcuts and retained safety
- LEFT Grip + BOTH thumbsticks no longer captures support grip position.
- RIGHT Grip + RIGHT thumbstick no longer starts right-glove live calibration; releasing Right Grip cannot save calibration.
- The only hand calibration entry point is the VR menu. Single-click melee/pause behaviour outside the menu remains unchanged, except Hand Focus (when enabled) intentionally owns right-click. LEFT and RIGHT holster guns have independent primary glove calibrations, marked LEFT, RIGHT or BOTH.
- Native Gauss/Chaingun gun anchors, mode detection, existing saved hand profiles and active PAINSAW audio remain unchanged.
- Profiles belong in RIPSHIFT's portable UserData calibration storage. Old flat UserData remains readable for backward compatibility; a separate manager NativeBridge folder is not used.

Actual cursor alignment, alternate handle pose, haptics and display legibility still require in-headset testing.
