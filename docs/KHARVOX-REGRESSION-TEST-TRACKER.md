# KHARVOX — ongoing headset & launcher regression tracker

**Canonical living checklist**. Update this document whenever a fix ships, a headset test passes, or a regression returns. Last updated **2026-10-14**. Last confirmed green baseline before latest targeted kill-speed changes: [Windows CI #360](https://github.com/MrEGS-Ops/KHARVOX-Custom/actions/runs/38386136841). For the newest build, check [Actions on the native fast-kill and wheel fix branch](https://github.com/MrEGS-Ops/KHARVOX-Custom/actions?query=branch%3Awork%2Foct14-restore-native-fast-kills). CI results are **not headset validation**.

**Status legend:** `FAIL` = reported broken in headset; `PENDING` = needs user's test; `BLOCKED` = implementation incomplete; `CODE` = shipped in source/CI but unverified in Quest. Never mark PASS without an actual observed pass and a build ID. GitHub Markdown checkboxes can be ticked individually when verified.

## 2026-10-15 — #466 Cactus KHARVOX v1.1.1 hand HUD ORIGINAL RESTORE (compiled and tested)

**User requirement:** regress all experimental hand HUD placement edits made Oct 8–12, restore CactusVRStudios/KHARVOX GitHub tag `v1.1.1` (release title "KHARVOX 1.11") wrist/hand HUD **as normal stock behaviour, not a separate mod**, without touching other gameplay mods.

**Source audit:** `src/hud/OffhandHudPolicy.h`, `src/hud/HudLayoutPolicy.h`, `config/offhand_hud_default.cfg`, `config/hud_flat_calibration_default.cfg`, `config/hud_profile_calibration_default.cfg` are byte-for-byte identical to Cactus `v1.1.1`. `src/hud/HudHook.cpp` differences comprise unrelated weapon-wheel reset, No HUD independence, revenge snapshot and comments; core hand HUD origin/basis/surface placement are unchanged from upstream. `src/camera/CameraHook.cpp` still calls original `KharvoxHudCaptureOffhandRenderFrame`; `src/openxr/OpenXRBootstrap.cpp` still publishes both hand poses. Historical custom hand-wrist changes were reverted in commits `f5bfb3e455` / `9ea20ca015` on Oct 12. Legacy launcher `BackOfHandHud` flag exists only for old JSON compatibility and is hard-disabled; no independent mod checkbox.

**Actual remaining regression:** `src/hud/HudHook.cpp::reloadOffhandHud` reads mutable, per-install `offhand_hud.cfg` over native geometry and never automatically refreshes that saved value from `offhand_hud_default.cfg`. A previously saved experimental custom HUD position survives later code/DLL rollbacks. If active config missing it otherwise initializes six zero-value records instead of Cactus accepted calibrations.

**Implemented:** `launcher/KharvoxLauncher/CactusHandHudRestore.cs` is invoked after exclusive launch gate/game-not-running/verified matching DLL and before DOOM starts. First launch validates shipped template is exactly original 7-line `v1.1.1`, backs up non-original saved `offhand_hud.cfg` to a unique `.pre-cactus-v1.1.1-*.bak`, atomically installs original file, and backs up/disables stale `enable_live_ammo_calibration` marker. Creates `.kharvox-cactus-v1.1.1-hand-hud-restored` marker so later user calibrations are NOT reset. If saved file is later deleted, safely recreates it from original. Separate `--self-test-cactus-hand-hud` asserts backup/no-overwrite/missing-file recovery/template validation; Windows CI passed. Both Full and Dev Patch now include `offhand_hud_default.cfg`; source code for wrist HUD orientation/position was not rewritten or copied wholesale to avoid unrelated regressions.

**Build #466** https://github.com/MrEGS-Ops/KHARVOX-Custom/actions/runs/38720224180 **SUCCESS**: launcher, original-HUD restoration self-test, native Release, C++ policies, resource mods, supervisor, packaging, and uploads passed. Artifacts: `KHARVOX-Full-Build` and `KHARVOX-Dev-Patch`.

**Headset status:** original Cactus placement restored in source and runtime setup, but visuals NOT YET USER VERIFIED. The #443 fast GK/chainsaw animations and #456/#461 camera/stereo fixes were left unchanged. If user runs with optional "No HUD" checked, it still deliberately hides HUD (separate intended mod), so uncheck for stock HUD.

## 2026-10-15 — #456 user test: head tilt improved, short blink remains

User reports after #456: "its working better but there is like a flash or a blink, maybe only a few frames but its quite distracting". #456 is an improvement, not a full headset pass. The GK/chainsaw full-speed timing that was confirmed working in #443 must be preserved.

**Working theory (NOT yet confirmed without a new runtime log):** The native/SFS scene identity in `src/openxr/OpenXRBootstrap.cpp` marks a still-active cinematic participant guard as a separate Scripted scene after DOOM's gameplay camera has already returned. `sameSceneContext` in `src/native/NativeFrameSource.h` rejects a pair from a different scene domain; with `nativeFrameValid=false` the projection deliberately submits zero OpenXR layers, which may look like a blink.

**#461 implementation (NOT HEADSET VERIFIED):** Use `KharvoxCameraReturnedToGameplayAfterCinematic()` as a read-only indicator for the existing gameplay-camera return latch to skip the extra Scripted stereo scene during the participant guard release grace. Preserve the stereo image-matching safety policy and source domains for genuine authored animations. Add `[STEREO-GAP]` diagnostics to count omitted projection frames; add C++ regression cases covering cinematic, scripted, gameplay, and quad domains. The original #443 DOOM-native kill timing logic is untouched.

Acceptance: GK/chainsaw normal-speed verified again; no tilt or blink after an execution, no one-eye mismatch/black projection, wheel blur and slow motion still work. If the blink remains, inspect `[STEREO-GAP]` + `[NATIVE-SOURCE]` before changing another renderer path.

## 2026-10-15 — diagnosis corrected: DOWN+SIDEWAYS world tilt, then SNAP level

User clarified: the headset remains physically trackable, but the **entire rendered view tilts down and to one side** for ~1 sec during/after GK, then abruptly resets upright. Do NOT interpret this as ordinary yaw catch-up or ask for forced neck/head turns. This supersedes the earlier 600°/s yaw-based recovery hypothesis.

**Source-level root cause candidate found:** `src/openxr/OpenXRBootstrap.cpp` sets `animatedSequenceActive = s.nativeAdaptiveParticipantGuardActive || s.immersiveCinematicActive`. The participant guard intentionally lasts beyond the actual cutscene. `src/camera/CameraHook.cpp::patchCamera` on DOOM's ordinary gameplay caller `0xE3F96A` calls `makeStableGameplayBodyBasis(gameplayCamera, animatedSequenceActive, ...)`. When the lingering guard is active, this **skips the usual gravity-level pitch/roll removal** and feeds a tilted native animation camera into OpenXR, despite gameplay already being resumed. When the guard expires 0.5–3s later, gravity leveling resumes and the world suddenly snaps level. This aligns with the symptom. Previous user extended logs show `[BODY] cinematic reentry` and `[NATIVE-CINEMATIC] state=0` **before** `[CINEMATIC-HZ] immersive hold released` and `[BODY-YAW] ... released`, proving a multi-frame handoff interval; **precise degree of tilt was not recorded**, so headset test still required.

**Changes for #456 (UNVERIFIED IN HEADSET):**
- Reverted earlier speculative `PostCinematicBodyTurnRecovery` and its gentle yaw turn policy back to confirmed-good #443. **No native yaw remap/catch-up changes shipped.**
- Added `postCinematicGameplayCameraReturned` state strictly between DOOM's observed cinematic->gameplay caller transition and the renderer's animated-guard clearing (or new cinematic entry). While true, an actual gameplay camera **must discard native pitch/roll immediately**, even if the participant observer remains latched. A REAL cinematic camera retains its authored animation and HMD rotation, preserving immersive GK visuals.
- Added `preserveAnimatedGameplayBodyBasis` pure policy and exhaustive tilt/roll handoff assertions in existing `tests/camera_basis_policy_tests.cpp`; registered that test under `tests/policy/CMakeLists.txt` to run in CI.
- **Acceptance:** user reports no down/side tilted-world hold or abrupt snap after GK/chainsaw; fast execution animations remain full-speed as confirmed in #443; HMD positional/rotational tracking remains comfortable; weapon wheel slow/blur intact. No forced head movements requested.

## 2026-10-15 — after-kill delayed unexpected recenter while RUNNING: body yaw guard

User clarified no violent headset test is appropriate: after fast GK, running away works initially, then view/head focus suddenly snaps or recentres as if Meta's reset view command were held. Direction (yaw vs pitch vs position) unknown. **Do not ask for forced head twisting or neck testing.**

**Trace supports native body yaw auto-catch-up as leading explanation:** `src/openxr/PostCinematicYawPolicy.h` releases the body yaw guard after 500ms quiet and 12 stable frames. `src/openxr/OpenXRBootstrap.cpp::nativePhysicalBodyTurnX` then resumes automated DOOM right-stick body catch-up with `bodyFollowYawSpeed=600` degrees/second. User extended log `3 (1).log` shows 52 cinematic body entry events, 43 cinematic layer-position holds and repeated `post-cinematic native yaw guard released reason=stable/manual-turn` roughly 0.3–0.5s after each exit. Real XR head-origin rebase `[HEAD] gameplay/load entry rebased...` occurs **only twice** in the full 32k line log; no evidence for a Meta headset tracking reset after each execution. Another known limitation: cinematic HMD physical translation is intentionally frozen until animation exit; fix this separately ONLY if proven to cause persistent positional issues.

**Source mitigation under test** (NOT headset verified): Upon cinematic-to-gameplay transition, arm `PostCinematicBodyTurnRecovery`. While moving with the left-stick, suppress ONLY *automatic* native body-yaw reconciliation (not HMD pose tracking, manual right-stick turns, locomotion, camera source/rendering, GK/chainsaw timing, weapon wheel time dilation). Once left-stick is neutral for 250ms, resume corrective turning at capped **90°/s** until head-to-body residual is settled; manual right-stick turn cancels the recovery latch immediately. Preserve latch through weapon wheel so opening the wheel cannot abruptly reactivate 600°/s catch-up. Existing native body-follow remains 600°/s during ordinary gameplay outside this narrow post-cinematic window. Pure C++ regression tests validate locomotion, guard waiting, user-directed turn priority, recovery speed and state reset.

**Acceptance:** no unexpected snap when running straight ahead after either GK or chainsaw; head pose stays responsive without forceful head test; GK and chainsaw play at real speed exactly as #443; weapon wheel still slows/blurs; no controller remapping changed. If user still feels *pitch* lock after release, isolate cinematic 6DOF HMD translation and per-eye re-projection separately. Do not claim the 600°/s body-turn as proven root cause without observing in-headset results.

## 2026-10-15 — Build #443 USER CONFIRMED fast kills work; NEW cinematic HEAD POSITION HOLD

**User headset confirmation:** "Both modes are working" (Fast Glory Kills AND Fast Chainsaw Kills) on #443 after restoring DOOM-native game tick pacing. Mark fast kill timing as **USER CONFIRMED WORKING; MUST NOT REGRESS**. The new issue is **HMD position feels locked ~1–3 sec during executions**. Do not edit timing, native sync CVar values or camera remapping to fix this.

**Observed in newly uploaded `3 (1).log` (native extended log, 2026-10-15 19:32-19:40):**
- 43 entries `[IMMERSIVE] OpenXR layer position held at cinematic entry; rendered HMD rotation pose synchronized` with matching `... position hold released`. Median hold ~1.12sec; initial intervals 1.364sec, 1.804sec, 1.082sec, 1.120sec, 1.147sec, 1.411sec; longest observed ~2sec. Exact match with headset subjective lock duration.
- Source: `src/openxr/OpenXRBootstrap.cpp` caches `s.immersiveCinematicLayerPosition=sourceMonoPose.position` at start and reuses it in SFS/native producer pose (`next.views[e].pose.position`, `next.head.position`), and `monoPose`. `src/camera/CameraHook.cpp` `patchCamera` for cinematic caller 0xA30CFF applies **HMD-only rotation**, explicitly **"HMD translation remains disabled"**. Thus **physical leaning/repositioning is intentionally frozen** throughout immersive executions by two complementary paths; not primarily input remapping.
- `[CUSTOM-MODS]` reports `handFocusRs=on`, but that is the right-stick interaction focus control and does not issue the cinematic layer-position freeze. `[IMMERSIVE] HMD freelook active` logs mean rotational pose is requested; they do NOT prove user's physical rotation actually remained responsive.
- Caveat: User said "head position locked" but did NOT distinguish physical positional leaning/side-stepping from **rotation** (turn/look left-right). Ask them this single clarification **before** potentially destabilizing SFS VR camera alignment. Full positional tracking requires coordinated changes to (1) DOOM cinematic render camera translations, (2) OpenXR projection/head pose, and (3) head-origin baseline/recentering. Merely deleting the layer hold without adjusting the cinematic camera may double translation or desync stereo and induce discomfort.
- Also note `src/openxr/OpenXRBootstrap.cpp::updateHeadPose` resets headZeroPosition while `worldActive=false` (cinematic), and sets `gameplayRecenterPending` when `KharvoxCameraGameplayActive=false`; these must be considered for 6DoF recovery. No code fix committed yet, no replacement #443 until clarified.

## 2026-10-15 — EXTENDED TRACE: 120 Hz forced simulation vs 37–39 Hz achieved game frames

**Received user upload:** 10 files, 5 unique SHA-256 contents (five duplicate filename pairs). The relevant newly extended `KHARVOX-KHARVOX(2).log` contains fast-execution diagnostic lines that earlier non-extended logs suppressed, and `KHARVOX(20261015-081718).log` confirms extended diagnostics enabled. An additional sidecar `KHARVOX-NATIVE-STEREO(6).log` is about native stereo observation rather than game timing.

**What the logs PROVE:**
- `[CUSTOM-MODS]`: `fastGK=on fastChainsaw=on`.
- `[FAST-KILLS]`: **`sync_noAdaptiveTick=1 expected=1`**, both when immersive execution begins and on release. The previous build's no-slow-motion native-CVar restoration was already working; retrying it cannot address slow kills.
- First immersive execution: `target=120 Hz` from OpenXR predicted display period `8333333 ns`; **`presents=193 elapsedMs=4946 measuredPresentHz=39.0`**. Starts ~19:16:36; likely a GK based on freelook log.
- Second immersive execution: `target=120 Hz`; **`presents=521 elapsedMs=14138 measuredPresentHz=36.8`**. Starts ~19:16:43 immediately after `[FAST-CHAINSAW] chainsaw attack -> native sync/cinematic kill classified`.
- Both hold-entry lines record native minHz=30, maxHz=200, adaptive=1, immediate=1, fixedTic=1. **Old immersive refresh code was forcing minHz=120 on every Present plus manually enabling native adaptive runtime tick**, over a game delivering only 37–39 completed Presents/sec. This is a direct confirmed timer configuration mismatch. Hypothesis: fixed-tick animation takes roughly 120/37 times wall-clock duration, explaining the very long chainsaw. This effect is not proven independently in headset yet.
- Presentation rates outside cinematic holds (~49-51Hz just before first hold, ~63Hz after second) were greater than inside the holds. Do not conflate headset nominal refresh with DOOM game frame rate.
- Log also has `[GLORY-SLOWMO] timescale cvar current value could not be resolved uniquely`. It's an old independent clock resolver that cannot resolve safely; it did not verify native effective speed. DO NOT force unknown memory addresses or fake 1x override.

**Focused source change awaiting headset test:** For the native full-speed path (either Fast GK / Fast Chainsaw enabled, or old slider requesting native bypass), do not force `com_adaptiveTickMinHz=120`, do not write `maxHz`, `immediateMode`, `com_fixedTic`, or `com_adaptiveTick`; do not rewrite DOOM's internal runtime enabled bit nor enable KHARVOX's participant adaptive bypass. Retain native `sync_noAdaptiveTick=1` and allow DOOM to own its game-time pacing. Preserve existing immersive forced pacing for **both fast-kill settings OFF**, and native weapon wheel entirely untouched. Log `[FAST-KILLS] ... pacing=DOOM-native minHz=... maxHz=... fixedTic=... headsetTargetHz=120` for first frame and `native pacing unmodified` at release. Add CI regression test asserting no forced cinematic pacing with either fast toggle or old GK Speed legacy level.

**Headset status:** STILL FAIL based on prior #440 test; source correction UNVERIFIED. Acceptance: no slowdown during or after BOTH execution types, weapon-wheel slow motion/blur preserved, VR cinematic rendering stays correct. If still slow with observed `minHz=30`, investigate intrinsic engine sync animation/cinematic timing next; DO NOT blindly add more CVar values.

## 2026-10-15 — new user launcher + native log evidence; zero slow motion still FAIL

User supplied paired logs `KHARVOX(20261015-080937).log` and `KHARVOX-KHARVOX.log` from `C:\\Users\\G180\\Documents\\Downloads\\KHARVOX-Full-Build\\`. The current folder is correct for the recent full-build packaging, but its hardcoded launcher `build=2026.09.29-launcher-v1.11-release` and native `build=2026.09.04-native-frame-inputs-r139` strings are NOT GitHub Actions identifiers. The log does not contain a `PATCH-BUILD.txt` commit/run marker, so exact build provenance remains unverified.

**Direct runtime evidence:**
- Launcher: `gloryKillSlowmoLevel=0`, arguments contain `+sync_noAdaptiveTick 1 +sync_alwaysSlowMo 0 +sync_lastEnemyEncounterSlowMo 0`. The launcher is requesting the same native fast-execution settings as Oct 5.
- Native: **six** `[GLORY-SLOWMO] native sync-attack flag ENTER/EXIT` pairs. The execution detector is alive.
- Native: `timescale cvar current value could not be resolved uniquely` appears repeatedly (about ten logged attempts); fallback memory resolver did NOT locate a safe unique value. This is separate from the launch CVars and does not prove whether the native `sync_noAdaptiveTick` switch worked.
- Both logs omit `[FAST-KILLS]` CVar readbacks and `[CINEMATIC-HZ]` status. This is **not** evidence those branches didn't run: launcher says `extendedLogging=disabled`, and `src/common/RuntimeLog.h` suppresses normal OpenXR `log()` messages unless extended diagnostics are enabled.
- Launcher reports `0xC0000005` after 181 s. Do not attribute this access violation to fast kills without a crash dump.

**Headset status remains FAIL** for zero time dilation during AND after BOTH GK and chainsaw. Launcher flags alone do not prove engine runtime CVar values. NEXT DIAGNOSTIC: In same KHARVOX build, enable Extended Logging, launch, execute one GK and one chainsaw, then read `<build>\\logs\\KHARVOX.log` and `<build>\\logs\\KHARVOX-KHARVOX.log`. Readback entries would distinguish adaptive-layout/signature failure, mode guard skipped, flag overwrite, or an independent game slowdown path. No claim of a completed fix.

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

## 2026-10-15 — exact Build #83 native Glory Kill implementation restored

**Ground-truth reference:** October 5 BUILD #83 = Git commit `c8a7b861de0ddb4437bab63b1a95384fbda197a1` (last change "Fix hitbox probe std::array memcpy source"), confirmed by the user's historical test log. That build's launcher had a single `DisableGloryKillSlowmo` checkbox and ONLY the following native DOOM launch CVars:
```
+sync_noAdaptiveTick 1 +sync_alwaysSlowMo 0 +sync_lastEnemyEncounterSlowMo 0
```
There was NO custom `timescale` CVar engine, NO kill playback multipliers, and NO post-execution recovery timer in Build #83's `CameraHook.cpp`. The user explicitly reported that VR Glory Kill slowdown disable was working beautifully. **Chainsaw executions were NOT independently confirmed in that historical test.**

**Restoration:**
- Restored the complete `updateImmersiveCinematicRefresh` function in `src/openxr/OpenXRBootstrap.cpp` byte-for-byte from commit `c8a7b861...`, including its native adaptive-tick state-save, reassert and restore semantics. Removed later runtime `nativeFastKills ? 1 : 0` CVar overwrites and persistent `sync_noAdaptiveTick` reassertions.
- Removed the entire October 15 post-execution 6.5-second synthetic time-dilation recovery guard, associated state, and `timescale` CVar forcing.
- Kept the October 5 native CVars identical and enabled by EITHER current Fast Glory Kills or Fast Chainsaw Kills checkbox (because the launcher has since renamed the original UI).
- Existing optional Boxing Guard uses its separate opt-in `timescale` path outside executions; no kill animation multiplier is applied while fast kills are selected. Native wheel blur and time dilation unchanged.

**Status: source restored / HEADSET PASS PENDING.** Test Glory Kills and chainsaw separately. If chainsaw remains slow, this is a distinct unproven native mode and requires native DOOM event classification, not another speculative global clock write.

**Mission select / save protection:** Current launcher passes `+devMode_enable 1` whenever a selected DOOM resource mod is actively installed, due to `-nopatchgame` (no executable patch). Bethesda confirms Developer Mode may flag campaign saves. A DOOMModLoader resource patch cleanup does not undo developer-only save flagging. DO NOT delete, rewrite, or automatically migrate saves; back up `%USERPROFILE%\\Saved Games\\id Software\\DOOM\\base` before any further modded launch. First verify whether launching with zero resource mods and no dev-mode flag restores Mission Select; if not, investigate save slot/provenance and Steam Cloud. Achievements/mission menu restoration is NOT verified.

- [ ] Re-run October 5-style Glory Kill test: zero slow motion during and after.
- [ ] Chainsaw execution full-speed during and after (independent acceptance).
- [ ] Native wheel slowdown/blur still works.
- [ ] No loss of mission select with unmodded launcher mode.
- [ ] Campaign save backed up prior to any further developer mode session.

## 2026-10-15 — ROOT CAUSE: immersive VR cinematic itself RE-ENABLED slow-mo

**Correction to earlier inaccurate history:** The user's verbatim 2026-10-05 confirmation "glorykill speed slowmo is disabled and working" was on **Build #81** (the same test reported GK displayed cinematically rather than properly in VR), NOT Build #83. The Oct 5 `DisableGloryKillSlowmo` checkbox was introduced in commit `a755f8ff1b47242ffd79c4ba7d385ea700c22f53`. User never separately confirmed chainsaw full speed in that test. Stop misidentifying later Build #83 as the user-confirmed speed baseline.

**Concrete code conflict diagnosed after repeated headset failures:** Despite the native `+sync_noAdaptiveTick 1 +sync_alwaysSlowMo 0 +sync_lastEnemyEncounterSlowMo 0` launch settings, the immersive VR cinematic function `updateImmersiveCinematicRefresh` in `src/openxr/OpenXRBootstrap.cpp` was executing **`write(syncNoAdaptiveTickCurrentRva,0)` on EVERY immersive cinematic frame**, and restoring the saved CVar at exit. This directly contradicts Build #81's native fast-kill launch flag. The old test also reported cinematic (not correct VR) presentation, so this present-path conflict was less exposed.

**Change (UNVERIFIED IN HEADSET):** Keep original three DOOM launch CVars. When EITHER fast-kill checkbox is enabled, maintain `sync_noAdaptiveTick=1` inside the immersive refresh hold, at hold release, and during ordinary frames. When both are off, preserve old immersive refresh behaviour. Do NOT use global `g_enableGameTimeScaling=0` (would break weapon wheel), custom kill speed multipliers, post-kill game-clock forcing or artificial 6.5s cleanup. Log first-frame and release CVar readback as `[FAST-KILLS]` to distinguish CVar reversion from other native timing causes. Added `fast-kill-sync-timing` C++ CI regression coverage.

**Acceptance - all MUST be tested on the new Windows build:**
- [ ] Glory Kill animation full-speed in headset with no time dilation during.
- [ ] Immediately normal movement/clock after Glory Kill (no tail).
- [ ] Chainsaw execution full-speed through entire takedown (NOT yet historically confirmed).
- [ ] Immediately normal movement/clock after Chainsaw (no tail).
- [ ] Weapon wheel time dilation and blur still work.
- [ ] `[FAST-KILLS]` immersive frame reports `sync_noAdaptiveTick=1` on enabled mode.
- [ ] No unintended change with both Fast Kill options off.
- [ ] No game saves or resource mod ownership modified by this timing fix.

**No build success or source inspection substitutes for those headset PASS reports.**

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
