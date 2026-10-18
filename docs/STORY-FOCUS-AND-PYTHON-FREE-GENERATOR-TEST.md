# KHARVOX story focus & Python-free mod generation — Test Build #540

## Argent Tower Destroyed: Pinky scene (LEFT of forward view)

- New VR Mods checkbox: **Story Camera Focus (Test)**, OFF by default.
- Existing story freelook discards the authored DOOM camera rotation and
  uses the physical headset over the body heading captured at entry.
- When the opt-in checkbox is ON in immersive cinematics, the native camera
  path retains DOOM's authored *story camera yaw* underneath the tracked
  HMD residual look. No hardcoded left/right rotation is applied.
- The special path explicitly excludes Glory Kills, chainsaw executions,
  ledge climbing and boss sequences; the existing camera basis is retained
  for these. It does not touch action-speed presets, sync tick Hz, hands,
  weapon mappings, positions, RS Free Walk or cinematic-exit tests.
- This can rotate the **virtual world**. Disable if it causes discomfort,
  unexpected tilt, eye mismatch or recenter snap. Do not force a physical
  head movement in response to VR camera rotation.
- Inspect the native log for `[STORY-FOCUS] native animated yaw ... ACTIVE`.
  Confirm that the Pinky moves from your LEFT toward DOOM's intended centre,
  and head turning remains tracked.
- Compare against OFF at the same save/checkpoint.

## Resource mod generation: No Python required

- The launcher now directly parses the installed unmodified DOOM 2016 v5
  `base/gameresources.index` and `gameresources.resources` in managed C#.
  No `py`, `python`, Windows Store alias or command-line interpreter runs.
- It only locates the game's original Aggressive Demons / Enhanced Gibs
  declarations and applies the existing numeric alterations.
- It validates archive header, count, unique record, source location, sizes,
  original patch flag, UTF-8 declaration structure, output content and ZIP.
  Unknown or patched resource layouts produce a safe explanation.
- It does NOT modify DOOM resource files. Output ZIPs go to the launcher
  packaged mods directory and remain unselected until you enable them.
- The launcher integration self-test constructs a temporary synthetic DOOM v5
  index and resource stream (raw deflate plus uncompressed entry), generates
  both ZIPs and verifies their contents. No proprietary assets are included.

After installing build #540, choose your DOOM folder, click Generate, and
report the result. If its original resource index layout is incompatible,
the new error should specify the failed validation rather than code 9009.

Status: build/CI and headset behavior must be confirmed before calling the
Pinky focus solved.
