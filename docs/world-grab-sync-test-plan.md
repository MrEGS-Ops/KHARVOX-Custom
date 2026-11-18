# World Grab: single automatic Hand Anchor, physics and render evidence

Experimental branch only. Revert to
`backup/world-grab-before-unified-hand-anchor-20261118` if necessary.

## Automatic sequence — same barrel, same held Grip

- One switch: DEV → Experimental Features → HAND ANCHOR (AUTO TEST ALL).
- Also enable PHYSICS / RENDER INSPECTOR. The inspector overlays native
  entity identity, physics readback, rest value, current stage, stage age,
  motion evidence, and follow status.
- Hold free-hand Grip on a fresh untouched movable barrel (type validated).
- Automatically attempts:
  1. Signature-verified native Activate; observe 200ms.
  2. 0.35 native-unit toward-hand **position proxy**, observe 500ms.
  3. 0.20 native-unit Z **position proxy**, observe 500ms.
  4. Passive entity/physics-tick observation, 350ms.
  5. **HELD GRIP KICK WINDOW, 12 seconds**: stay gripping and walk/kick the
     very same barrel while it's unresponsive. Never force hand-follow unless
     native physics movement is observed while Grip is held.
  6. Direct-follow fallback after the kick window, still unverified until
     the user confirms the rendered barrel responds. 28-unit/tick follow.
- If native body movement is observed in an active stage (outside the probe
  write quarantine) the current stage is recorded, other stages stop, and
  the existing 6DoF hand-relative follow begins.
- Release logs that same handle plus stage/follow state, then samples physics
  displacement for 650ms. Reacquiring after kicking records rest/position and
  another independent activation sequence on the same engine handle.

## Evidence

`[WORLD-GRAB-AUTO]`: START (handle, entity/body vtable RVAs, origin, hand,
storage), ATTEMPT (method and writes), OBSERVE (body/rest), RESULT (native
change or no evidence), HELD_KICK_WINDOW, NATIVE_EVIDENCE (candidate),
FALLBACK, RELEASE.

`[WORLD-GRAB-SYNC]`: body position and rest. `[WORLD-GRAB-KICK-FINGERPRINT]`
is passive-motion evidence without a verified kick callback.
`[WORLD-GRAB-DROP-TRACE]`: release correction.

**Not measured yet:** independent DOOM entity render/model pose and a
validated selective player/object collision filter. The display labels them
UNVERIFIED. A physics-memory write is never counted as visible success.
No forced resting state during held grip; old Strong Anchor movement retained.

Test A: grip untouched barrel and observe auto methods without kicking.
Test B: KEEP GRIP HELD while moving/walking into the SAME barrel and KICK.
Test C: release, kick again if necessary, reacquire and compare.
Test D: natural release with hand still, inspect jump/fall/collision issues.
