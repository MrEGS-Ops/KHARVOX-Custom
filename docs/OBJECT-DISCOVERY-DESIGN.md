# Object Discovery Mode — replacement design

This document records the replacement for the retired World Grab debug and
activation experiments. **The continuous scanner, rings and in-game item
calibration are not yet implemented in this cleanup branch.**

## UI and gameplay
- Opt-in toggle under Mods > DEV > Experimental Features.
- Continuous, performance-limited scan during normal gameplay.
- Scan radius user-selectable from **1 to 15 metres**, default **5 m**,
  in 1 m steps.
- **Grab reach is a separate slider**, **1–5 metres**, default **2 m**,
  in 0.5 m steps. Long reach must never bypass native movement evidence.
- Subtle floating world-space rings (not material changes):
  * Grey = candidate physics object whose movability is unverified.
  * Amber = verified movable type with no saved calibration.
  * Green = same verified type with a saved calibration.
- Update all visible matching rings immediately after profile save.
- No free-hand ID circle. No manual naming or catalogue maintenance.
- Auto-generate profile names from validated model/declaration identity,
  not temporary in-level object handles.
- On successful grab of already physically moving unconfigured object,
  open a compact calibration overlay without the Mods menu.
- Reuse Weapon Grip Calibration centimetre nudges and controller-driven
  OBJECT rotation, never rotate the hand/glove; save left/right profiles
  separately in UserData.
- Already calibrated objects are grabbable without interrupting play.

## Physics safety invariant
- Scan, proximity, ray hit, lookup and **holding Grip** are strictly
  read-only while the specific object has no independently verified
  movement initiated by DOOM's own native physics.
- No Activate calls, fake kicks, contact proxies, micro moves,
  force-to-follow, origin/axis/momentum writes, material edits or other
  changes while the object is stationary.
- An at-rest flag flipping or a successful native function call is
  **not** proof of simulated movement; match the verified native body
  and its instance before allowing any grab.
- Saved profiles do not permit grabbing inactive individual instances.
- If tracking disappears or hand tracking takes over, stop safely
  without treating synthetic inputs as valid controller input.
- Keep normal weapon/calibration, Gunslinger, HUD and focus behavior.

## Integration dependencies and limits
- The surviving DOOM-thread FocusTracker candidate and resolved-entity
  hooks are **read-only ray observations**; they do not enumerate a
  radius and do not yet prove actual rigid-body movability.
- Native proximity enumeration, durable model identity, movement
  verification, projected world-space rings, calibration overlay and
  physically stable release need independent implementation and VR testing.
- The old automatic wake/contact/micro/tick/held-kick sequence, forced
  direct-follow fallback, momentum reset and physics HUD have been removed.
- Prior experimental history is retained only in Git history and backup
  branch `backup/pre-object-discovery-cleanup-20261122`.
