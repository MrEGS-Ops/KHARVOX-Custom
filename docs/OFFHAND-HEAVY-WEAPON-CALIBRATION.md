# KHARVOX heavy-weapon hand calibration — VR menu only
Active branch: `work/manager-187-wheel-holsters`. Applies to the Mod Manager's NativeBridge, not the original Cactus installation.

## In-headset controls
1. During gameplay, click **BOTH thumbsticks** to open the VR hand calibration panel.
2. Select **Gauss Cannon** or **Chaingun**; the menu detects which physical hand holds the gun. The hand selector chooses the physical glove to calibrate: the holding hand is PRIMARY and the other is SUPPORT.
3. Select **PROFILE: NORMAL** or **PROFILE: SIEGE/TURRET** to keep ordinary and alternate hand positions separate.
4. Press **CALIBRATE**; the panel closes for live placement using the LEFT controller.
5. For the RIGHT hand, a virtual point **15 cm forward** of the LEFT controller prevents physical controller collisions. Position and rotation are saved together.
6. For the LEFT support hand, only the weapon-local position is saved. Rotation is sampled afresh at the moment of every normal grip.
7. Pull **RIGHT trigger** to save and automatically reopen the menu. **RESET THIS WEAPON** clears only that weapon's overrides, including its alternate slots.

## Retired shortcuts and retained safety
- LEFT Grip + BOTH thumbsticks no longer captures support grip position.
- RIGHT Grip + RIGHT thumbstick no longer starts right-glove live calibration; releasing Right Grip cannot save calibration.
- The only hand calibration entry point is the VR menu. Single-click melee/pause behaviour outside the menu remains unchanged, except Hand Focus (when enabled) intentionally owns right-click. LEFT and RIGHT holster guns have independent primary glove calibrations, marked LEFT, RIGHT or BOTH.
- Native Gauss/Chaingun gun anchors, mode detection, existing saved hand profiles and active PAINSAW audio remain unchanged.
- Profiles are stored in `NativeBridge/offhand_right_hand_positions.cfg` and `NativeBridge/two_hand_weapon_profiles_saved.cfg`; per-weapon resets create adjacent `.before-vr-reset.bak` files.

Actual cursor alignment, alternate handle pose, haptics and display legibility still require in-headset testing.
