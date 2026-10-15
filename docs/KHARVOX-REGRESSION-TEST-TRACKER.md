# KHARVOX — ongoing headset & launcher regression tracker

**Canonical living checklist**. Update this document whenever a fix ships, a headset test passes, or a regression returns. Last updated **2026-10-14**. Last confirmed green baseline before latest targeted kill-speed changes: [Windows CI #360](https://github.com/MrEGS-Ops/KHARVOX-Custom/actions/runs/38386136841). For the newest build, check [Actions on the native fast-kill and wheel fix branch](https://github.com/MrEGS-Ops/KHARVOX-Custom/actions?query=branch%3Awork%2Foct14-restore-native-fast-kills). CI results are **not headset validation**.

**Status legend:** `FAIL` = reported broken in headset; `PENDING` = needs user's test; `BLOCKED` = implementation incomplete; `CODE` = shipped in source/CI but unverified in Quest. Never mark PASS without an actual observed pass and a build ID. GitHub Markdown checkboxes can be ticked individually when verified.

## 2026-10-15 — user-reported Build #408 regressions and targeted source changes

**Headset feedback (FAIL):**
- **Fast Glory Kills:** execution is a little quicker, but the player remains slowed for **1–2 seconds after** the animation.
- **Fast Chainsaw Kills:** execution is a little quicker, but the player remains slowed for **about 5 seconds after** the animation.
- **Persistent Super Shotgun Reload:** original visible reload animation STILL DOES NOT RESTART after an interrupted reload; only a motionless gun and artificial firing block are observed.

**Latest source response (UNVERIFIED, NOT PASS):**
- After an observed native sync/Chainsaw execution ends, an opt-in, 6.5-second normal-gameplay guard checks the validated DOOM `timescale` float and clears residual fractional time dilation to 1.0. It is **OFF during executions, native weapon-wheel use, Boxing Guard and all cinematics**, and is reset on OpenXR focus loss. This targets lingering slowdown; whether the reported residue is caused by this CVar still requires headset proof. Check `[GLORY-SLOWMO] post-execution residual slowdown cleared` in the log.
- **SSG safety rollback:** the source has no verified original DOOM idHands reload-animation start/completion callback. The 1.6-second synthetic XInput firing gate and the unproven native reload CVar pulse are bypassed regardless of the saved checkbox until a real native animation hook exists. This removes KHARVOX's invisible firing delay, **but does not deliver the requested persistent animation restart**. Launcher tooltip explicitly marks it INCOMPLETE.
- Do not consider any problem solved because CI compiles. Inspect the exact build and verify in Quest. Keep native wheel's time dilation, blur and the previously working hand-exit behaviour unchanged.

**Acceptance:**
- [ ] Glory Kill completes, then **immediate normal gameplay speed**, no 1–2-second linger; wheel slow-motion still works.
- [ ] Chainsaw execution completes, then **immediate normal gameplay speed**, no 5-second linger; wheel slow-motion still works.
- [ ] No slow-motion leak on cancel, death, pause, or loss of focus.
- [ ] SSG no longer hangs motionless due to an artificial KHARVOX-only timer; ordinary native reload/firing preserved.
- [ ] SSG original animation ACTUALLY restarts on re-equip after interruption. **Still BLOCKED; not provided by the rollback.**

## 2026-10-15 — full-speed means NO slow motion during OR after execution

**User clarification:** Fast Glory Kills and Fast Chainsaw Kills must run at normal uninterrupted speed throughout their animations, and hand back control immediately at 1.0x afterward. No special game-wide speed multiplier. Preserve native weapon-wheel time dilation and blur.

**Recovered historical evidence:** On 2026-10-05, user explicitly confirmed the Build #83 VR Glory Kill slowdown-disable was working beautifully. That working approach requested DOOM's native `+sync_noAdaptiveTick 1 +sync_alwaysSlowMo 0 +sync_lastEnemyEncounterSlowMo 0`. It is **not** the unrelated, cancelled 2026-10-09 Actions run #83 in the newer fork; do not assume that cancelled SHA is the user's tested binary.

**Source changes subsequent to Build #414 (HEADSET TEST PENDING):**
- Found the launcher only passed those three native bypass CVars when the legacy Glory Kill Slow-Mo slider was below 10. Merely checking **Fast Glory Kills** or **Fast Chainsaw Kills** with the slider still at 10 did NOT activate all native settings at DOOM startup. Fixed: either checkbox now requests the three native CVars at process launch.
- The CameraHook previously still allowed fractional timescale from the old level 1–9 setting even with Fast Kills checked. Fixed: enabled Fast Kill checkboxes use native level 0 and skip fractional custom timescale during executions.
- Boxing Guard slow motion is suppressed while a native Glory Kill or chainsaw execution is active. Native weapon wheel presentation remains untouched.
- Keep the short, guarded residual native-timescale cleanup from Build #414 for AFTER execution; it never runs during another cinematic, native wheel, or guard pose. This fallback has NOT been verified as the root of the user's post-kill slowdown.

**Test acceptance (do NOT pre-mark PASS):**
- [ ] Fast Glory Kill: continuous normal speed from first frame of execution through first frame of free movement; no 1–2s slow tail.
- [ ] Fast Chainsaw Kill: continuous normal speed for the entire takedown and immediate exit; no ~5s slow tail.
- [ ] Neither checked: default DOOM native slow motion stays available (assuming old slider 10).
- [ ] Weapon wheel still produces its native time dilation and blur.
- [ ] Existing Boxing Guard still slows time outside executions.
- [ ] SSG native reload animation still incomplete; artificial 1.6-second silent gate remains bypassed from Build #414.

## 2026-10-15 — Build #414 DOOM resource fatal: cross-build safety preflight

**User-reported headset/desktop failure:** DOOMx64vk.exe aborted with `FATAL ERROR: Loading unverified resource` referencing `base/gameresources_002.pindex`. The user had enabled DOOM resource mods in a **different KHARVOX build**, then tried no mods in #414, then reselected mods without resolving the crash. This is a mod-resource startup failure, not a verified rendering fault.

**Old defect:** `DoomResourceModSession.PrepareAsync` returned immediately when selections were empty and that *build's* local manifest was missing, without inspecting leftover DOOMModLoader resource files. Each KHARVOX build kept a different private mod ownership state. Unverified artifacts could remain in DOOM/base and crash startup.

**Source repair, build test pending:**
- Always scan DOOM resource outputs before launch, even with zero mod selections and no prior manifest.
- Cross-build ownership manifest moved to **`<DOOM install>/KHARVOX-DOOM-RESOURCE-MODS.json`**, with legacy same-build manifest migration.
- Recover unverified generated numbered `gameresources_NNN.patch`/`.pindex` and `base/video/mods/` files by moving to **`<DOOM install>/KHARVOX-Resource-Recovery/<timestamp-id>/`**. Keep original bytes and a SHA256 inventory; **no permanent deletes**.
- Skip resources with official `.verify` companions. Unknown `gameresources.*` paths not matching the generated format **stop safely** rather than risk moving unknown/official game files.
- If the mod ZIP directory is deleted or a previously selected source is unavailable, preserve checkbox choices and start without resource mods once recoverable stale outputs are backed up. If DOOMModLoader is absent/invalid, use the same unmodded fallback. Re-apply still-enabled mods only from a clean generated-resource state.
- On loader errors, preserve a recovery copy of generated partial files, then report failure. No automatic changes to DOOMx64vk.exe or original mod ZIPs.
- Unit tests cover foreign patch detection, orphan .pindex recovery, file byte preservation, `.verify` protection, ambiguous-resource refusal, video output recovery and stable state across build directories.

**Not yet headset/DOOM validated:** Need to test both an unmodded launch after cross-build orphan cleanup AND an opt-in modded launch. DOOMModLoader still uses `-nopatchgame` and DOOM's `+devMode_enable 1` for enabled resource mods; no claim that a new mod install bypasses this independent engine validation issue.

- [ ] Run with no DOOM mods selected after old-build mods: game starts without unverified-resource fatal.
- [ ] Confirm recovery inventory/backup exists and user ZIPs are unchanged.
- [ ] Delete or move a previously selected mod ZIP: launcher preserves preference and starts unmodded.
- [ ] Switch KHARVOX build directories: resource ownership remains verifiable.
- [ ] Reselect known-good mod: installs and starts game with developer mode, no unverified-resource fatal.
- [ ] Original Cactus VR mods and previously working controller mapping unchanged.

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
