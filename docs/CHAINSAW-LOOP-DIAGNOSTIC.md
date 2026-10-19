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

- Kill 1: normal single-swipe cut.
- Kill 2: arm inside the oval, pause hand movement mid-cut for >= 0.5 s,
  then resume without releasing Trigger.
- Kill 3: slow, deliberate swipe across the oval and back out.
- Kill 4: quick single swipe, keep Trigger pressed and hand stationary.
- Kill 5: release Trigger partway if DOOM allows; then do one uninterrupted cut.
  Also test pulling the chainsaw out with no kill, and ordinary ledge climbing
  (negative controls). These should not produce a chainsaw execution session.

## Data captured automatically
`[CHAINSAW-LOOP-PROBE] CONFIG/TESTS/START/SAMPLE/HAND_STATIONARY/
HAND_RESUMED/SWIPE_COMMITTED/END` in the runtime **logs** directory,
`KHARVOX-KHARVOX.log` or the launcher’s copy of KHARVOX logs.

At a max of one SAMPLE every 50 ms (350 per kill), captures: local monotonic
elapsed milliseconds; XR frame and display period; native execution/sync/cutscene
flags; trigger analogue pressure; tracked controller coordinates; oval overlap;
hand-speed estimate; swipe distance and commit; requested engine Hz; XR headset
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

## Safety / rollback
The checkbox only enables structured logs, not additional ticks, new hooks,
memory scanning or writes. All existing movement, hands, weapon input and
sync action speed mods continue unchanged. Logs have finite 350 samples per
kill; turn OFF after capturing evidence to avoid excess disk usage.
