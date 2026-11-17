# RIPSHIFT VR — safe UserData layout

The five portable folders are:

- `UserData/Calibrations/`: per-hand, per-weapon, HUD and recoil calibration.
- `UserData/Settings/`: launcher, live VR mod/HUD preferences, original KHARVOX location.
- `UserData/Statistics/`: persistent campaign deaths and accumulated gameplay time.
- `UserData/Records/`: ranked level records and personal bests.
- `UserData/Profiles/`: known configuration marks.

## Migration and rollback contract

1. On start, RIPSHIFT copies only known personal files from the old flat
   `UserData/` layout to the matching category. Existing categorized files win.
   **No legacy file is deleted or overwritten.**
2. For old launcher settings, sources are considered in this order: existing
   categorized UserData, existing flat UserData, the former install folder,
   original KHARVOX AppData and, finally, shipped CalibrationDefaults.
3. Runtime reads prefer a categorized file when present; if it is absent they
   continue reading the old flat UserData file. Unknown editable configuration
   files remain at the original location.
4. Factory `*_default.cfg` files and hand model assets remain in `Config/`
   and the shipped `CalibrationDefaults/`. Shader binaries, install markers
   and launch-time environment/config files stay at their existing locations.
   The original KHARVOX folder is never modified by this migration.
5. `UserData/LegacyBackups/` retains pre-migration launcher conflict copies.
   Diagnostic logs, temporary files and native runtime output are excluded.
6. To roll back to a release that uses the flat layout, **copy** (do not move)
   the desired settings back from the new folders into the previous
   `UserData/` root while RIPSHIFT is closed. The original flat files
   are kept automatically until the user chooses to remove them.

## Gate before release

- Run `RIPSHIFT.exe --self-test-userdata` for copy-only migration,
  repeated launch, original-file preservation, fresh categorization and
  unmapped-file handling.
- Run the Windows native `runtime_paths_tests` target and existing
  launcher self-tests, then require a successful full native build.
- Verify fresh installation, upgrade with an existing populated `UserData/`,
  editor re-save, level record persistence/reset, weapon grips, Gunslinger,
  HUD and original-KHARVOX picker on an actual VR headset.
- No automatic promotion to the live experimental release on a failing gate.

This is a portable user-data layout change, not a DOOM saved-game migration.
