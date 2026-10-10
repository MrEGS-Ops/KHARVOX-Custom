# KHARVOX first-party DOOM mods

Status: **development / opt-in**. No game assets or third-party Nexus mod files are shipped. Passing unit tests is not proof of DOOM runtime compatibility.

## Development scope

| Feature | Approach | Current state |
| --- | --- | --- |
| Aggressive Demons | Independently patch original aiGlobalSettings numeric fields | Baseline generator and synthetic test |
| Enhanced Gibs | Independently patch original 8-gauge goreBehavior numeric fields | Baseline generator and synthetic test |
| Immersion Controls | Investigate original highlights, materials and FX; separate pickup glow and outlines | Research; no placeholder checkbox |
| Skip Intro | Existing launcher option +com_skipIntroVideo 1 | Already passed; needs actual startup-video verification |
| KHARVOX Visual Enhancements | VR-specific shaders across SFS/AER | Design pending stereo and performance profiling |
| Directional Dash | Native KHARVOX gameplay | Existing experimental track; not recreated here |

Generator source: tools/Generate-KHARVOX-Mods.py. It reads original resources from the user's own *unmodified* DOOM installation. It can also accept declarations exported by the user. It does not use third-party mod data and does not write into the game directory.

## Developer usage — first two mods

Requires Python 3.9+ and an installed DOOM (2016) with readable base/gameresources.index and base/gameresources.resources.

    python tools/Generate-KHARVOX-Mods.py --self-test
    python tools/Generate-KHARVOX-Mods.py --list

To generate experimental patches from the user's original game resources:

    python tools/Generate-KHARVOX-Mods.py --game-dir "C:\Program Files (x86)\Steam\steamapps\common\DOOM" --output-dir "<KHARVOX install>\mods\doom\kharvox"

If the original game uses unsupported compression or the baseline declaration cannot be found, generation stops safely. It does not invent a partial replacement. Output goes into the KHARVOX packaged-mod folder, never into DOOM itself.

Alternatively pass --source-dir with the user's own original-declaration export root (see --list for exact paths). Do not use modified Nexus declarations as input.

The generated mods are **disabled until explicitly selected**. They require a verified DOOMModLoader installation; existing KHARVOX preflight must reject collisions with other selected mods replacing the same resources. No game resources are silently overwritten.

Experimental default tuning (not yet gameplay-balanced):

- Aggressive Demons: --attackers 6 and --cooldown 0.5 seconds
- Enhanced Gibs: --min-limbs 2 --max-limbs 5 --gib-impulse 48

Only existing numeric fields are altered. The generator retains the rest of the user's original resource, rejects missing fields or unmatched AI entries and validates output ZIP integrity before replacing any previous generated ZIP.

## Before a release

1. Verify index format, declaration naming and compression against an unmodified Steam install. Current tests use synthetic game resources.
2. Generate and enable one ZIP at a time and confirm DOOMModLoader loads it correctly.
3. Test complete combat encounters and confirm values actually affect in-game AI and gib behaviour.
4. Verify mod overlap detection, rollback and preservation of user files.
5. Test SFS and AER frame-time/VR stability.
6. Add launcher settings and supported presets only after these stages pass.

## Immersion Controls

Do not repackage 82 third-party material/FX files. Extract original assets from the user's installed game and determine a small, independent whitelist of specific highlight toggles. Preserve essential interaction cues, document supported fields and add field-level tests.

## KHARVOX Visual Enhancements

Keep VR rendering changes in KHARVOX, not in a ReShade OpenGL injector. Prototype colour, contrast and light sharpening without screen-space depth dependence. Test both eyes in SFS/AER and budget GPU time. No unverified fisheye or monocular effects; disabled by default.

## Copyright and scope

Third-party archives may be examined as technical references but their declarations, compiled animation assets, maps, shaders and binaries must not be copied into this project. This implementation is independently generated from original game resources owned by the user.
