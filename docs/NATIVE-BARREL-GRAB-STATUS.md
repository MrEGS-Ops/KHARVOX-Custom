# DOOM (2016) native world-object grab — implementation ledger

## Goal: physics properties, not specific meshes or barrel types
- Free-hand controller AIM ray (up to 0.90 m) or short-range hand contact/overlap finds an actual DOOM collision hit.
- Resolve the hit entity and its physics body on DOOM's game thread.
- Allow any verified, non-static, non-kinematic, movable **dynamic physics body**.
- Exclude player, characters, projectiles, scripted objects and unknown/stale handles.
- Valid candidate -> red aim laser; unknown/static/non-grabbable -> cyan aim laser.
- Fresh free-hand Grip -> select one movable object and allow native-Vacuum-style travel to DOOM's unmodified collection destination. The hand geometrically INTERCEPTS the object's measured collision surface; then Catch/AnchorToHand stops Vacuum without collecting it. Trigger after Catch -> grenade-derived launch; Grip release -> Drop. Physics must remain collision-owned.
- Never require a fixed mesh, entity name or barrel-specific whitelist.

## Confirmed from game data already examined
- DOOMx64vk.exe contains the registration string g_dragEntity and native class names idDragEntity, idProp2 and idProp_Moveable.
- The envhazard/barrels/red declaration uses idProp2 with physicsAttributes="barrels" and isStatic=false, but class/declaration strings are **not** the eligibility criterion.
- RIPSHIFT's FocusTracker trace hook at RVA 0xD48938 modifies an interaction ray. This does not yet establish the engine's generic collision hit output.

## User's DOOM-native-physics-report.txt — verified 17 Nov 2026
- The owner's on-disk `DOOMx64vk.exe` measures **101,529,088 bytes** and has **11 PE sections**. Those values matched the earlier read-only trace dump's EXE size.
- Confirmed actual `CALL rel32` instructions: RVA `0xD48ABF` -> `0x1637CE0`, and `0xD48B30` -> `0x1637CE0`. The destination is a trace-request builder/queue, **not** a resolved collision hit.
- On-disk class-name string and simple RIP-relative LEA code references:
  - `g_dragEntity` string RVA `0x2222208` -> code `0x122136`.
  - `idProp_Moveable` string RVA `0x240C420` -> code `0x1B4224`, `0x1344D54`.
  - `idProp2` string RVA `0x246C570` -> code `0x1CDB34`, `0x13C2374`.
  - `idPhysics_RigidBody` string RVA `0x264DB58`, `idPhysics_RigidBody3DOF` RVA `0x26E58D0`, `idDragEntity` RVA `0x263F048`, `ApplyImpulse` RVA `0x252C979`. The original *simple* LEA scan found no references to these latter strings; **that does not prove absence of a function**.
  - `physicsAttributes` string RVA `0x254C5C0`; `isStatic` RVA `0x254C5D8`. Neither is a confirmed native property offset in a live object.
- `tools/Inspect-DoomWorldPhysics.ps1` was advanced to dump RVA-labelled code windows around the precise reference sites, inspect REX extended-register LEA references and search data sections for exact absolute-VA RTTI references. This is offline analysis, not engine modification; no fresh output has yet been evaluated.

## v3 deep executable report — stronger native entity lookup identified
- The owner supplied `DOOM-Physics-DEEP-v3-REPORT.txt`, generated from their same 101,529,088-byte DOOMx64vk.exe; it contains enough machine-code bytes to disassemble the FocusTracker entity-lookup path.
- After its two native trace requests, FocusTracker tests candidate handles and follows a DOOM entity-manager pointer stored at **RVA 0x5B0F6D0**. This shared manager slot is confirmed by the RIP-relative memory loads at **RVA 0xD48B60** and **0xD48BE8**.
- At those sites, DOOM passes a pointer to a stack-held candidate handle (primary `[rbp+0x6C]`, secondary `[rbp-0x24]`) into the engine's own virtual call at offset **0x7F8**. It then tests the returned object for null and invokes a further virtual predicate at offset **0x600**. The predicate's semantics and whether movable barrels are returned remain **unverified**.
- The `idProp_Moveable` code references near `0x1B4224` and `0x1344D54` are class registration/reflection-related routines, **not** identified runtime physics movement functions. The `idProp2` reference family shows a similar pattern.
- `idPhysics_RigidBody` and `idPhysics_RigidBody3DOF` have multiple static absolute-VA references in the data sections, consistent with class metadata; these do not by themselves reveal a safe physics impulse ABI.
- `CameraHook.cpp` now signature-checks **both exact 14-byte native instruction spans**, observes candidate handles **before** the engine's entity lookup/predicate, and restores its original RCX/RDX/RAX inputs for DOOM's normal execution. It never calls the unknown virtual methods itself and does not dereference the returned entity pointer.
- DEV Experimental now reports `[WORLD-GRAB-NATIVE-HIT]` with the path, numeric candidate handle and level generation; no arbitrary DOOM pointer leaves the native thread. This is a *real native candidate observer* but not a validated movable physics target.
- A no-game-binary CI test verifies both signature bytes and their common global slot `0x5B0F6D0` to detect accidental hook regressions. The machine-code observer still requires Windows CI and in-game verification before being described as working.
- **No more offline report is requested** from the owner for this native-candidate stage.

## Native trace reverse engineering — confirmed from earlier uploaded byte dumps
- Recovered `KHARVOX-TraceBytes.log` and `KHARVOX-TraceResults.log` (read-only copies of the user's DOOMx64vk.exe machine-code windows).
- FocusTracker has **two** direct relative CALL instructions to RVA `0x1637CE0`, at RVAs `0xD48ABF` and `0xD48B30`. The destination is a native **trace request queue/builder** (not a verified generic raycast result getter); its entry signature begins `48 89 5C 24 18 48 89 74 24 20 41 56`.
- On return the FocusTracker stores opaque request tokens at offsets `+0x100` and `+0x108`. Do not treat these as entity pointers or physics handles.
- FocusTracker's own helper at RVA `0xD481E0` reads `[tracker+0x68]`, `[tracker+0x6C]` and `[tracker+0x70]` while maintaining a selected target. **These are native selection-state fields, not validated hit physics.**
- The existing signature-verified FocusTracker hook at RVA `0xD48938` provides that same tracker pointer while executing on the DOOM thread.
- `CameraHook.cpp` now captures only those already-read opaque selection fields in DEV Experimental mode, without following the reference, calling unidentified vtable entries or touching physics. `CameraHook.h` provides a synchronized read-only snapshot to `OpenXRBootstrap.cpp`; DEV logs changes at a throttled cadence.
- **What remains unknown:** whether a loose physics barrel ever appears in FocusTracker selection; the generic collision query's resolved result layout, object's idPhysics interface, and a signature-verified impulse method.

## 17 November native physics-gun probe — executable code, not Vacuum simulation
- Implemented a signature-checked, game-thread observer at **RVA 0xD48B7C** and **RVA 0xD48C04**, after DOOM's own FocusTracker entity resolver has returned a non-null entity and before DOOM's normal virtual interaction predicate. Both windows displace the same exact 14 bytes (`48 8B 10 48 8B C8 FF 92 00 06 00 00 84 C0`); replaying the bytes in each trampoline preserves DOOM's native method call and `AL` predicate result.
- The handler reads the native entity's existing vtable value (which DOOM is about to read anyway), publishes **only an image-relative vtable fingerprint** and current-level candidate handle; live entity pointers are never handed to OpenXR and are not treated as physics bodies. Signatures are included in automated build checks.
- New `[WORLD-GRAB-RESOLVED]` DEV log differentiates `PRIMARY` and `SECONDARY`, showing handle, vtable RVA, image membership and level generation. It is deliberately throttled. The original `[WORLD-GRAB-NATIVE-HIT]` log remains available.
- Fixed a real input wiring gap: `OpenXRBootstrap.cpp` previously published the FocusTracker hand ray from the **weapon hand** only (or left it at native head focus) even while the DEV physics-gun laser was on the other hand. While an eligible free-hand Grip is held, the existing native world-interaction ray is now aimed from the actual free hand. Releasing Grip, entering menus, bracing weapons, or starting scripted actions restores original Hand Focus behaviour. VR weapon firing and aiming remain unchanged.
- This is native **entity detection and hand-targeting**, not true **generic rigid-body physics selection**: FocusTracker's ordinary interactable filter may still exclude barrels. A game-thread collision query capable of selecting ANY dynamic body still requires engine verification.
- No object is pulled, frozen, teleported, launched, reclassified or mutated by these probes. The Vacuum rune and ordinary grenade controls are untouched. The present hand-follow spring and grenade-ballistics structures are **intent policies only**, not a native adapter.

## Native Vacuum attraction, item-spec contact catch — owner's corrected design
- The free hand is a **selector and interceptor**, NEVER a Vacuum target or a directional attraction field. On a laser hit, Grip assigns one **already native-movable physics object**, by physics body identity.
- Temporarily simulate/borrow only the *item attraction behaviour*. **DOOM's existing native Vacuum/pickup destination remains unchanged.** The object travels along DOOM's original attraction route as a normal vacuumable item would.
- Do **not** redirect attraction toward the gripping hand; no hand-tether, spring-follow, gravity-gun beam, or fixed stopping distance.
- Derive the catch point mathematically from that specific item's actual collision shape and orientation. Compute the **closest distance between the hand contact point and the selected object's native collider surface**. Example: a face-on 1-metre-wide box has a centre-to-face extent of 0.5 metres, so the box reaches the catching hand when its **centre is still 0.5 metres away**. Other shapes, orientations and sizes yield different stopping offsets.
- The hand **intercepts** the moving object as its surface reaches the hand. Only at this contact does the native adapter stop/suspend attraction and hold the caught object. If the object does not intersect the hand, the hand must not alter DOOM's native Vacuum destination.
- Before activating native attraction, the adapter must positively verify the selected **dynamic body**, its native collider dimensions and coordinate transform, and an **item pickup-consumption suppression mechanism**. If native collection cannot be blocked, **do not activate Vacuum for that object**; otherwise an unmodified pickup could disappear or alter inventory.
- The object retains its original class, physical mesh, collision rules and barrel-specific effects. It never becomes a health pickup, ammo pickup or grenade.
- Trigger launches **only a caught object**, via verified DOOM grenade launch ballistics on the same existing rigid body. Grip release stops temporary attraction/hold and returns the body to normal; losing tracking, level changes or weapon support do the same.
- `WorldGrabMotionPolicy.h` now exposes an **item-sized oriented bounding-box catch test**, using measured body half-extents, rotated collider axes, and the free-hand contact position in the same metre-space. A small 0.015-metre geometric contact tolerance avoids exact floating-point contact errors; it is **not** an attraction radius.
- The native adapter still does **not** exist. The state/geometry policy and pure regression tests are a source-level contract, not a claim of working native Vacuum, collider discovery or playable object grabbing. Real DOOM's Vacuum rune may only expand pickup range; its physical movement/consumption internals remain unverified.

## Caught-object native hand anchor — latest revised owner decision
- No **idDragEntity / g_dragEntity** dependency is required for holding. The object first travels along DOOM's **original** Vacuum/pickup route. The gripping hand only **intercepts** its measured collider surface, at an item-specific centre offset (e.g. a 1 m box is caught at 0.50 m centre-to-hand distance face-on).
- At contact, temporarily suppress Vacuum movement and native collection and **anchor the original existing physical body to the hand's game-world transform**, preserving its exact offset and orientation. While Grip is down, move that same body using the saved relative pose. No new object, fake grenade, hand-directed suction or spring impulse.
- `WorldGrabMotionPolicy.h` now computes the relative object-centre and body-axis transform in hand coordinates on Catch, then emits `AnchorToHand` with a recomputed world-space desired pose on subsequent frames. Rotating/moving the hand maintains the 0.50 m face offset without snapping the box's centre into the controller.
- Grip release, hand tracking loss, weapon-brace conflicts or level transitions demand **detach + restore**. Trigger only after Catch demands detach then native grenade-style launch on that same object.
- **Critical implementation gap:** no verified DOOM (2016) generic prop Bind/Unbind, SetOrigin/SetAxis or kinematic-body transform ABI is yet identified. The anchor output is a tested **math/engine-command contract**, NOT a real native object movement connection. Attaching only the rendered mesh would leave the rigid body and collision behind, so do not do that.
- The existing player-physics origin setter was retired due teleportation defects and must not be repurposed for props. Native dynamic-body identity and a validated writable transform path are still required.

## Grenade-derived trigger throwing — 17 November 2026
- Confirmed from the live source: RIPSHIFT's optional Physical Grenade Throw responds to controller Grip release outside the chest pocket by setting `equipmentThrowPulseUntil` for 100 ms; the input mapper then sends the existing equipment-button input to DOOM. This code does **not** contain DOOM's grenade launch speed, arc, drag, impact physics, damage or fuse values.
- The desired barrel mechanic is **trigger-to-throw, not hand-swing-to-throw**. The barrel stays its original object; do not spawn a grenade or duplicate explosive properties. Reuse DOOM's *verified native grenade launch* vector, gravity scale and damping, then restore the barrel's original flight parameters once the temporary flight interval ends. A falling barrel's collision shape and object-specific behaviour remain its own.
- `WorldGrabMotionPolicy.h` has removed its former invented throw formula (smoothed hand movement × 1.7 plus fixed 6 m/s forward, clamped to 14 m/s). That formula was unrelated to the grenade launcher and should not be restored.
- `WorldGrabGrenadeBallistics` now carries verified native launch velocity in **DOOM native units** plus verified grenade flight gravity/damping and level generation. A trigger edge issues a Throw command only when those values are finite, valid and generation-matched. No native sample -> **keep holding; do not invent a throw or launch a grenade instead**.
- `WorldGrabOutput` passes verified grenade ballistics to an eventual native engine adapter as-is. Grip state now passes through **Acquire → native-target VacuumAttract → geometry-based Catch → AnchorToHand → Throw/Drop**. Hand position is used for contact interception ONLY, never for attraction direction, moving the object, or changing launch speed.
- This is an intentionally fail-closed policy contract; **the native grenade launch capture and movable-object physics writer are not yet bound**. CI validates the policy but cannot demonstrate working barrel throws. DOOM grenade speed and trajectory constants have not been claimed or guessed.
- An identical starting velocity alone does not guarantee identical landing distance for differently sized objects with differing collision/drag behaviour. Physics adaptation must be validated with actual native body properties, preserving the original object and restoring its physics state.

## Implemented in source — NOT yet functional in headset
- WorldGrabTargetPolicy.h: generic hit snapshot, dynamic-physics eligibility and cyan/red visual feedback decision. Requires a validated native body ID; does not infer movability from a rendered laser.
- WorldGrabMotionPolicy.h: both-hand-aware Acquire/VacuumAttract/Catch/AnchorToHand/Drop/Throw intents use the generic verified native physics-hit snapshot, a 0.90 m initial selection limit, and **real object collider half-extents/orientation for catching**. Trigger Throw requires Catch and verified DOOM grenade ballistics.
- Existing VR laser/Grip probe remains read-only; the former hand-to-body spring-follow intent was removed. Pure math now determines item-size catch contact. No arbitrary DOOM entity memory writes are made.
- Existing engine FocusTracker selection is now observed passively via a thread-safe native state bridge. This is **not** a replacement for native all-physics-object collision detection and does **not** make barrels draggable.
- Two additionally verified engine trace-to-entity candidate hook sites now produce separate, read-only, frame-context candidate handle snapshots. These are not physics body pointers; DOOM continues to perform its normal entity resolution.
- Isolated tests cover dynamic vs static bodies, kinematic objects, player/character/projectile exclusions, stale level handles, unknown trace, contact range, ownership, release and throw.

## Still missing before a real gameplay build can grab anything
1. Confirm a native **collision trace or hand overlap query** that returns the actual hit entity (not just the FocusTracker interaction focus).
2. Verify how DOOM exposes the hit entity's physics body, dynamic/kinematic/static state, and writable/impulse-capable physics API in this game revision.
3. Verify the native Vacuum attraction path, **keep its original pickup destination**, suppress pickup consumption, read selected item's real collision bounds, and implement safe Catch/AnchorToHand at hand-surface contact using a verified native body Bind/SetOrigin/Axis API. No custom hand-target spring or new attraction force.
4. Feed validated body/bounds into WorldGrabMotionPolicy; bind verified grenade launch behaviour to the caught native rigid body, then restore properties on Drop/Throw. Cover removal, teleport, loading, tracking loss and multiplayer constraints.

**The cyan laser is still visual-only. The new trace-candidate observer logs DOOM-native handles only; red target indication and actual object grabbing are not connected to the game.** Do not give the tester another laser-only screenshot task.

## Concrete next native investigation (no headset test)
- A local, read-only inspector is committed at `tools/Inspect-DoomWorldPhysics.ps1`. It reads DOOMx64vk.exe on the owner's PC without loading an injected DLL, enabling developer/cheat CVars, or changing game files.
- Run from the repository checkout: `powershell -ExecutionPolicy Bypass -File .\\tools\\Inspect-DoomWorldPhysics.ps1` (use `-ExePath` only for a custom Steam install). It writes `tools/DOOM-native-physics-report.txt`; this report is a read-only analysis artifact; it is never consumed by gameplay.
- The upgraded inspector also extracts focused native code windows for every located class/string reference, the two FocusTracker calls, and the queue builder; it checks simple REX LEAs and absolute-VA data references. It **does not** prove a native dynamic-physics setter or make the build playable.
- Build CI now checks PowerShell syntax and compiles the analyzer without requiring DOOMx64vk.exe on the runner. GitHub build success has not yet been verified.

## Boundaries
- Never use the retired player idPhysics velocity setter (previously caused void teleport).
- Do not enable developer/cheat CVars, install additional loaders, write game archives or touch UserData.
- Opt-in DEV testing only until the actual native adapter passes in-headset verification.
- Preserve DOOM's original Vacuum destination and global pickup rules. Never patch the Vacuum rune globally, redirect it to the controller, or let selected physics props be consumed as health/ammo.
