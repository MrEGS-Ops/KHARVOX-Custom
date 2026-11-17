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

## Implemented in source — NOT yet functional in headset
- WorldGrabTargetPolicy.h: generic hit snapshot, dynamic-physics eligibility and cyan/red visual feedback decision. Requires a validated native body ID; does not infer movability from a rendered laser.
- WorldGrabMotionPolicy.h: both-hand-aware Grip acquire/follow/drop/throw commands now use the generic physics-hit snapshot rather than a verifiedBarrel flag, including zero-distance hand contact and a 0.90 m reach limit.
- Existing VR laser/Grip probe and spring/throw velocity calculations remain separate. No arbitrary entity memory writes are made.
- Isolated tests cover dynamic vs static bodies, kinematic objects, player/character/projectile exclusions, stale level handles, unknown trace, contact range, ownership, release and throw.

## Still missing before a real gameplay build can grab anything
1. Confirm a native **collision trace or hand overlap query** that returns the actual hit entity (not just the FocusTracker interaction focus).
2. Verify how DOOM exposes the hit entity's physics body, dynamic/kinematic/static state, and writable/impulse-capable physics API in this game revision.
3. Write a game-thread adapter that fills WorldGrabPhysicsHit from that native query and applies validated physics impulses/spring motion.
4. Feed the verified hit to red/cyan laser rendering and to WorldGrabMotionPolicy; handle removal, teleport, load, focus loss and multiplayer constraints.

**The current cyan laser is visual-only. Red indication and actual object grabbing are not connected to the game yet.** Do not give the tester another laser-only screenshot task.

## Boundaries
- Never use the retired player idPhysics velocity setter (previously caused void teleport).
- Do not enable developer/cheat CVars, install additional loaders, write game archives or touch UserData.
- Opt-in DEV testing only until the actual native adapter passes in-headset verification.
