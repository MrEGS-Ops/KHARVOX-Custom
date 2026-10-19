# KHARVOX — Chainsaw Animation Investigator v2 (ONE-RUN READ-ONLY)

**Goal:** Discover a repeatable, independently changing *native animation
progress proxy* and, if the game's supported binary exposes one, ultimately
identify and validate a dedicated animation playhead. A proxy score is NOT
proof. This instrument does not run speculative memory scanners, dereference
new game object pointers, change hook signatures, modify DOOM memory, seek
animation frames or synthesize damage. Its observations come only from the
existing validated renderer, weapon, camera, controller and timing hooks.

## Enable and collect
1. Install the latest KHARVOX build. Launch the standard launcher.
2. Sync Action Speed → Advanced → **Diagnostic · Chainsaw Animation
   Investigator (one run)**. Keep the physical chainsaw mod enabled; enable
   Show Cutting Circle to compare actual chord position. Restart DOOM.
3. Execute the test sequence **in a single session**. Leave the advanced
   diagnostic on only during collection.
4. Retrieve `logs/chainsaw-animation-investigation.csv` plus
   `logs/KHARVOX-KHARVOX.log` and the normal
   `KHARVOX-NATIVE-STEREO...`/crash logs. Turn the checkbox off afterward.

The normal log now contains `[CHAINSAW-ANIM-INVESTIGATOR] CONFIG / START /
CHORD_ARM / OPPOSITE_EDGE_COMPLETE / SECOND_TRIGGER_FAST_FINISH /
TRIGGER_RELEASE / CLOCK_TRANSITION / CANDIDATE / RANKED_PROXY`.
The CSV contains synchronized time series with engine-provided renderer values,
source freshness, hand/oval/chord state, native-action/sync classification,
XR frame+period, headset translation and requested game timing.

**Cinematic coverage:** the diagnostic temporarily broadens READ-ONLY capture
at the already-verified weapon-render and weapon-prop callbacks when the
chainsaw is the selected weapon and a native world/cinematic scene is active.
This is cache publication only; normal gameplay-only VR pose modifications
and the render submission path remain exactly as before. Missing samples are
explicitly marked invalid rather than fabricated.

**Bounds:** one baseline sample every 200 ms, up to 120 baseline samples;
up to 1200 samples per kill at >=25 ms intervals, maximum 12 native kills.
Events are collected immediately even between periodic samples. The file
flushes at boundaries and periodically, improving partial recovery on crashes.

## Six tests that disambiguate animation from movement
* **Control:** Hold the chainsaw equipped in normal gameplay for 3–5 seconds
  *without* initiating a kill; look and move the controller. This establishes
  which renderer candidates change even without a chainsaw animation.
* **Kill A — stationary:** Start a chainsaw execution, keep hand stationary
  for 0.5–1.0 seconds with Trigger held before entering the oval. Observe
  whether render time, weapon model, and grip joint keep advancing.
* **Kill B — slow chord:** Enter at one visible edge and deliberately move
  toward the other edge with two pauses. Compare target engine Hz, camera
  motion, render time, native weapon transform and joint movement.
* **Kill C — quick chord:** Cross the exact opposite edge in one swift
  movement. The gesture should latch the 30-Hz finish at the last edge.
* **Kill D — early skip:** Release/re-press Trigger immediately after native
  kill begins *before entering the circle*. Confirm a 30-Hz transition.
* **Kill E — mid-cut skip:** Enter the circle, reach halfway, release Trigger,
  then press again without oval re-entry; verify 300→30 Hz and native exit.
* **Kill F — negative controls:** Climb a ledge, take a long fall, and do a
  Glory Kill with the chainsaw equipped when possible. These must NOT open
  a native chainsaw investigation session solely due to camera changes.

## Current candidate signals (automatic differential ranking)
1. **Native weapon-render time** — the int passed into the verified native
   weapon-render update. Units and relationship to animation time are unknown.
   A steady increase may indicate a GLOBAL render clock, not a chainsaw clip.
2. **Weapon presentation origin** — final drawn weapon transform, which
   KHARVOX may modify for VR and thus can track the controller (confounder).
3. **Native prop origin** — actual DOOM weapon-prop entity transform already
   observed by the current weapon hook, with freshness checking. Still
   includes global camera/weapon-body motion.
4. **Animated hand-grip joint** — proven joint lookup from existing idHands
   hook; actual joint-space deformation can be useful, but this pivot is
   NOT confirmed as the chainsaw blade's animation and may be stabilized.
5. **Cinematic camera source** — read-only, previously verified camera origin.
   May track executions but camera can hold while blade animation progresses.

Each candidate receives valid-pair counts, changes with physical hand still
vs moving, changes during slow/high engine-Hz requests vs fast/low requests,
baseline changes outside kills, clock reversals, data discontinuities, and
a **heuristic proxy score**. Output reports the highest-ranked candidate
per kill. NEVER interpret the ranking as proof that a native animation
playhead or safe setter exists. The CSV supports independent cross-run
correlation and offline nonlinear curve fitting.

## Criteria before introducing real frame-based control
Verify a candidate against at least three repeated **same-type enemy**
chainsaw executions with different controller paths. It must advance with
DOOM's visible chainsaw clip even while the hand is still, preferably
correlate with a weapon-joint or optical signal independently of HMD/hand,
exhibit boundaries aligned to native execution start/end, differentiate
fast from slow clock requests, and reset predictably between executions.
Next inspect native animator ownership and event callbacks without using
unverified game binary offsets. Only consider a write hook after separate
guards for camera, damage, hit events, fuel, audio, and mission progression.

## Alternative routes researched (ordered by usefulness)
**1. Extract actual DOOM (2016) assets offline**: Revenant advertises
Doom 2016 animation extraction to Cast, and DOOMExtract offers resource
extraction/repacking. Inspect chainsaw execution variants, clips,
event markers, and duration to understand exactly which portion should
map to the entry/exit chord. This does NOT reveal a runtime playhead.
References: https://dtzxporter.com/tools/revenant and
https://github.com/emoose/DOOMExtract .

**2. Match named native execution clips/events**: The Doom 2016 Glory
Kill Overhaul mod uses extra/unused chainsaw kill transitions, suggesting
some kill variants are exposed through asset/event definitions.
Track clip identity and timestamps rather than guessing an idAnimator
instance. Source: https://www.nexusmods.com/doom/mods/23 .
Animation-tool/editor documentation about MD6 (not direct evidence of
a DOOM 2016 runtime API): https://idstudio.idsoftware.com/animation/model-editor/index .

**3. Visual-motion proxy from existing capture**: compare a small chainsaw
blade/weapon image region across already-captured left-eye frames while
correlating physical stillness and engine Hz; use CPU/offline optical flow
instead of new VR render callbacks or full-frame readbacks each game frame.
Optical motion can indicate animation without exposing game memory, but
weapon camera movement and FX must be masked.

**4. Stable joint-track proxy**: With independently validated named
chainsaw attachment/joint and known model owner, use the game's already
verified joint getter *from its valid render-thread callback* to sample
a blade-relative transform and infer normalized phase. A live per-bone
getter is still not an animator setter. Do not start unbounded joint scans.

**5. Predictive chord retiming (no playhead)**: Extract approximate per-enemy
clip duration and continuously request engine Hz from *remaining visible
chord fraction* and predicted native time left, with a bounded catch-up
phase at the far edge. This approximates a spatial cut and avoids trying to
rewind or lock global time to frozen controller motion.

**6. Segment/event-gated camera/haptics**: Instead of forcing a game animation,
map physical-entry/centre/exit chord events to chainsaw contact rumble and
existing-safe engine clock windows. This improves tactile causality even
where native animation is locked to a predetermined script.

### Important limits and safety
- No guarantee an accessible, writable per-chainsaw animation playhead
  exists in this executable.
- Existing engine-Hz requests are not XR headset frequency, and
  candidate render time is not verified animation time.
- This diagnostic does not modify the chainsaw's new directed-edge gesture,
  45-Hz physical limit, 300-Hz released-trigger hold, 30-Hz second-trigger
  fast finish, outside-only pulsing haptics, collision, or damage.
- It is limited to supported existing cache owners and must not become
  a generic memory scanner or uncontrolled multi-gigabyte capture.
