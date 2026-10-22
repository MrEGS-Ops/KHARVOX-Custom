# KHARVOX — Five Chainsaw A/B Experiments (Build after #613)

## Where to find them
Enable **Physical Chainsaw · Motion / Haptics / Zones** under VR Mods.
Open **Sync Action Speed → Advanced → Chainsaw A/B Test** and choose
exactly ONE experiment. Click **Launch DOOM** again: the choice is captured
at launch, not changed live. A/B options are radio-style checkboxes. Current
stable **Baseline Free Motion** and legacy **Pointer Focus** remain as
additional controls and are never enabled simultaneously with an A/B mode.

This is scoped exclusively to *confirmed native chainsaw executions*.
Glory Kills, cinematics, ledge climbing, long-fall landing, ammo, fuel,
native damage and camera ownership remain untouched. A second physical
Trigger press at ANY point in the same kill always requests 30-Hz fast
finish, including before an activation circle has been entered.

## Controlled experiment matrix

| Mode | Circle arm | Motion→engine clock | Completion | Haptic signature |
|---|---|---|---|---|
| Baseline (1) | Outside→Inside once | 300–45 Hz via free 3-D hand velocity | 32 cm cumulative sweep, then 30 Hz | Existing cutter rumble |
| X1 Short Single Swipe (4) | Outside→Inside once | 180–45 Hz after arm | 24 cm free sweep then 30 Hz | High-speed cutting + strong completion hit |
| X2 Continuous Throttle (5) | Outside→Inside once | 200–45 Hz, continuously | No distance finish: native exit or second press | Amplitude/pitch proportional to hand speed |
| X3 Three-Phase Hybrid (6) | Outside→Inside once | first ~220 ms at 180 Hz then 300–45 Hz | 28 cm sweep then 30 Hz | distinct engage, cut, and finish feel |
| X4 Physical Impact (7) | Outside→Inside once | steady 155 Hz during cut | 22 cm sweep then 30 Hz | rhythmic bite, strong stroke/impact events |
| X5 Native Fast + Motion Haptics (8) | **none** | 40 Hz for entire native kill, 30 Hz after second Trigger | native exit, no swipe threshold | hand motion changes rumble but not execution clock |

**Hz refers to KHARVOX's scoped DOOM native action tick request, NOT
Quest display refresh or XR haptic frequency.** All physical distances
are tracked 3-D controller-path distances after arming. The circular
activation marker disappears once armed; never needs to be re-entered.
X5 has no ring. We have **not** verified access to a DOOM 2016
per-chainsaw animation playhead or writable animator hook. X3 is a
time-based approximation, not frame-level synchronization.

## User headset test sequence (per mode)

1. Start an execution on an ordinary small demon with fuel available.
   If that variant uses the ring, hold Trigger, move outside it and
   return inside ONCE; confirm ring vanishes.
2. Move the chainsaw in free space, clearly outside where the ring was.
   Try slow motion, medium motion, and one deliberate fast stroke.
3. Stop your hand mid-cut and check how quickly the animation slows.
4. Try releasing Trigger followed by a **second** press during a new
   execution; it must always request 30 Hz without re-arming.
5. Repeat at least two kills, noting *time from hand-finished to visual
   native kill exit*, whether a cut took multiple strokes, and how
   the rumble feels. Repeat for all five options on similar enemies.
6. Test a ledge climb and long landing with the chainsaw merely
   equipped. Those must NEVER take ownership of action-speed timing.

## Logs and interpretation

Every native chainsaw execution records:
- `[CHAINSAW-AB] START` — unique kill number, selected mode ID/name,
  swipe target and whether a circle is required.
- `[CHAINSAW-AB] SAMPLE` — throttled ~130 ms, at most 350 samples/kill,
  with native XR frame, time since native kill entry, phase (0 unarmed,
  1 X3 engaging, 2 cutting, 3 finishing), whether physically armed,
  circle radius (diagnostic ONLY), tracked/Trigger state, hand m/s,
  distance travelled, physical gesture fraction, **requested native Hz**,
  **haptic amplitude/frequency/active**, second press, and verified
  weapon-render observations `renderTimeUnknown` and `renderFrame`.
  Event transitions are recorded immediately instead of waiting 130 ms.
- `[CHAINSAW-AB] END` — duration, sample count, final distance, native
  exit. Does not imply the native animator finished at swipe completion.

Old `[CHAINSAW-EXPERIMENT]` and `[CHAINSAW-ONE-ZONE]` traces also
remain; enable **Diagnostic · Chainsaw Animation Investigator (one run)**
for the separate, bounded `logs/chainsaw-animation-investigation.csv`.
The latter is read-only and has not been repurposed into a guessed
animation-memory write.

### A/B data review
Look for (1) fewer seconds after physical completion,
(2) shortest variance across enemy types, (3) minimal idle/freeze,
(4) realistic on-contact rumble vs floatiness, and (5) absence of
head tracking/camera/ledge regression. The logged engine render counter
appears to reflect accumulated simulation time, **not** an independently
verified per-clip animation position. Never interpret correlation as
proof of exact clip alignment.

### Known limitation
X1, X3 and X4 fast-finish at their physical swipe thresholds but DOOM
may continue its native visual kill frames before releasing the player.
X2's native kill can take longer if the hand remains motionless. X5 is
intended to be the reliable escape hatch when physical motion control
isn't acceptable. Trigger-second-press is the universal manual escape.
