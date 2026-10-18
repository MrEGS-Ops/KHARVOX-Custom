# Uninterrupted Story — automatic narrative hold A/B prototype

**Build intent:** Keep DOOM 2016 story/dialogue/script timing native, and try to restore ordinary walking while characters continue speaking. Target first test: Argent Facility (Destroyed), first terminal where Samuel Hayden talks.

## Implemented in this iteration
- New optional **Uninterrupted Story (Experimental)** checkbox in VR Mods, default OFF.
- Detects independent in-world scripted cinematic holds. Waits 900 ms so the triggering interaction can begin before trying to release player movement.
- No RS press required. While you push the movement stick, it tries the existing *validated, bounded* idPlayer horizontal movement-inhibit release. Other inhibit bits, camera pose, origin, weapons, audio, scripts, objectives and doors are left to the original engine.
- Excludes glory kills, chainsaw kills, ledges, bosses, menus, invalid tracking and other non-world contexts. Stops on exit or after 45 seconds, even if a cutscene flag stays stuck.
- Temporarily supersedes older RS Story Skip and RS Free Walk if those checkboxes were saved ON. They are not deleted or modified. When the new option is OFF, previous mods are unaffected.
- Emits `[UNINTERRUPTED-STORY]` log lines every ~1.5s while active: native cutscene flag, whether movement stick is pushed, validated idPlayer inhibit mask, movement bits, weapon-control inhibition, physics-origin freshness, player-origin displacement, last capture age, native narrow release result and level generation.
- The important diagnostic distinction: a moving stick with **no fresh physics origin** or **no horizontal inhibition** means script/animation or physics ownership must be investigated. Merely clearing input inhibition cannot force normal locomotion.
- The prior 30/40/50 Hz scoped action presets, cinematic camera focus, camera exit A/B, rendering and XR input mapping are unchanged.

## Limitations — not a universal skip yet
The current KHARVOX hooks do NOT expose DOOM's terminal script completion callback, narrative audio lifetime or door unlock triggers. We cannot safely bypass all waits or force a door open generically. This prototype **does not claim** to complete objectives or automatically advance scripts. Even if the native release is applied, walking may remain blocked by DOOM's animation/physics authority.

A real universal implementation will require verified high-level native script controls or an engine call that releases player control independently of dialogue; that has not been located. More robust support may require mission-specific patching for scripts where doors are intentionally held until dialogue concludes.

## Test instructions
1. Back up your save. Start at Argent Facility (Destroyed) before the first terminal.
2. Keep Action Exit Handoff OFF and Story Camera Focus OFF for the first comparison, to avoid conflating behaviours.
3. Turn **Uninterrupted Story (Experimental)** ON. You don't need RS; activate the terminal and let it start.
4. After about a second, try moving with the normal left stick. Does the player physically walk? Does Hayden keep speaking and does the objective eventually continue?
5. Exit normally and send the `KHARVOX-KHARVOX*.log` and native stereo log. Search for `[UNINTERRUPTED-STORY]` and `[STORY-FREE-WALK]`.
6. If you remain stuck, do **not** repeatedly toggle random CVars or force unknown native addresses. The logs now record whether control-inhibit flags or scene/physics holds are blocking progress.

All native writes fail closed on unknown idPlayer metadata, player identity, baseline, or nonmovement flag. Tests: `tests/uninterrupted_story_policy_tests.cpp`.
