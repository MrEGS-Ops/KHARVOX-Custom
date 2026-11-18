# World Grab — Synchronization and Activation

Experimental only; preserved baseline: `backup/world-grab-pre-sync-inspector-20261118`.

## Log-backed regression
Build 788 forced the native resting field to zero while manually moving the
physics-body origin. The model appeared frozen until Grip release, then jumped.
This affected Strong Anchor and all five activation methods.

## Tests (record video and native log)

1. **Passive kick fingerprint** — Enable SYNC INSPECTOR with activation mode
   OFF. Aim at an untouched movable barrel, observe for 2 seconds, physically
   kick it. `WORLD-GRAB-KICK-FINGERPRINT` identifies passive wake or movement
   *candidates*; it is not a verified kick-event callback.
2. **Untouched Grip** — Choose Test 1 Direct; point at an untouched barrel,
   hold Grip, move free hand, walk. Note **visible** motion separately from
   the body's `GetOrigin` readback.
3. **Strong Anchor control** — Select Strong Anchor. Kick a fresh barrel,
   Grip, walk and rotate. The object should now visibly track instead of
   being frozen until release.
4. **Release** — Hold hand still and release. Compare video with
   `WORLD-GRAB-DROP-TRACE`. No snap to hand or violent downward throw.
5. **Activation comparison** — Only after live follow is established, repeat
   on untouched props with tests 2–5. No memory-write-only successes.

## Distinguish measured signals

- `WORLD-GRAB-SYNC`: physics origin, verified rest-state read, memory
  readback. Does **not** establish visual/model movement.
- `WORLD-GRAB-KICK-FINGERPRINT`: read-only native state/movement candidate
  without recent RIPSHIFT origin edits. Video correlates whether a real
  kick caused it.
- `WORLD-GRAB-HOLD-SYNC`: confirms forced rest is disabled while held.
- `WORLD-GRAB-DROP-TRACE`: physical-origin movement following release.
- Native entity/render-model positions are currently **UNVERIFIED**.
