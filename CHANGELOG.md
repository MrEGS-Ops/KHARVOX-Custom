# RIPSHIFT VR — Changelog

Changes to the RIPSHIFT VR add-on for DOOM (2016) using the original KHARVOX VR installation. This changelog describes the current experimental work; successful builds and code tests do not substitute for headset verification.

## 25 November 2026 — RIPSHIFT packaging and name cleanup

- Consolidated the current experimental packaging into `.github/workflows/build-ripshift.yml` with the `RIPSHIFT-Experimental` artifact, preserving the current `_RIPSHIFT.exe` launcher and verified experimental native DLL.
- Removed obsolete external Mod Manager/updater workflows, archived manager tracking metadata, and the retired GitHub watcher script.
- Renamed ancillary RIPSHIFT resource-mod and immersion-probe developer tools and updated native CI. Original KHARVOX runtime names remain unchanged where needed for compatibility.
- This is a packaging/source rename, not headset verification of Object Discovery grabbing.

## 23 November 2026 — Object Discovery (in development)

- New user-toggleable Object Discovery with separate persisted scan radius (1–15 m, default 5 m) and grab reach (1–5 m, default 2 m).
- Add coloured world-space candidate markers, native-motion-gated object gripping, and an in-game item calibration overlay opened directly after a successful first grab.
- Generate object profile names from uniquely validated model resource paths, with separate left/right hand offsets and rotations saved in UserData.
- Make profile colours update on save; keep unknown native model identities session-only rather than assigning unrelated props to the same profile.
- Release held props through DOOM native gravity without artificial downward throw; keep weapon-hand calibration independent.
- **Headset acceptance pending:** engine entity-pool discovery and stable model names cannot be established from compilation alone.

## 17 November 2026 — Experimental

### Launcher and installation
- RIPSHIFT launches from its own folder and uses the separately installed original KHARVOX runtime.
- Locate the original KHARVOX launcher on first setup; detect or browse for the DOOM game folder separately.
- Preserve existing UserData settings and weapon calibration during upgrades.
- Keep gameplay configuration in the in-headset RIPSHIFT Mod Menu rather than a separate external mod manager.

### Combat and VR controls
- Resolved Gunslinger regression: the equipped gun no longer appears inside the open weapon wheel (reported in experimental build 658; subsequently confirmed fixed in-headset).
- World Use follows the active weapon hand: R3 for right-handed Gunslinger ownership and L3 for left-handed ownership; the opposite short stick click remains Pause, while long R3 retains the Mod Menu.
- In normal Gunslinger configuration, the selected full-size translucent zone preview snaps to the matching Grip controller centre immediately on hold, follows live, stays on release, and saves only after Right Trigger. B cancels without saving.
- Gunslinger supports either weapon hand, behind-head weapon selection and configurable left/right hip quick holsters.
- Weapon Grip Calibration maintains independent left/right primary and support grips, with fine-tune, rotation and alternate-fire calibration for Gauss/Chaingun.
- PAINSAW adds physical trigger-based chainsaw operation; Physical Glory Kills and KILLTIME configure execution behaviour.
- Two-handed weapon stability, physical jumping, physical crouch and A-button manual crouch are available.
- Gameplay controller reconnection handling pauses safely on genuine loss of tracking and avoids unwanted controller-input events.

### HUD statistics and personal records
- Completed level / NEW PERSONAL BEST result notices automatically disappear after five seconds of real elapsed time; the notification is transient and does not affect saved personal bests. The XR quad is dismissed at the same deadline so the notice cannot remain stuck over later menus.
- Independent HUD displays for campaign/session deaths and time, level time, level deaths and level best, with configurable positioning.
- Overlapping enabled HUD readouts resolve by disabling the conflicting earlier display.
- Level recording supports explicit start/stop, completed personal records, time/deaths ranking and saved results.
- Unfinished recording is disarmed when leaving a level or exiting to desktop; current timer and record behaviour still requires full headset audit.

### Maintenance
- Remove the obsolete OpenXR chainsaw-zone outline overlay and its dedicated debug resource path. Chainsaw gameplay and haptics are not intentionally changed.
- Retain the original KHARVOX installation as an external requirement; RIPSHIFT packages only its own runtime and necessary dependencies.

## Verification
- Headset audit on 17 November: 31 original passes; medical-station interaction subsequently accepted, with collectible behaviour unchanged.
- Level timer, death counter, personal records and additional menu options remain subject to user testing.
