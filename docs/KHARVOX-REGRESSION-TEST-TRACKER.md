# KHARVOX — ongoing headset & launcher regression tracker

**Canonical living checklist**. Update this document whenever a fix ships, a headset test passes, or a regression returns. Last reviewed **2026-10-14**. Baseline for these tests: [successful Windows CI #360](https://github.com/MrEGS-Ops/KHARVOX-Custom/actions/runs/38386136841) — compilation and automated tests passed; these results do **not** constitute headset validation.

**Status legend:** `FAIL` = reported broken in headset; `PENDING` = needs user's test; `BLOCKED` = implementation incomplete; `CODE` = shipped in source/CI but unverified in Quest. Never mark PASS without an actual observed pass and a build ID. GitHub Markdown checkboxes can be ticked individually when verified.

## Active priority: fix these THREE before other changes

### [FAIL] 1. Fast Glory Kills
- [ ] Toggle **Fast Glory Kills** on, perform a normal Glory Kill, confirm animation genuinely speeds up, not just the environment or timer. Note demon, weapon and approximate duration.
- [ ] Toggle off: game returns exactly to native timing. Re-enable and retest.
- [ ] Ensure BOXING slow motion and weapon wheel time dilation don't override kill speed incorrectly.
- [ ] Capture `%TEMP%\KHARVOX.log` from the **same** run and look for `[KHARVOX][GLORY-SLOWMO]` and sync-classification lines.
- Earlier in-game working build remains **unidentified**; earlier conversation contained the report that a native/full-speed mode had worked, but no verified exact SHA/build recovered. Do not claim a specific old build is known until evidence confirms it.

### [FAIL] 2. Fast Chainsaw Kills
- [ ] Toggle **Fast Chainsaw Kills** on and use chainsaw on a target where a takedown animation occurs. Should noticeably accelerate the takedown, not just the input lead-in.
- [ ] Turn off: native chainsaw kill timing restored.
- [ ] Test after selecting chainsaw via its normal input and via weapon selection. The game may switch the weapon declaration when animation starts.
- [ ] Test Chainsaw ON + Glory OFF, then Chainsaw OFF + Glory ON: independent behavior.
- [ ] Capture `%TEMP%\KHARVOX.log` and confirm chainsaw kill classification and timescale override lines.
- Earlier on/off wiring was coded but in-headset effectiveness **unconfirmed/FAIL**.

### [FAIL] 3. Super Shotgun — real reload animation persistence
- [ ] Fire Super Shotgun once, switch to rifle before its reload animation finishes, then switch back immediately.
- [ ] **Required:** the **original DOOM Super Shotgun reload animation RESTARTS from its start** when you return. A motionless gun with a 1.6-second input lock is **FAIL**.
- [ ] Before the visible animation completes, shooting must remain impossible. After native reload completion, fire is enabled.
- [ ] Switch away and back repeatedly: each interrupted reload restarts correctly; no ammo duplication.
- [ ] Toggle mod off: native switch/reload behavior restored.
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

- [ ] [PENDING] Left and right behind-head zones reachable without shoulder strain.
- [ ] [PENDING] Original weapon-wheel dimensions, correct pose/angle and reset each opening/closing.
- [ ] [PENDING] Move wrist to change weapon; hold Grip to freeze selection and pull to equip.
- [ ] [PENDING] Exit without Grip cancels and retains previously held weapon (no brief accidental equip).
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
