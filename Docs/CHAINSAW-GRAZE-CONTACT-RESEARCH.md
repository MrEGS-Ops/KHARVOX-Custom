# Chainsaw Graze — native contact investigation (2026-10-19)

Status: **research only; NO idle-Grip damage or enemy stagger is installed**.

## New gameplay observation
The player reports that **holding weapon Grip and touching a solid object already
produces sparks** with the chainsaw equipped, without pulling Trigger. This is
strong evidence the game has a native **world-contact visual FX path**. It is **not**
evidence that the FX event contains a damageable enemy ID. Do not treat sparks
as a combat-hit callback without runtime instrumentation.

## Verified source seams (specific to this repository)
- `src/weapon/WeaponHook.cpp`: `installFrontPushbackSuppression`,
  signature-checked DOOM RVA `0x6305B4`. Two near-contact traces can set a
  **player hands pushback flag** (`state+0x175`). KHARVOX clears the flag
  and skips the shared first-person hands animation block to preserve VR hand
  alignment. This does **not** disable native world collision, damage, or
  player physics. Crucially, this is **not** the enemy-damage API and must not
  be used to apply damage based solely on proximity to world geometry.
- `src/weapon/WeaponHook.cpp`: `armHandsHitReactionSuppression` handles
  the **player's first-person hands reactions**, not demon pain/stagger
  (RVA `0xD88998` / `0xD892E4`). Not a suitable demon-stagger hook.
- `src/camera/CameraHook.cpp`: `patchPlayerFocusTrace` at RVA
  `0xD48938` is **interaction focus**, not chainsaw tooth contact.
- `src/openxr/OpenXRBootstrap.cpp`: Grip, weapon identity, game-camera
  state, and the native chainsaw-execution flag are known and can gate a
  future probe. Existing logs report Grip edges and native execution.
- Existing resource-mod generators patch `aiGlobalSettings` and
  `goreBehavior`; these do not authorize a per-contact enemy health edit.

## Next safe experimental step
1. With an opt-in **read-only** diagnostic, locate and signature-check the
   specific native function responsible for chainsaw idle-contact sparks.
   Correlate that function's enter/exit with Grip pressed, weapon=Chainsaw,
   Trigger released, and DOOM native execution inactive.
2. Record the *native contact kind*, hit point, surface type, and **validated
   target entity identity** if exposed. Distinguish world static materials
   from damageable demons; a spark on a wall must never cause damage.
3. Confirm the native demon health/damage and *minor pain reaction*
   dispatch. Hook only a proven event with a stable native identity and
   controlled re-entry; never call a guessed vtable/offset on live enemies.
4. Add experimental Graze as a **separate opt-in checkbox**, with a capped
   low damage rate (~1 per 300 ms), ordinary light-enemy hit reactions,
   zero boss effects, and full suppression during any chainsaw execution.
   Keep DOOM's Trigger + fuel and execution lifecycle unchanged.

Until 1–3 are verified from runtime evidence, no damage/stagger code should
be installed. A cosmetic spark path alone cannot safely drive enemy health.
