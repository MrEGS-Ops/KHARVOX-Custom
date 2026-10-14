# KHARVOX — ongoing headset & launcher regression tracker

**Canonical living checklist**. Update this document whenever a fix ships, a headset test passes, or a regression returns. Last updated **2026-10-14**. Last confirmed green baseline before latest targeted kill-speed changes: [Windows CI #360](https://github.com/MrEGS-Ops/KHARVOX-Custom/actions/runs/38386136841). For the newest build, check [Actions on the native fast-kill and wheel fix branch](https://github.com/MrEGS-Ops/KHARVOX-Custom/actions?query=branch%3Awork%2Foct14-restore-native-fast-kills). CI results are **not headset validation**.

**Status legend:** `FAIL` = reported broken in headset; `PENDING` = needs user's test; `BLOCKED` = implementation incomplete; `CODE` = shipped in source/CI but unverified in Quest. Never mark PASS without an actual observed pass and a build ID. GitHub Markdown checkboxes can be ticked individually when verified.

## Active priority: fix these THREE before other changes

### [FAIL / SOURCE ROLLBACK — AWAITING HEADSET TEST] 1. Fast Glory Kills

2026-10-14 restoration branch: **deleted** the unverified 1.70x/1.90x game-timescale multiplier policy and both accompanying tests. The checkbox now selects DOOM's native `+sync_noAdaptiveTick 1 +sync_alwaysSlowMo 0 +sync_lastEnemyEncounterSlowMo 0` mode, based on the on/off mechanism the user explicitly confirmed working in the earlier Build #83 conversation. Cinematic refresh reasserts the native bypass instead of forcing the CVar to 0. The exact Build #83 SHA has not been positively matched to that chat reference. **Not PASS until observed in headset.**

- [ ] Fast Glory Kills ON: execution at the formerly working full-speed timing, entirely in VR.
- [ ] Both fast-kill toggles OFF: original game slowdown returns.
- [ ] Check VR camera continuity, cinematic refresh, Boxing Guard and native wheel time dilation.
- [ ] Capture same-session `%TEMP%\\KHARVOX.log`; confirm native slowdown CVar is active for fast kills.

### [FAIL / NATIVE SHARED BYPASS — AWAITING HEADSET TEST] 2. Fast Chainsaw Kills

The Chainsaw checkbox intentionally requests the **same** native sync-slowdown bypass. DOOM's existing native setting is global to the sync system, so enabling either checkbox enables the engine bypass for both. They are separate UI controls but **not independently timed**; never claim that switching only one on leaves the other execution type untouched. Chainsaw cutscene semantics still need headset proof.

- [ ] Chainsaw kill while either Fast Kills checkbox is ON: check whether the same native bypass affects chainsaw animation.
- [ ] Both OFF: check native chainsaw kill speed returns.
- [ ] Test native wheel, alternate weapon select and chainsaw animation cleanup.
- [ ] Keep status FAIL/PENDING until observed; do not claim verified from passing CI.

### [FAIL / GUARDED NATIVE ANIMATION REPLAY ATTEMPT — NEEDS QUEST TEST] 3. Super Shotgun — real reload animation persistence

The VR checkbox remains explicitly marked **Incomplete** in its launcher tooltip. The XInput firing delay alone was inadequate. The latest source now makes an **experimental attempt to restart the ORIGINAL DOOM reload animation** on SSG re-equip, using only three uniquely located native reload CVars whose name-pointer layout matches the independently validated DOOM timescale CVar. It pulses them for 250 ms and restores their originals; missing signatures or existing incompatible mod ownership **fail closed without writing game memory**. **The full 1.6-second input gate restarts from zero on each interrupted redraw.** None of this proves the native animation actually restarts until a Quest test shows it. Look for `[KHARVOX][SSG-NATIVE-REPLAY]` in the log. A trustworthy native shot/reload animation completion hook is still required for final correctness.

- [ ] Fire Super Shotgun once, switch to rifle before its reload animation finishes, then switch back immediately.
- [ ] **Required:** the **original DOOM Super Shotgun reload animation RESTARTS from its start** when you return. A motionless gun with a 1.6-second input lock is **FAIL**.
- [ ] Before the visible animation completes, shooting must remain impossible. After native reload completion, fire is enabled.
- [ ] Switch away and back repeatedly: each interrupted reload restarts **from frame zero** correctly; no ammo duplication. Check for unintended forced reload on the rifle or other guns.
- [ ] Toggle mod off: native switch/reload behavior restored. The three native reload CVars must always revert after their 250ms pulse, any weapon switch, menu/focus loss or feature disable.
- Current source provides only an equipped-time XInput firing gate, **NOT** native animation restart. Status stays FAIL/BLOCKED until native idHands/weapon reload animation restart path has been safely verified and connected. Don't call it fixed merely because CI passes.

## Queued — launcher & Windows UI

- [ ] [PENDING] Main launcher and Custom Mods dock side-by-side with no visible gap, aligned height/title bar.
- [ ] [PENDING] Default width narrow and resizable without excess blank area.
- [ ] [PENDING] Install / Generate / Mod Folder button alignment.
- [ ] [PENDING] Moving either main window moves the docked group; minimize/restore keeps windows paired.
- [ ] [PENDING] Close companion windows without closing unrelated browser or Explorer windows.
- [ ] [PENDING] All 16 VR mod checkboxes visible, grouped, numbered; hover tooltips readable without flicker.
- [ ] [PENDING] Drag VR mod order within category, preserve Requires/Disables linked positions and greyed-out conflicts.
- [ ] [PENDING] Mods Folder ZIP drag/drop, context menu, Copy/Paste/Delete, Move Up/Down and persistent user-mod ordering.
- [ ] [PENDING] Nexus default-browser popup: separate window, fast launch, tracked reuse, correct position, focus, linked dock motion.
- [ ] [PENDING] USER MODS refresh catches added/removed/replaced ZIPs.
- [ ] [PENDING] Good/Bad/Unmarked configuration ratings saved and tied to exact settings.
- [ ] [PENDING] Skull-key Easter egg: visual, click feedback, three-click launch and clean exit.
- [ ] [PENDING] DML install/verify/repair and optional first-party resource mods. No need to install user mods for VR fixes.

## Queued — baseline VR & HUD

- [ ] [PENDING] Normal weapon aim/fire/selection, locomotion, smooth/snap turning, jump, menu controls.
- [ ] [PENDING] Handedness: changing weapon-hand owner doesn't swap menu/locomotion mapping unexpectedly.
- [ ] [PENDING] Original Cactus rectangular hand HUD present with correct position/orientation; **not** a separate mod.
- [ ] [PENDING] No HUD and No Weapon Wheel remain independent.
- [ ] [PENDING] Unmodified native weapon fire/feedback with all experimental checkboxes off.

## Queued — behind-head weapon wheel

- [x] [USER-CONFIRMED PRE-REGRESSION] Moving the wheel hand OUT of its behind-head activation zone closes the wheel. Do not replace, retune or relitigate this working zone-exit mechanism. The remaining defect is native weapon commit when leaving WITHOUT Grip.
- [ ] [PENDING] Left and right behind-head zones reachable without shoulder strain.
- [ ] [CODE / HEADSET TEST REQUIRED] Wheel HUD reoriented by 180-degree in-plane rotation (not mirror), original dimensions retained, headset-facing normal unchanged. Verify which rotation axis was actually wrong in headset.
- [ ] [PENDING] Move wrist to change weapon; hold Grip to freeze selection and pull to equip.
- [ ] [CODE / HEADSET TEST REQUIRED] Exit zone without Grip: no delayed 300ms weapon restore; send three centered native-selection frames before releasing native wheel button. **Test carefully** that DOOM truly discards its prior highlighted slot; centering alone may be insufficient. Grip release while hand remains inside must not close wheel. If equipped weapon still changes even momentarily, FAIL; must intercept native commit instead.
- [ ] [PENDING] Correct left/right weapon hand and opposite grenade hand ownership after confirmed selection.
- [ ] [PENDING] No accidental melee/secondary fire/hip grab when entering or leaving wheel zone.
- [ ] [PENDING] Native weapon wheel time dilation, blur and haptics retained.
- [ ] [PENDING] Weapon Wheel Button Remap tap/hold and joystick-crouch mapping.

## Queued — invisible hip holsters & Quest open hand

- [ ] [PENDING] Two accessible distinct hip zones positioned comfortably.
- [ ] [PENDING] Empty hip Grip stows weapon identity **without a hip mesh** or hip 6DoF weapon model.
- [ ] [PENDING] Occupied hip Grip draws stored weapon **even when the same hand already holds another**; no implicit swap.
- [ ] [PENDING] No duplicated inventory entries or weapon IDs across hips.
- [ ] [PENDING] Gauss, Chaingun and BFG blocked from hip storage.
- [ ] [PENDING] Quest 3 capacitive/proximity full open-hand gesture after 450ms virtually unequips; no world drop.
- [ ] [PENDING] Natural hand movement, trigger use and wheel reaches don't falsely unequip.
- [ ] [PENDING] Wheel or hip draws can re-equip after open-palm gesture; gun remains in DOOM inventory.
- [ ] [PENDING] Left/right hip entry haptics route to correct controller.

## Queued — grenades, combat and movement

- [ ] [PENDING] Chest zone small and near sternum, not abdomen-wide.
- [ ] [PENDING] Grenade Grip TAP changes type once; Grip HOLD does not cycle first.
- [ ] [PENDING] Held pull/release emits one throw; cancelled gesture does not.
- [ ] [PENDING] Original grenade projectile mechanics retained; hand-directed physics requires validation.
- [ ] [PENDING] Grenade activation and cooking heartbeat haptics without unwanted grenade/weapon fire.
- [ ] [PENDING] Punch melee fires only on forward/horizontal punches, not raising hands or reaching wheel.
- [ ] [PENDING] Physical Glory Kill hit registration and aiming unaffected (separate from Fast GK).
- [ ] [PENDING] Boxing Guard pose head focus / slight time dilation / return-to-hand delay and native wheel effect priority.
- [ ] [PENDING] Gauss Siege Charge Slow Movement allows intended slow movement (previously still immobile).
- [ ] [PENDING] Physical Crouch triggers crouch/stand without jump toggling.
- [ ] [PENDING] Directional Dash remains DISABLED after unsafe native physics regression.
- [ ] [BLOCKED] Haptic Overhaul damage pulses require validated live health/armour read; don't expect pulses.
- [ ] [BLOCKED] Revenge Demon is diagnostic only.
- [ ] [BLOCKED] Dual-wielding is just policy/design; not yet a usable DOOM user mod + VR bridge.

## Reporting protocol

For each tested item, annotate **PASS / FAIL**, **build number**, approximate level/weapon, and brief observed behavior. Retest all failed items on the next build, then preserve previously confirmed passes in this document.

**Current focus lock:** Until the three top-priority issues are actually fixed, avoid unrelated changes unless required to keep CI/runtime stable.
