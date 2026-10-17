# KHARVOX Sync Action Speed — focused action testing

These are **experimental whole-engine tick targets**, not rendered FPS
or independent animation clocks. No action overrides turn on automatically.

## Interface

- Presets **1 Native, 2 90 Hz, 3 70 Hz, 4 50 Hz, 5 30 Hz** remain at the
  top of VR Mods, outside Advanced.
- Checking **any** of the five action overrides in Advanced greys out **all
  five** presets. On launch, the previously chosen preset is temporarily
  replaced by Native (1) as the between-action baseline; the saved choice is
  not deleted.
- Uncheck the overrides, or collapse Advanced (which suspends their runtime
  effects), to restore the saved preset selection.
- Five editable action targets: **Glory Kill, Chainsaw kill, Ledge climbing,
  Gauss Siege charge, Scripted action**. They start unchecked.
- Sliders have no tick-mark clutter. **Right = faster synchronized action
  (30 Hz); left = slower action (240 Hz)** based on the current experiment.
  A higher tick target does not guarantee higher actual frame rate.
- **Log action events (melee + grenade probes)** is the only read-only
  diagnostic option. Melee and grenade markers are input pulses, not
  confirmed animation start/end boundaries.
- Removed dead controls: weapon reload, enemy animation, boss sequence and
  weapon wheel probes. Their absent per-animation hooks were never usable
  for independent timing; weapon wheel retains its existing slowdown.

## Controlled test

1. Select preset 3 or 5, tick Advanced, then tick **one** action override.
   Confirm all five preset buttons grey out, preserving their selected value.
2. Set the action's slider e.g. 200 Hz, tick **Log action events**, restart DOOM.
3. Perform the action several times and capture `logs/KHARVOX.log`.
4. Disable its checkbox and verify the saved preset bar is enabled. Repeat
   the same action with comparable conditions. Compare individual launches,
   not unrelated 30 Hz and 200 Hz sessions.
5. Repeat for Gauss: *equipping* Gauss must not start an action override.
   Active Siege charge should produce a state edge only while firing.

## Log evidence

- `[ACTION-HZ] ENTER ... target=... Hz`: verified active override.
- `[ACTION-HZ] TRANSITION ...`: action state changed within a native hold.
- `[ACTION-HZ][TEST] ... elapsedMs=... measuredPresentHz=...`: event
  duration and presentation estimate (not unique simulation frames).
- `[ACTION-HZ] RESTORE ... engine minHz=...`: original values restored.
- `[ACTION-HZ][PROBE]`: melee/grenade **input edges**, and the five
  confirmed native action states, with no unsupported animation claims.
- `[CINEMATIC-HZ]`: shared adaptive-tick CVar policy and hold accounting.

## Safety

- Fast Glory Kills and Fast Chainsaw Kills always take precedence and
  preserve the previously verified `sync_noAdaptiveTick` native bypass.
- All action Hz overrides are scoped and experimental. The active
  whole-engine rate can also affect other nearby gameplay; no per-enemy
  animation clock is implemented.
- The game CVars are restored after action/hold exit.
- Keep backup Build #473, and stop any override test that produces a VR
  snap, black frame, discomfort or engine instability.
