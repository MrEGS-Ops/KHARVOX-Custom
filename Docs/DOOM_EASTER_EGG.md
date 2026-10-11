# Hidden DOOM face easter egg (launcher-only)

Three left-clicks on the tiny Doomguy-style portrait beside Made by MrEGS within
four seconds trigger a silent 26-second, very low contrast moving outline in
the VR MODS and DOOM MODS panels. It fades in and out. Click again to stop it.
Hiding the Custom Mods sidecar immediately stops it.

## Genuine E1M1 footage (optional)

The launcher does NOT contain an E1M1 recording or automatically run the
original DOOM engine yet. It uses an original wireframe corridor as the
fallback. To use real E1M1 footage: record your own DOOM 1993 E1M1 video,
install FFmpeg, and run the prep script in this branch:

  powershell -File tools\Create-DoomEasterEgg.ps1 -Video "C:\clips\my-e1m1.mp4" -LauncherDirectory "C:\path\to\KharvoxLauncher"

Place assets\easteregg\e1m1_outline.gif next to KharvoxLauncher.exe.
Restart launcher. The conversion makes a 12 FPS / 512px wide, monochrome
outline-only animation without audio. No copyrighted footage is bundled.

## Authentic status-bar face (optional)

Put your own licensed DOOM1.WAD or DOOM.WAD in assets\easteregg\ next to the
launcher. The portrait loads only PLAYPAL, STFST00, STFTL00, STFTR00, and
STFEVL0 at runtime. No original DOOM game assets are distributed. Otherwise
the launcher uses an original pixel portrait placeholder.

## Current limits

The visual is drawn on the two inner mod tables and may be hidden by opaque
subcontrols. Original DOOM executable/demo playback and offscreen engine frame
capture are not implemented. This is a testable launcher UI prototype, not
a claim that the DOOM engine is integrated. No build was started.