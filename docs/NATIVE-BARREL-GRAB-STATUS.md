# DOOM (2016) native barrel grab — implementation ledger

## Confirmed from the user's files
- `DOOMx64vk.exe` contains the registration string `g_dragEntity`, with a description about aiming at physics objects and holding fire, and the native class names `idDragEntity`, `idProp2`, and `idProp_Moveable`.
- The uploaded `envhazard/barrels/red` declaration uses `idProp2`, `physicsAttributes = "barrels"`, `isStatic = false`. The ordinary `prop/moveable/barrel_a` is a separate movable-prop family.
- The executable matches the existing RIPSHIFT FocusTracker machine-code signature at RVA `0xD48938`. That trace is currently an **interaction** trace, not a validated all-physics-object selector.

## What has been implemented
- `WorldGrabMotionPolicy.h` computes two-hand-aware Grip acquisition, held-object follow velocity, release and Trigger throw intent, with distance checks, level-generation release, collision/weapon-owner priority, and finite/velocity clamps.
- Unit tests exercise these state transitions independently of DOOM. This is **not yet a functional grabbing feature**: it deliberately has no game-engine writes.

## Still required for a real headset test
1. Verify a native collision trace that identifies non-interactable props, not just focusable UI/collectibles.
2. Confirm the runtime target is the exact red explosive-barrel entity (not a projectile, world brush, or stale handle).
3. Identify and signature-validate a native rigid-body impulse/force method on DOOM's actual object class. **Do not reuse the retired player's idPhysics velocity setter** (caused void teleport).
4. Connect the engine adapter on the game thread, including release on level change and focus loss.
5. Only then replace the input-only DEV probe with the playable grab toggle.

## Save / launcher boundaries
Do not enable developer mode, install an outside loader, write game archives, change cheat/developer CVars, or touch UserData. The test must be runtime-only and opt-in.

## Current verification
CI compilation and policy tests are necessary but do not establish in-headset native grabbing.
