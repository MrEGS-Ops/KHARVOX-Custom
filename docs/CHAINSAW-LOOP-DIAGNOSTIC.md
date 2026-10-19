# KHARVOX — Chainsaw animation loop feasibility, one-run diagnostic
Status: **read-only instrumentation, NOT an animation-loop mod.**

## Why
The current physical chainsaw experimental control adjusts game engine tick Hz.
It does **not** access idAnimator playback position. Our 300/480 Hz stall is
not a genuine pause, and changing the Hz is not a rewind. Before attempting a
repeatable 150–250 ms loop we need evidence of a safe dedicated chainsaw
animation clock / playhead, and any linked execution, damage, sound and camera
events that would need to be prevented from refiring.

## Activate once
1. Install this new build in the same KHARVOX runtime folder as usual.
2. Launch KHARVOX launcher; under Sync Action Speed → Advanced, enable
   **Diagnostic · Chainsaw Loop Feasibility (one run)**. Leave the working
   Physical Chainsaw settings, oval and controller mapping unchanged.
3. Launch DOOM, keep the chainsaw equipped, then complete the set below
   **in one gameplay session** (no restart or different configs).
4. Turn the checkbox OFF after the session and restart DOOM.

## One-session test matrix
Do a few kills with short gaps between each. KHARVOX auto-detects and numbers
each verified native chainsaw execution; it DOES NOT automatically perform a
kill or control your headset.

- Kill 1: hold first Trigger and cross circle RIGHT edge to LEFT edge.
- Kill 2: enter circle, pause halfway >=0.5s, then reach opposite edge.
- Kill 3: enter, backtrack / move sideways, then cross the original chord.
- Kill 4: release and re-press Trigger BEFORE ever entering the oval;
  target should become 30 Hz immediately.
- Kill 5: enter, move halfway, release Trigger, re-press anywhere for 30 Hz;
  on a new execution the counter resets and physical cutting needs its
  own initial outside-to-inside entry.
  Also test pulling the chainsaw out with no kill, and ordinary ledge climbing
  (negative controls). These should not produce a chainsaw execution session.

## Data captured automatically
`[CHAINSAW-LOOP-PROBE] CONFIG/TESTS/START/SAMPLE/HAND_STATIONARY/
HAND_RESUMED/SWIPE_COMMITTED/END` in the runtime **logs** directory,
`KHARVOX-KHARVOX.log` or the launcher’s copy of KHARVOX logs.

At a max of one SAMPLE every 50 ms (350 per kill), captures: local monotonic
elapsed milliseconds; XR frame and display period; native execution/sync/cutscene
flags; trigger analogue pressure; tracked controller coordinates; oval overlap;
hand-speed estimate; stroke distance, input chord progress and opposite-edge commit; requested engine Hz; XR headset
position; read-only DOOM-authored cinematic camera source **if known**.

Each END summarizes real kill wall-time, sample count, hand-stationary windows,
hand-resume windows, swipe commitment, camera-source observations, and tracking
loss. All of this is observational. `HAND_STATIONARY` is NOT proof that the
animation froze. Camera translation is NOT animation time. The current code
does NOT access a confirmed animation playhead.

## How we decide whether native looping is possible
Read the collected sequences to identify clean repeated visual/body/camera
segments and their durations. Before shipping any looping code, locate and
verify one positively identified native chainsaw-specific animator/playhead
and test event ownership in isolation. If no such playhead is verified,
report **not yet feasible** rather than guessing DOOM offsets or touching
global game time. Damage, audio, fuel, camera, death and level transitions
must NOT repeat or stick.

## Physical chainsaw: directed edge-to-edge stroke and skip-anytime Trigger

The first Trigger press starts DOOM's normal native chainsaw execution.
While holding it, move OUTSIDE the enlarged projected oval, then enter.
KHARVOX locks a single direction from the entry edge directly toward the
OPPOSITE visible-ring edge. Only forward movement along that narrow chord
drives requested engine tick pace: quickest physical motion requests **45 Hz**.
Stopping, reversing, leaving the chord sideways, or losing tracking slows
toward the idle target; none of those movements completes a gesture.

Crossing the opposite VISIBLE edge on the originally locked chord commits
the input gesture and latches **30 Hz** until DOOM ends that native execution.
This replaces the old arbitrary 25cm cumulative-travel threshold. Releasing
Trigger after physical completion does NOT cancel the 30Hz finish.

If the first stroke is not yet complete, releasing Trigger after arming
holds **300 Hz** while the same native kill remains active. **Any second
Trigger press, anywhere during that native kill (even BEFORE the oval was
entered, and even when the hand is untracked), immediately latches 30 Hz**.
The first Trigger that launched the native kill never counts as a skip.
Further press/release cannot unset it; native exit resets all latches.

The native animator/playhead remains UNRESOLVED. The chord's INPUT progress
is NOT animation-frame progress. Moving the hand across the entire ring
does not literally seek from first to last animation frame; finishing the
chord accelerates the remaining native frames through the global scoped
action-clock override. This is a best-supported first iteration toward a
spatially deliberate cut. No unsafe guessed native animator memory writes
or rewinds, no changes to other actions, fuel, camera or damage.

## Outside-the-circle pulsing guidance (weapon hand only)

The physical Circle mode increases the visible oval and the *same* 3D
controller projection capture region ~15% (quad 1.50m x 1.98m vs 1.30m x
1.72m, rendering at the unchanged normalized ring radius .69).

During a **confirmed native chainsaw execution**, the weapon controller is
**completely silent while INSIDE** the visible ring. As soon as the hand moves
OUTSIDE the drawn ring, it receives repeated clearly separated buzz bursts:
**150 Hz haptic frequency, 92% amplitude, 160ms ON / 200ms OFF**. Each outside
entry begins with a burst, with pulse phase anchored at that exit event.
Moving back inside stops it immediately; unavailable/invalid tracking is
also silent, never falsely "outside". The guide starts at native execution
entry even before the first oval arming gesture, so the pulse indicates
the starting side of a deliberate reach-and-cut.

The guide replaces old sustained cutting vibration and the previous
one-shot ring-entry/chop pulse in Circle mode to prevent masking its silent
inside region. Controller haptic pulses use the existing OpenXR output
writer; genuine damage still takes precedence, and pointer mode, other hand,
bHaptics, idle weapon haptics, native game execution and action timing remain
unchanged. Log [CHAINSAW-ONE-ZONE] gives guideOutside / guidePulse states.
Actual physical pulse sensation still needs testing in the headset.

## Safety / rollback
The checkbox only enables structured logs, not additional ticks, new hooks,
memory scanning or writes. All existing movement, hands, weapon input and
sync action speed mods continue unchanged. Logs have finite 350 samples per
kill; turn OFF after capturing evidence to avoid excess disk usage.
