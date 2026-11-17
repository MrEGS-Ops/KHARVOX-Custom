# DOOM (2016) native world-object grab — implementation ledger

## Goal: physics properties, not specific meshes or barrel types
- Free-hand controller AIM ray (up to 0.90 m) or short-range hand contact/overlap finds an actual DOOM collision hit.
- Resolve the hit entity and its physics body on DOOM's game thread.
- Allow any verified, non-static, non-kinematic, movable **dynamic physics body**.
- Exclude player, characters, projectiles, scripted objects and unknown/stale handles.
- Valid candidate -> red aim laser; unknown/static/non-grabbable -> cyan aim laser.
- Fresh free-hand Grip -> acquire; hand motion -> body follows through DOOM physics; release -> drop; Trigger while holding -> throw. Physics must remain collision-owned.
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

## Implemented in source — NOT yet functional in headset
- WorldGrabTargetPolicy.h: generic hit snapshot, dynamic-physics eligibility and cyan/red visual feedback decision. Requires a validated native body ID; does not infer movability from a rendered laser.
- WorldGrabMotionPolicy.h: both-hand-aware Grip acquire/follow/drop/throw commands now use the generic physics-hit snapshot rather than a verifiedBarrel flag, including zero-distance hand contact and a 0.90 m reach limit.
- Existing VR laser/Grip probe and spring/throw velocity calculations remain separate. No arbitrary entity memory writes are made.
- Existing engine FocusTracker selection is now observed passively via a thread-safe native state bridge. This is **not** a replacement for native all-physics-object collision detection and does **not** make barrels draggable.
- Two additionally verified engine trace-to-entity candidate hook sites now produce separate, read-only, frame-context candidate handle snapshots. These are not physics body pointers; DOOM continues to perform its normal entity resolution.
- Isolated tests cover dynamic vs static bodies, kinematic objects, player/character/projectile exclusions, stale level handles, unknown trace, contact range, ownership, release and throw.

## Still missing before a real gameplay build can grab anything
1. Confirm a native **collision trace or hand overlap query** that returns the actual hit entity (not just the FocusTracker interaction focus).
2. Verify how DOOM exposes the hit entity's physics body, dynamic/kinematic/static state, and writable/impulse-capable physics API in this game revision.
3. Write a game-thread adapter that fills WorldGrabPhysicsHit from that native query and applies validated physics impulses/spring motion.
4. Feed the verified hit to red/cyan laser rendering and to WorldGrabMotionPolicy; handle removal, teleport, load, focus loss and multiplayer constraints.

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
