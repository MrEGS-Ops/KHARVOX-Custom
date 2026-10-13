# DOOM 2016 Dual-Wield — Gameplay Requirements

Status: **Design / isolated policy only**. Dual-wield engine hooks, second weapon render/fire path, and game-side resource mod have **not** been implemented. This specification is for a conventional DOOM gameplay mod, with KHARVOX optionally supplying independent left/right VR input.

## Global rules

1. Dual-wield requires **two different, eligible weapon types**. Do not duplicate a weapon.
2. No dual-wield if **either weapon is heavy**. Initial heavy list: Gauss Cannon, Chaingun, BFG, Rocket Launcher. Chainsaw is not a gun and is not eligible. Combat Shotgun and Super Shotgun are permitted as a pair.
3. Both weapons draw from **existing DOOM ammunition pools**, without separate left/right ammo inventories or ammunition duplication. Combat Shotgun + Super Shotgun both draw shells from the same reserve.
4. Each weapon tracks its own independent aim, primary fire, alternate fire (if present), cooldown/loaded status, and rendering.
5. While holding two weapons, any non-automatic weapon requiring cycling/reload **cannot cycle/reload**. Opening one hand to virtual-unequip its gun immediately permits the remaining held gun to reload/cycle. Unequipped guns stay in native inventory, not world pickup entities.
6. Automatic guns can fire while eligible and consuming native ammo normally. This exception is for non-automatic guns.

## Combat Shotgun: dual-mode exception

The Combat Shotgun has exactly **one primary-fire activation and one weapon-mod alternate-fire activation** per dual-wield reload cycle, **in either order**. These two separate allowances do not reset during dual-wield.

- Primary: one ordinary shotgun blast, consumes normal native shell ammunition.
- Alternate with Explosive Shot equipped: one explosive round (native shell use).
- Alternate with Charged Burst equipped: one **complete charged-burst activation** (up to three pellets/rounds discharged by the native mod, not a single sub-shot). Native ammo usage is preserved.
- The alternate allowance is spent **when the game accepts/releases the actual shot/burst**, NOT when the player merely holds the mod-charge control. Starting a charge then cancelling it must not consume the allowance.
- Both allowances can be spent regardless of order; after both are spent, the Combat Shotgun is empty/unavailable for firing until it is allowed to reload by leaving dual-wield mode.
- The other hand can still fire independently (subject to native ammo and its own readiness).
- If either ammo is insufficient or no weapon mod equipped, firing is rejected and that allowance is **not consumed**.
- Let go of the other gun -> Combat Shotgun immediately reloads/cycles when ammo is available; after successful reload, both allowances reset. Letting go of the Combat Shotgun instead should immediately permit the other gun to reload.
- A normal native reload/cycle completion event, not the input release alone, should mark reload successful, to avoid loading without ammo or interrupting engine animations.

## Example: Combat Shotgun + Super Shotgun

Player has 20 shared shells and both weapons ready. Combat Shotgun fires one primary blast and one Explosive Shot: game applies its own shell costs, and both Combat Shotgun action allowances are exhausted. Super Shotgun may fire independently. Neither gun can cycle/reload while both are held. Open the Super Shotgun hand -> it is virtually unequipped; the Combat Shotgun immediately becomes eligible to complete its reload and restore both firing allowances.

## Implementation boundary

**DOOM resource/gameplay mod:** gameplay weapon definitions, shot execution, native ammo deduction, native projectile spawning and damage, reload/cycle state, modifiers, animations and consistency with upgrades.

**KHARVOX bridge:** distinct physical hand assignment, 6DoF muzzle/aim poses, separate triggers/mod inputs, holster draw, open-palm unequip, wheel integration, and communicating dual-wield state to the game mod.

**Native engine caveat:** DOOM 2016's existing player gun controller assumes one active weapon. Conventional resource editing alone may not permit a truly independent second firing instance. Prototype the game-side second shot path first; introduce a minimal native gameplay hook only if necessary. Never claim full dual-wielding until independently directed projectiles and native shared-ammo accounting have been demonstrated.

## Acceptance tests

- Combat Shotgun may fire PRIMARY then ALT, or ALT then PRIMARY, exactly once each before solo reload.
- Charged Burst's entire one activation counts as the ALT; internal burst rounds don't individually consume multiple allowances.
- Separate native ammo decrements on either hand; no duplicate pools.
- Missing ammo or cancelled charging does not spend an allowance.
- One hand opening ends dual mode and permits only the remaining weapon to reload.
- Any heavy and duplicate pair is refused before dual mode engages.
