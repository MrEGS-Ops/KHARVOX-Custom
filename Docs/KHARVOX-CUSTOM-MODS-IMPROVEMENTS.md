# KHARVOX — Custom Mods improvement queue
Updated: 2026-10-11

## NOW — implemented in source, pending build and hands-on verification

- [x] **Improved drag-and-drop:** hover-to-reveal grip, insertion position line, valid drop highlight, edge auto-scroll; prevent intergroup drops.
- [x] **Undo and reset:** right-click a mod row for Undo, Move Up, Move Down, Reset This Group and Reset All Groups. Only reorder presentation, never change checked state.
- [x] **Interactive dependency references:** show clickable `Requires #N` and `Disables #N` links below the mod name. Clicking navigates to/highlights the referenced mod. Rebuild links and tooltip references after a move.
- [x] **Development status:** compact status markers with tooltip explaining Verified, Experimental, Diagnostic only and Incomplete. Status is attached to mod identity rather than numbered position.

### Invariants

1. Positions `01`–`15` are fixed. Only mod content moves between slots within its category.
2. Categories stay fixed in this sequence: Hands & Arms, Legs & Movement, Weapons & Combat, HUD & Immersion, Demons & AI.
3. Requires/Disables references dynamically resolve the CURRENT position of the referenced mod.
4. Enabled states, mod IDs, gameplay hooks, saved mod flags and controller bindings never change from reordering.
5. Mod order is persisted separately in `kharvox-custom-mod-order.json`. Undo history is session-local.
6. The source changes are staged on `work/custom-mod-priority-order-no-build`. Do not merge/push to the auto-building branch until the user approves a build.

### Pending release checks

- [ ] Compile Windows .NET Framework 4.8 launcher and run Custom Mods UI self-test.
- [ ] Verify pointer drag gestures, insertion line visibility, invalid group drop, hover grips and edge scrolling on Windows.
- [ ] Verify link hit areas, target highlight/scroll and dynamic dependency numbers after each drag, undo and reset.
- [ ] Confirm disabled mods remain disabled and enabled mods stay enabled, even when moved.
- [ ] Check narrow docked layout, window focus on Close and user profiles/settings remain intact.
- [ ] Verify status classifications against actual runtime behavior before marking additional mods Verified.

## LATER — deliberately not implemented

- [ ] **Custom mod profiles:** save/load named combinations of enabled mods, independent of ordering; show settings diff before applying.
- [ ] **One-click diagnostics:** collect Supervisor logs, active settings and errors into a reviewable ZIP with privacy safeguards.
- [ ] **Windows scaling/layout:** dedicated tests and visual refinements at 100%, 125%, 150% and 200% display scaling, especially docked narrow layouts.
- [ ] **Context-sensitive VR haptics:** subtle feedback for holster zone, weapon draw and grenade throw gesture recognition.
- [ ] **Per-mod calibration controls:** individual thresholds/sensitivity for physical crouch, grenade throwing, chainsaw gestures and other physical interactions.
- [ ] **DOOM resource mod load-order manager (separate from VR mods):** add draggable numbered load positions in the DOOM MODS section, save user-defined ordering, and clearly distinguish display order from actual DOOMModLoader installation/override order. First verify upstream DOOMModLoader ordering and conflict-resolution behaviour; only then wire the order into KHARVOX staging/installation if supported or implementable safely. Show resource-overlap conflicts and which mod would take precedence before allowing any override. Never silently bypass current overlap checks or modify source archives. Include reset/undo and regression tests.

These six backlog ideas are **planning only**; no code implementation or CI job should be inferred.
