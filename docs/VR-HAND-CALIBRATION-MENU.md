# KHARVOX in-headset weapon-hand calibration (initial version)

This is an independent OpenXR compositor overlay in the **mod-enabled NativeBridge**, not a modification to the original DOOM or Cactus launcher's menus.

## Controls

- **Hold RIGHT thumbstick click for 1 second** during gameplay to open or close the VR calibration panel; another hold during calibration cancels and returns to the panel without saving. A normal short click still toggles Hand Focus (when enabled). Old LEFT Grip + BOTH sticks and RIGHT Grip + RIGHT stick capture shortcuts are gone.
- Point the **right controller** at a weapon, LEFT/RIGHT hand choice, CALIBRATE, or RESET THIS WEAPON. **Right trigger** selects the highlighted control. **Left thumbstick up/down** can scroll weapons; **right trigger** starts calibration when not pointing at a menu control. Only the explicit RESET THIS WEAPON button resets a profile.
- Equip the chosen weapon normally before starting calibration. The panel never cheats inventory or silently switches DOOM's selected weapon. A mismatch leaves the panel open with an explanation.
- For Gauss Cannon and Chaingun, choose **PROFILE: NORMAL** or **PROFILE: SIEGE/TURRET** in the same menu. These keep the separate alternate hand slots accessible without an old controller capture shortcut.
- On opening, KHARVOX detects whether the weapon was drawn into the **LEFT or RIGHT primary hand** (including Gunslinger hip holsters) and automatically selects that physical hand; the LEFT/RIGHT buttons can select the other hand for support-grip calibration.
- Selecting CALIBRATE hides the panel. The **opposite/free controller** moves the selected virtual hand live. Primary-hand fitting targets a point 15 cm ahead of that controller to prevent physical controller collision.
- Pull the **physical RIGHT trigger** once to persist the selected weapon/hand and reopen the panel automatically.
- The panel displays **LEFT**, **RIGHT**, **BOTH** or **-** to indicate which physical *weapon-holding* hand has a saved position and rotation. Support-hand positions do not falsely count as primary-hand calibration. RESET THIS WEAPON clears that weapon's left/right glove and support-hand overrides, including its heavy-weapon alternate overrides; verified native Gauss/Chaingun gun anchors and other weapon profiles remain unchanged.

## Rotation behaviour

**PRIMARY weapon hand (LEFT or RIGHT):** each physical hand has an independent position and weapon-local quaternion, saved together and rigidly locked to the weapon. Both holster sides work for pistol, shotgun, HAR, plasma rifle, rocket launcher, super shotgun, Gauss Cannon, Chaingun, BFG and chainsaw. New LEFT-primary settings use `<weapon>_left_hand_position` / `<weapon>_left_rotation` in the existing config; previous RIGHT-primary keys are unchanged.

**SUPPORT hand (opposite the weapon hand):** only the support position is saved. Its rotation is sampled anew on every normal grip. RIGHT support positions for LEFT-held weapons are independent of LEFT support positions for RIGHT-held weapons. Normal and Siege/Turret alternate profiles remain separately selectable.

## Settings and backups

The existing `NativeBridge/offhand_right_hand_positions.cfg` now stores **both physical LEFT and RIGHT primary glove poses** with independent keys. `NativeBridge/two_hand_weapon_profiles_saved.cfg` still holds LEFT support positions for RIGHT-held weapons; the new `NativeBridge/two_hand_weapon_profiles_left_primary_saved.cfg` holds RIGHT support positions for LEFT-held weapons. Per-weapon RESET writes adjacent `.before-vr-reset.bak` backups. The NativeBridge updater preserves all active profile files and backups. All files remain in the portable manager folder.

### Build/test limitations

Windows/GitHub builds and pure calibration policy tests can check compilation and serialization. Actual pointing, legibility, controller haptics and calibration alignment need Quest 3 headset verification. The menu supports weapons equipped in either physical hand and requires the existing Off-Hand Grip Capture mod to be enabled.
