# Object Discovery Mode — native experimental implementation

Development branch: `feature/object-discovery-full-grip-calibration-20261123`

**Runtime validation is pending.** Code and CI compilation alone cannot confirm
DOOM's unknown entity-manager layout, model resource pointers or in-headset
physical release. Never replace the last working experimental build until
headset testing confirms the new runtime.

## Player controls

- Mods > DEV > Experimental Features > Object Discovery Mode is toggleable.
- Scan radius: **1–50 metres** in 1 m increments, default **5 m**.
- Grab reach: **1–5 metres** in 0.5 m increments, default **2 m**.
- Scan and reach are separate and saved in `UserData/ObjectGrip/object_discovery.cfg`.
- Grab Reach Laser: separate default-OFF toggle. A cyan free-hand pointer shows
  precisely the configured 1–5 m reach (not the larger scan radius), and
  tracks the currently selected weapon hand's opposite free-hand AIM ray.
  It remains the configured length even if an object or wall lies before it;
  disabling the visual does not affect scanning or grabbing.
- Each ring has two vertical semicircles: screen-left is the **physical LEFT**
  hand profile, screen-right is the **physical RIGHT** hand profile.
- Each half is GREY when native movement is unverified, AMBER for a
  naturally moved but uncalibrated item, GREEN for a calibrated hand.
- GREY, AMBER and GREEN halves all remain visible throughout the selected
  1–50 m SCAN RADIUS. Saved hand profiles stay GREEN even when the player
  steps beyond Grab Reach. Grab Reach changes only whether a catch is allowed.
- Every frame reads both hand profiles, so saving recolours matching objects
  immediately. One object uses one of the 32 visible marker slots even though
  the GPU may draw two semicircle meshes.
- Native discovery keeps up to 128 recent candidates; the renderer selects at
  most 32 useful visible rings, prioritising objects inside Grab Reach, then
  incomplete calibration, then nearest distance. Scan candidates are
  refreshed with a fixed
  physics-read budget to avoid large per-frame 50 m spikes.
- No hand-attached identification circle, old physics inspector or test menu.

## How an item is caught

1. Only the signature-validated, game-owned world entity registry discovers
   movable items. FocusTracker hand and gun rays DO NOT add discovery hits;
   the free-hand AIM ray is used solely to select a nearby item to catch.
   A strict native RTTI check requires an idPhysics_RigidBody.
2. The game thread samples its native GetOrigin/GetAxis. Native movement is
   proven by observed world-origin displacement, not a forced wake or
   at-rest flag alone. A stale movement observation cannot authorize a grab.
3. With a valid, unobstructed physical free-hand GRIP, only an individually
   moving item within selected reach and aligned with free-hand AIM can
   be caught. Gun bracing/holsters/weapon wheel retain their input priority.
4. An UNSAVED item starts calibration from the neutral hand-local upright
   pose, regardless of its tipped/rolled physics orientation. A SAVED item
   instead loads its existing hand-specific position AND rotation, remains in
   normal gameplay controls, and does NOT open a calibration overlay. Only
   OFFHAND GRIP + OFFHAND TRIGGER deliberately reopens saved calibration
   and resets to neutral. The native follow writes remain bounded to avoid
   a violent teleport when the item is initially metres away.
5. Without offhand Grip: nudge the object's LOCAL position using left
   stick X/Y for lateral/depth and right stick Y for vertical. By default
   each step is 1 cm; HOLD the OFFHAND TRIGGER to make each step 5 cm.
   Releasing that trigger immediately restores 1 cm. The separate weapon-hand
   trigger always remains SAVE and never enables quick mode.
6. HOLD OFFHAND GRIP to freeze the object's complete world-space pose
   (POSITION and ORIENTATION). The object floats in a fixed world position
   while the player freely WALKS and TURNS around it. Moving and rotating
   the free hand does not move the frozen object. On GRIP RELEASE, convert
   the frozen world pose into the exact new hand-local six-degree-of-freedom
   offset/orientation based on the released controller's current pose;
   the object then follows that hand without jumping. The initial grab
   Grip can engage WORLD FLOAT immediately after acquisition.
   No right-stick spin or hand-tilt mode remains. Offhand trigger 5 cm
   fine-tuning and ordinary 1 cm fine-tuning still work with Grip released.
   The DOOM native thread publishes a pose-lock revision, which OpenXR
   acknowledges before applying further stick nudges; this prevents
   one-frame stale calibration feedback from undoing a new lock.
7. While an object is held, offhand equipment/grenade input is owned by
   Object Grip regardless of whether calibration is open. The native grenade
   trigger and delayed throw/cycle pulses are suppressed before XInput
   dispatch; an already armed chest grenade gesture is cleared, so pulling
   the offhand trigger CANNOT spawn a grenade inside the held object.
   With GRIP RELEASED, the offhand trigger is a 5 cm fine-tune modifier.
   Press OFFHAND GRIP + OFFHAND TRIGGER to re-centre and restart the held
   item's calibration from neutral upright/forward, even when it was saved
   previously. One trigger press produces one reset. Neither operation
   replaces the weapon-hand trigger's SAVE mapping.
8. WEAPON-HAND TRIGGER saves; B cancels. The held item is released without
   artificial downward impulse. While WORLD FLOAT is held, DOOM locomotion
   and turning remain active so the player can walk around the object.
   With Grip released, calibration fine-tune owns the movement sticks.
   The weapon-hand trigger remains the exclusive save control.
9. DOOM's original native gravity resumes after a signature-verified
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

## 25 Nov 2026 visual-barrel identification and Grapple Anchor evidence

- Log `KHARVOX-KHARVOX(20261125-094108).log`: **17** separate
  acquisitions of handle **745324524** reported the automatic model name
  **`Uac Barrel Tech Red`**. This is a distinct step forward from the
  earlier log `KHARVOX-KHARVOX(20261125-080821).log`, where **13**
  acquisitions of handle **3462377753** were labelled
  **`Unidentified Movable Object`**. Handles are session-local; matching
  model names does not prove these handles represent the same instance.
- User recalled an earlier generic label sounding like "Icon 2". Neither
  supplied log contains that string. `idProp2` is a plausible native DOOM
  class label mentioned in code, **not a confirmed historical log label**.
- The new human-readable `Uac Barrel Tech Red` comes from the normalized
  verified model-path candidate found on the native movable entity, not from
  collision mesh measurement or actual render triangles. Log the underlying
  complete model asset key when capturing further mesh evidence.
- 17 Grapple Anchor hits in the newer log all used
  `ESTIMATED_RAY_PROXY (NOT_MESH_HIT)`; zero
  `[MESH-INSPECT]` entries existed. This explains why a successful hand lock
  can attach far from the visible barrel. No collision-filtering or exact
  visual-mesh contact has been verified by this evidence.
- Separate **Tech Barrel Outline Preview** switch is OFF by default. It
  performs a finite oriented-cylinder intersection on the identified red
  barrel instead of choosing a point 20 units in front of its physics origin.
  Its 26-native-unit radius and 37-unit half-height are *experimental
  estimates*, not measured model extents. Other items retain the existing
  ray-grapple estimation. Do not conflate this with genuine render mesh.
- The CPU render-triangle intersection kernel is implemented and unit-tested,
  but DOOM's actual static-render vertex/triangle access remains unverified.
  The read-only render-model ownership probe records possible RTTI types
  when the outline preview is used, without making unsafe vtable calls or
  misreporting an approximate hit as an exact one.
- The next proof needed for the real requested feature is a validated native
  render-mesh triangle/asset provider, or a verified scene-depth-based hit
  mapping. No collision mesh fallback may be silently substituted.

## Held-object collision requirement (pending verified native hook)

- While a movable object is attached to either VR free hand, it must NOT
  collide with or push the DOOM player capsule. The held object must remain
  attached if the hand passes through the player body while moving/turning.
- Restore the original player-collision behaviour on deliberate release,
  save/drop, cancel, level transition, tracking failure and game exit.
- Preserve collisions with walls, floors and other world props where
  technically supported. Never disable all clip contents or the player's
  own physics as a shortcut.
- Current limitation: the native rigid body's verified RTTI and its
  GetOrigin/GetAxis accessors are NOT evidence of an independently verified
  player-only collision mask/filter or a safe restoration API for DOOM 2016.
  This requirement is therefore NOT yet implemented; it requires finding
  and validating the exact DOOM player-vs-held-prop contact path first.
  Do not mark this feature complete or distribute it as working without
  successful native compilation and an in-headset player-contact test.

## Scanner safety and limitations

- True proximity scanning first attempts to bootstrap an independently
  enumerable, game-owned native entity registry without pointing the gun.
  It requires DOOM 2016 exact code/data signatures, verified game vtable,
  consistent bounded entity array, and several valid native RTTI records
  including a movable prop. The performance-tuned scan checks 48 entries
  every 175 ms independently of the FocusTracker ray. The old flat
  pointer-pool and both FocusTracker entity detection hooks were deleted
  rather than disabled, avoiding duplicate native entity probes and hook
  trampolines. The 128-entry cache is retained; up to 12 idle
  physics objects are read per world tick (minimum 80 ms between near-object
  reads, 250 ms for distant objects). Failed body probes retry every 800 ms.
  Actively held objects are exempt from every background slowdown and retain
  the full-speed hand-follow update on DOOM's world thread.
- The DEV experimental menu says SCANNER: PROXIMITY ACTIVE when the world
  registry validates, or SCANNER: PROXIMITY UNAVAILABLE when it does not.
  No ray-derived or flat pointer-pool fallback runs. Without a validated
  registry, no discovery markers/grabbing are claimed. Normal VR hand/head
  interaction focus is unaffected by this cleanup.
- The game-thread scanner does not call itself recursively when a native
  enumerated object is observed; the ordinary world tick handles discovery.
- Active held-object ownership keeps the native scanner and hand input live
  even when the original interaction focus ray moves elsewhere. The native
  controller input retains the last validated pose for up to 300 ms during
  a transient XR tracking gap, and grip-off uses a short 150 ms debounce
  outside calibration. Explicit release, level transition, lost gameplay,
  or cutscene still release promptly. Up to three failed native pose reads
  are retried without writing unverified body positions.
- A slightly stronger, bounded follow (40 native units/tick) keeps up with
  normal hand movement and a 5 m reach; native body velocity is never
  artificially injected. A much larger unsafe offset still fails closed.
- The world-registry scanner traverses bounded native slots read-only,
  rejects invalid memory/RTTI and never invokes unknown virtual functions
  to enumerate. If the verified registry is absent or becomes invalid,
  discovery fails closed; there is no legacy fallback. In-headset validation
  of full 360-degree discovery remains necessary after this cleanup.
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

## 25 Nov 2026 cleanup — object grab single path

- Retired the six-switch Smart Grip experiment and its corresponding code,
  settings loader, preview controls, and superseded tests. The existing
  `smart_grip_experiments.cfg` file is left untouched on disk for rollback;
  current builds no longer load it.
- Retired the UAC barrel-only outline preview and its hard-coded cylinder.
  Old `grapple_anchor_v2` settings are read, but the obsolete barrel flag is
  deliberately ignored; future writes use `grapple_anchor_v1` (one enabled bit).
- Removed duplicate speculative `MESH-INSPECT` and `RENDER-MESH` memory
  scanning routines from the active native game-tick path. Retain bounded
  model naming, acquisition evidence and native release diagnostics.
- Object Discovery remains the ONE proximity/eligibility scanner and the
  manual six-DOF profile-calibration path is preserved. Grapple Grab remains
  the ONE alternative 6DOF laser attachment path. Both use the same DOOM
  rigid-body follow and release routines. There is no per-object shape lookup.
- The universal ray-triangle render-mesh intersection kernel remains for
  eventual integration with a **verified** DOOM render triangle provider.
  No engine triangle provider has been verified; existing runtime grapple
  still uses the origin-ray proxy instead of falsely claiming visual contact.
- Last green rollback before cleanup: RIPSHIFT Experimental Build #110,
  commit `8ce38ac54b2631e592d4f71c95549e4459572364`.

## Deferred cleanup: retire manual object-grip calibration after universal surface grabbing is proven

- **User decision (25 Nov 2026):** If universal visual/render-mesh grabbing works reliably for *all eligible movable objects*, manual object-grip calibration is no longer wanted and should be removed in the **next cleanup**, rather than retained alongside it.
- This is **conditional**, not a request to remove calibration from the current build. Do **not** retire existing right-trigger save, per-object hand profiles, free-floating grip calibration, or associated settings until universal visual-surface pickup actually works on varied shapes/sizes and passes normal pickup, hand-relative rotation, release and regression checks.
- Once the replacement is demonstrated, remove the redundant calibration UI, code paths, profile-management logic, obsolete tests and dead configuration readers together, while preserving a safe rollback/migration policy for existing UserData. Keep coloured discovery rings, scan-radius and grab-distance controls, cyan reach laser, object identification, and normal grab/release mechanics.
- Surface-contact targeting is distinct from user-authored pose presets. If the user later requests fixed poses (e.g. a mug automatically oriented to drink), treat that as a separate feature decision rather than silently preserving the entire manual calibration subsystem.
