# KHARVOX in-headset weapon-hand calibration (initial version)

This is an independent OpenXR compositor overlay in the **mod-enabled NativeBridge**, not a modification to the original DOOM or Cactus launcher's menus.

## Controls

- **Click both thumbsticks together** during gameplay to open or close the VR calibration panel. The same chord during calibration cancels calibration and returns to the panel without saving.
- Point the **right controller** at a weapon, LEFT/RIGHT hand choice, CALIBRATE, or RESET THIS WEAPON. **Right trigger** selects the highlighted control. **Left thumbstick up/down** can scroll weapons; **right trigger** starts calibration when not pointing at a menu control. Only the explicit RESET THIS WEAPON button resets a profile.
- Equip the chosen weapon normally before starting calibration. The panel never cheats inventory or silently switches DOOM's selected weapon. A mismatch leaves the panel open with an explanation.
- Selecting CALIBRATE hides the panel. The **left controller** then moves the selected virtual hand live; RIGHT-hand fitting targets a point 15 cm ahead of the left controller along its pointing direction to prevent real controller collision.
- Pull the **physical RIGHT trigger** once to persist the selected weapon/hand and reopen the panel automatically.
- The panel displays **R / L** saved markers for each weapon. RESET THIS WEAPON clears only that weapon's right and left hand overrides, including its heavy-weapon alternate overrides; the verified native Gauss/Chaingun gun anchors and all other weapon profiles remain unchanged.

## Rotation behaviour

**RIGHT primary hand:** position and weapon-local quaternion are saved together and remain locked to the weapon. The VR menu extends right-hand glove profiles to pistol, shotgun, HAR, plasma rifle, rocket launcher, super shotgun, Gauss Cannon, Chaingun, BFG and chainsaw.

**LEFT support hand:** ONLY the weapon-local support position is saved. On ordinary grip acquisition the hand orientation is sampled exactly as before, so every fresh grab may have a different rotation. The live calibration preview temporarily follows the current left controller orientation, but it is never written as a locked left-hand quaternion.

## Settings and backups

The existing local `NativeBridge/offhand_right_hand_positions.cfg` saves right-hand glove poses; `NativeBridge/two_hand_weapon_profiles_saved.cfg` saves left support positions. RESET writes adjacent `.before-vr-reset.bak` copies first. NativeBridge installation copies those backups and both live settings from the old runtime when updating. All files remain under the portable manager's local folder.

### Build/test limitations

Windows/GitHub builds and pure calibration policy tests can check compilation and serialization. Actual pointing, legibility, controller haptics and calibration alignment need Quest 3 headset verification. The menu currently operates on equipped right-handed weapons and requires the existing Off-Hand Grip Capture mod to be enabled.
