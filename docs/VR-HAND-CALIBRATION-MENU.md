# KHARVOX in-headset weapon-hand calibration (initial version)

This is an independent OpenXR compositor overlay in the **mod-enabled NativeBridge**, not a modification to the original DOOM or Cactus launcher's menus.

## Controls

- **Hold RIGHT thumbstick click for 1 second** during gameplay to open or close the VR calibration panel; another hold during calibration cancels and returns to the panel without saving. A normal short click remains DOOM world Use; it no longer toggles weapon-hand/head focus. Old LEFT Grip + BOTH sticks and RIGHT Grip + RIGHT stick capture shortcuts are gone.
- Point the **right controller** at a weapon, LEFT/RIGHT hand choice, CALIBRATE, or RESET THIS WEAPON. **Right trigger** selects the highlighted control. **Left thumbstick up/down** can scroll weapons; **right trigger** starts calibration when not pointing at a menu control. Only the explicit RESET THIS WEAPON button resets a profile.
- Equip the chosen weapon normally before starting calibration. The panel never cheats inventory or silently switches DOOM's selected weapon. A mismatch leaves the panel open with an explanation.
- For Gauss Cannon and Chaingun, choose **PROFILE: NORMAL** or **PROFILE: SIEGE/TURRET** in the same menu. These keep the separate alternate hand slots accessible without an old controller capture shortcut.
- On opening, KHARVOX detects whether the weapon was drawn into the **LEFT or RIGHT primary hand** (including Gunslinger hip holsters) and automatically selects that physical hand; the LEFT/RIGHT buttons can select the other hand for support-grip calibration.
- Selecting CALIBRATE hides the panel. The **opposite/free controller** moves the selected virtual hand live. Primary-hand fitting targets a point 15 cm ahead of that controller to prevent physical controller collision.
- Hold the **physical weapon-hand Grip** to use native alternate fire (Gauss Siege or Chaingun Turret) during live calibration; the other controller is free for fitting. Pull the **physical RIGHT trigger** once to save without firing and reopen the panel automatically.
- The panel displays **LEFT**, **RIGHT**, **BOTH** or **-** to indicate which physical *weapon-holding* hand has a saved position and rotation. Support-hand positions do not falsely count as primary-hand calibration. RESET THIS WEAPON restores the exact uploaded factory poses and support placements for that weapon (including Siege/Turret), keeping other weapons unchanged and backing up the previous user calibration.

## Rotation behaviour

**PRIMARY weapon hand (LEFT or RIGHT):** each physical hand has an independent position and weapon-local quaternion, saved together and rigidly locked to the weapon. Both holster sides work for pistol, shotgun, HAR, plasma rifle, rocket launcher, super shotgun, Gauss Cannon, Chaingun, BFG and chainsaw. New LEFT-primary settings use `<weapon>_left_hand_position` / `<weapon>_left_rotation` in the existing config; previous RIGHT-primary keys are unchanged.

**SUPPORT hand (opposite the weapon hand):** only the support position is saved. Its rotation is sampled anew on every normal grip. RIGHT support positions for LEFT-held weapons are independent of LEFT support positions for RIGHT-held weapons. Normal and Siege/Turret alternate profiles remain separately selectable.

## Settings and backups

The saved PRIMARY hand position now defines the weapon's physical grip pivot, keeping gun rotation centred on the calibrated grip (right or left). The same anchor drives two-handed aiming and the hand meshes. The original heavy-weapon anchor values stay intact as fallbacks.

The **editable** primary glove poses and rotations are stored at `UserData/offhand_right_hand_positions.cfg`. The right-primary support grips are in `UserData/two_hand_weapon_profiles_saved.cfg`; left-primary support grips are in `UserData/two_hand_weapon_profiles_left_primary_saved.cfg`. On a fresh install these are seeded from the owner's uploaded calibration files shipped read-only in `NativeBridge/CalibrationDefaults/`. Per-weapon Reset restores matching factory values without deleting other weapon calibrations, while writing `.before-vr-reset.bak` beside the active files. The updater preserves the entire `UserData` directory.

### Build/test limitations

Windows/GitHub builds and pure calibration policy tests can check compilation and serialization. Actual pointing, legibility, controller haptics and calibration alignment need Quest 3 headset verification. The menu supports weapons equipped in either physical hand and requires the existing Off-Hand Grip Capture mod to be enabled.
