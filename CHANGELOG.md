# RIPSHIFT VR — Changelog

Changes to the RIPSHIFT VR add-on for DOOM (2016) using the original KHARVOX VR installation. This changelog describes the current experimental work; successful builds and code tests do not substitute for headset verification.

## 17 November 2026 — Experimental

### Launcher and installation
- RIPSHIFT launches from its own folder and uses the separately installed original KHARVOX runtime.
- Locate the original KHARVOX launcher on first setup; detect or browse for the DOOM game folder separately.
- Preserve existing UserData settings and weapon calibration during upgrades.
- Keep gameplay configuration in the in-headset RIPSHIFT Mod Menu rather than a separate external mod manager.

### Combat and VR controls
- Gunslinger supports either weapon hand, behind-head weapon selection and configurable left/right hip quick holsters.
- Weapon Grip Calibration maintains independent left/right primary and support grips, with fine-tune, rotation and alternate-fire calibration for Gauss/Chaingun.
- PAINSAW adds physical trigger-based chainsaw operation; Physical Glory Kills and KILLTIME configure execution behaviour.
- Two-handed weapon stability, physical jumping, physical crouch and A-button manual crouch are available.
- Gameplay controller reconnection handling pauses safely on genuine loss of tracking and avoids unwanted controller-input events.

### HUD statistics and personal records
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
