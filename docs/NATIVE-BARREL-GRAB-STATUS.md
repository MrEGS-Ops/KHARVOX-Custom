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
- Isolated tests cover dynamic vs static bodies, kinematic objects, player/character/projectile exclusions, stale level handles, unknown trace, contact range, ownership, release and throw.

## Still missing before a real gameplay build can grab anything
1. Confirm a native **collision trace or hand overlap query** that returns the actual hit entity (not just the FocusTracker interaction focus).
2. Verify how DOOM exposes the hit entity's physics body, dynamic/kinematic/static state, and writable/impulse-capable physics API in this game revision.
3. Write a game-thread adapter that fills WorldGrabPhysicsHit from that native query and applies validated physics impulses/spring motion.
4. Feed the verified hit to red/cyan laser rendering and to WorldGrabMotionPolicy; handle removal, teleport, load, focus loss and multiplayer constraints.

**The current cyan laser is visual-only. Red indication and actual object grabbing are not connected to the game yet.** Do not give the tester another laser-only screenshot task.

## Concrete next native investigation (no headset test)
- A local, read-only inspector is committed at `tools/Inspect-DoomWorldPhysics.ps1`. It reads DOOMx64vk.exe on the owner's PC without loading an injected DLL, enabling developer/cheat CVars, or changing game files.
- Run from the repository checkout: `powershell -ExecutionPolicy Bypass -File .\\tools\\Inspect-DoomWorldPhysics.ps1` (use `-ExePath` only for a custom Steam install). It writes `tools/DOOM-native-physics-report.txt`; this report is intentionally ignored by no gameplay code and can be shared for reverse engineering.
- The inspector validates the two observed FocusTracker-to-native trace queue CALL targets and enumerates string locations and simple RIP-relative references to physics-class symbols. It does **not** by itself prove a native dynamic-physics setter, and it does not convert this build into a playable grab.
- Build CI now checks PowerShell syntax and compiles the analyzer without requiring DOOMx64vk.exe on the runner. GitHub build success has not yet been verified.

## Boundaries
- Never use the retired player idPhysics velocity setter (previously caused void teleport).
- Do not enable developer/cheat CVars, install additional loaders, write game archives or touch UserData.
- Opt-in DEV testing only until the actual native adapter passes in-headset verification.
