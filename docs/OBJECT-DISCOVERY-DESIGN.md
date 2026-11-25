# Object Discovery Mode — native experimental implementation

Development branch: `feature/object-discovery-full-grip-calibration-20261123`

**Runtime validation is pending.** Code and CI compilation alone cannot confirm
DOOM's unknown entity-manager layout, model resource pointers or in-headset
physical release. Never replace the last working experimental build until
headset testing confirms the new runtime.

## Player controls

- Mods > DEV > Experimental Features > Object Discovery Mode is toggleable.
- Scan radius: **1–15 metres** in 1 m increments, default **5 m**.
- Grab reach: **1–5 metres** in 0.5 m increments, default **2 m**.
- Scan and reach are separate and saved in `UserData/ObjectGrip/object_discovery.cfg`.
- Grab Reach Laser: separate default-OFF toggle. A cyan free-hand pointer shows
  precisely the configured 1–5 m reach (not the larger scan radius), and
  tracks the currently selected weapon hand's opposite free-hand AIM ray.
  It remains the configured length even if an object or wall lies before it;
  disabling the visual does not affect scanning or grabbing.
- Grey world-space ring: candidate with no confirmed real native movement.
- Amber ring: object physically moved in DOOM's own simulation; no saved grip.
- Green ring: matching verified model profile or current-level profile exists.
- Every frame reads current profile state, so saving immediately recolours
  all rendered objects of the same verified model resource.
- No hand-attached identification circle, old physics inspector or test menu.

## How an item is caught

1. DOOM resolves a native world item or the read-only nearby pool scanner
   finds it. A strict native RTTI check requires an idPhysics_RigidBody.
2. The game thread samples its native GetOrigin/GetAxis. Native movement is
   proven by observed world-origin displacement, not a forced wake or
   at-rest flag alone. A stale movement observation cannot authorize a grab.
3. With a valid, unobstructed physical free-hand GRIP, only an individually
   moving item within selected reach and aligned with free-hand AIM can
   be caught. Gun bracing/holsters/weapon wheel retain their input priority.
4. The item is held in controller-relative six-degree-of-freedom pose.
   An unconfigured item opens a small calibration overlay immediately.
5. Without Grip: nudge object 1 cm per step using left stick X/Y for
   horizontal/depth and right stick Y for vertical. Doom movement/turning is
   suppressed while calibrating. With Grip: the object stays hand-anchored;
   its world orientation remains fixed while the controller rotates, changing
   the object's relative orientation without dropping or detaching it.
6. RT saves; B cancels. The held object is released without an artificial
   downward impulse. A while holding an already-profiled item reopens
   calibration.
7. DOOM's original native gravity resumes after a signature-verified
   momentum clear and Activate *only for an item that actually moved
   before the grab*. Nothing can be activated by scanning or Grip alone.

## Model identity and UserData

- The game-thread scanner only accepts unique and validated native
  `models/...` resource paths with known model extensions. A generic
  class, entity handle or raw pointer is not accepted as a persistent key.
- Automatically generated names derive from the model's filename.
- A distinct LEFT and RIGHT controller profile saves offset and local
  quaternion in `UserData/ObjectGrip/object_grip_profiles.cfg`.
- When native model identity cannot be proven, the item can still be
  calibrated for this level, but the exact instance-only profile is
  intentionally **not persisted across restarts**. The UI explains this.
- An unknown model that has moved is amber, not falsely green.
- These are disposable development configs: no migration, no flat-path fallback.
  Existing flat files are left untouched and ignored by the current build.

## Scanner safety and limitations

- Native FocusTracker read-only hooks accumulate valid physics candidates.
- The separate nearby scanner attempts to identify the native entity-pointer
  pool via the signature-verified manager pointer and an already resolved
  entity. It traverses bounded pointer entries read-only, rejects invalid
  memory/RTTI and never invokes unknown virtual functions to enumerate.
- If pool layout cannot be validated, the system falls back to ray-discovered
  props; therefore complete 360-degree radius enumeration is **not yet
  confirmed**.
- The visual rings and optional cyan reach laser use a separate late Vulkan
  overlay pipeline with depth testing and writes disabled so they stay visible
  over props and world meshes.
  DOOM's native materials, object meshes and depth buffer are not modified.
- Native body pointers never cross into the XR thread; immutable snapshots,
  owner checks, generation, vtables and input age gates protect handoffs.
- Pausing, losing tracking, switching the mode off or entering cutscenes
  releases a native object on DOOM's world thread.
- The old automatic activation-stage scanner and intrusive physics tests
  must not be reintroduced.

## Acceptance gates

1. Windows compiler and policy tests pass on the experimental branch.
2. The `RIPSHIFT-Experimental` ZIP packages the
   matching experimental native DLL and pinned standalone manager.
3. In headset, ensure scanner detects candidates automatically as the
   player runs near them and doesn't wake stationary props.
4. Verify handedness, 1–5 m reach, calibrated offset and rotation, profile
   load/save, instant amber-to-green and natural gravity release.
5. Inspect native logs and recorded headset footage for unexpected
   movement, loss-of-tracking issues, false greens or render depth errors.
