# KHARVOX VR mod: Persistent Super Shotgun Reload

## Scope
An **independent optional VR mod** named **Persistent Super Shotgun Reload**. It does not require Dynamic Quick Holsters, Enable Hands, weapon-wheel remapping, or the future dual-wield resource mod. Default OFF. Saved in `kharvox-custom-mods.json` as `PersistentSuperShotgunReload`, passed to the DOOM process as `KHARVOX_MOD_SSG_PERSISTENT_RELOAD`.

## Intended gameplay
- Fire the Super Shotgun once, switch immediately to another gun: the unfinished SSG reload stays pending. **Switching back does not grant a loaded blast.**
- While the SSG remains actually held/equipped in normal gameplay, its reload window advances. If stowed, unequipped, in a weapon wheel, a menu, cutscene, or paused, that progress freezes. It resumes after re-equipping.
- Repeated quick-swapping cannot credit unequipped time. Other weapons (including a rifle used during the SSG cooldown) fire normally, independently of the SSG debt.
- Releasing then freshly pressing the SSG trigger is required after the cycle. Holding trigger across a swap cannot auto-fire when equipping the SSG.
- New map/level resets the input-level reload state. Disabling the mod restores native gun input.

## Current implementation and known limitation
First implementation lives in `src/openxr/PersistentSuperShotgunReloadPolicy.h` and `OpenXRBootstrap.cpp`. It enforces a **1.6-second held-in-gameplay time** from a VR trigger edge by suppressing DOOM's virtual Fire input until the gate allows it. This is a **conservative input-level approximation**, not proof of native Super Shotgun shot completion or reload state: in the present repository a trusted DOOM native `SuperShotgunFired` / `ReloadCompleted` hook has not yet been verified. Consequently, an unsuccessful shot attempt may still enter the lockout, and DOOM's own animations may not represent the persistent gate correctly.

**Do not claim native reload animations or ammo internals are overridden.** When a safe native shot/reload callback has been established, replace the estimated fire acceptance and timed gate with actual native cycle state persistence while retaining the independent launcher switch and tests. Do not patch guessed memory offsets or reinitialize idHands state unsafely.

## Regression and acceptance tests
`tests/persistent_ssg_reload_policy_tests.cpp` (both main CMake and policy-only suite) validates:
- SSG shot -> 0.2s equipped -> 9s rifle -> return: no free reload.
- Resume equipped-time countdown from remaining time.
- Fresh trigger press after permitted reload cycle.
- No fake progress during menus, weapon wheel, hip holster or unfocused state.
- Other gun's trigger is never suppressed.
- Rapid equip/unequip is ineffective until actual equipped time has elapsed.
- No phantom shot with known empty ammo.
- Mod disabled passes trigger through and clears policy state.

## Not part of this mod
The proposed **dual-wielding** separate future mod may require the SSG to remain in hand **and the other hand to be completely free** to reload. That free-hand rule does not apply to ordinary single-gun play; this mod is purely about preserving SSG reload requirements through weapon swaps.
