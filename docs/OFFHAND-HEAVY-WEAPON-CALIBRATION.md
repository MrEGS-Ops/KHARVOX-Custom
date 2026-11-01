# Off-Hand Grip: Gauss Cannon and Chaingun calibration (Quest 3)
Active branch: `work/manager-187-wheel-holsters` — VR Mod Manager NativeBridge.

With **Off-Hand Grip Capture** enabled and either Gauss Cannon or Chaingun equipped, the LEFT controller acts as a placement/rotation probe for the RIGHT visual glove. The native gun pivot and its hand are linked, not separate.

- **Both joystick clicks, no Grip held**: put LEFT hand where the RIGHT hand should hold the gun; save weapon anchor POSITION (only for the equipped heavy gun).
- **Hold RIGHT Grip + both joystick clicks**: rotate LEFT controller to the desired RIGHT-glove orientation; save **RIGHT-hand ROTATION** in the gun's local frame. No position changes.
- **Hold LEFT Grip + both joystick clicks**: place LEFT hand on the new support point on the heavy gun; save the **LEFT two-hand support calibration** relative to the adjusted weapon anchor. Release and re-grip the LEFT controller after the cue.
- **Do not hold both Grips** when clicking both sticks: ambiguous chords are rejected. No other weapon position/rotation changes from these heavy-weapon modes.
- **Calibration click safety:** the joystick capture chord consumes both stick clicks even if you press them at different moments while both are eventually held together. Single-click melee (right) and pause (left) trigger **on release** when Off-Hand Grip Capture is enabled, never on press, so they cannot fire a normal melee animation/pause halfway through calibration. With Off-Hand Grip Capture disabled, ordinary joystick click timing is unchanged.

Each successful capture is recorded in `logs/diagnostics/OFFHAND-GRIP-CAPTURE.csv`, with outcome details in `offhand_grip_capture_status.txt`. Rejected captures emit a short light haptic; successful captures emit the existing longer confirmation pulse.

The right-hand position and rotation are stored in portable `NativeBridge/offhand_right_hand_positions.cfg`. Left support values stay in `NativeBridge/two_hand_weapon_profiles_saved.cfg`. The previous files are backed up before replacement. All settings stay local, survive updater refresh, and remain distinct for Gauss vs Chaingun.

**Gauss/Chaingun melee recovery:** The saved right-hand rotation is reapplied to the visual glove when gameplay resumes, even if the transient melee animation changes grip state. The full DOOM weapon model may temporarily animate during melee itself.

**Safety:** never manipulate physical weapon firing, melee buttons, controller tracking, crouch, turn or virtual gunstock. Use normal hand tracking for aiming. These VR pose features require in-headset validation; passing unit tests alone cannot certify visual alignment.
