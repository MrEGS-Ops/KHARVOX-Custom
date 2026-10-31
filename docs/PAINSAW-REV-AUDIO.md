# PAINSAW: Ripshift Overdrive — authentic DOOM 2016 rev sounds

The native PAINSAW XAudio2 engine looks for three **user-owned local WAV files**.
The public KHARVOX repo, build, updater and release do NOT contain game audio.

## Installation

Unzip the audio pack extracted from your own copy of DOOM 2016.
Copy the `audio` folder into the **same directory that contains
`KharvoxLayer.dll`** (the actual native game runtime, not the manager EXE):

- `audio/painsaw/rev_1.wav`
- `audio/painsaw/rev_2.wav`
- `audio/painsaw/rev_3.wav`

These map to the actual single-player Wwise event
`Play_wpn_sp_chainsaw_rev_surge`, present in `doom_weapon_sp.bnk`:

| PAINSAW cue | Retail media ID | Original sample | Duration |
|---|---:|---|---:|
| First pull | 206403322 | wpn_chainsaw_start_up_01.wav | 0.753s |
| Second pull | 165174355 | wpn_chainsaw_start_up_02.wav | 0.839s |
| Third pull | 504012594 | wpn_chainsaw_start_up_03.wav | 0.878s |

All three are WAVE PCM, signed 16-bit stereo at 44.1kHz.
On each confirmed native kill/re-pull event, PAINSAW plays the corresponding
original cue; holding Trigger does not repeat it, and kill exit stops it.
DOOM's original chainsaw and action sounds remain untouched.

The input files are loaded once on first PAINSAW rev. Restart DOOM after
changing/removing the files. If any original WAV is absent, invalid, or in
a different format, all three cues fall back to the previous synthesized
samples; the logging marker says `synthetic-fallback` rather than
`DOOM2016-rev-surge-original`. This fail-safe avoids interrupting executions.

Never commit DOOM audio into the KHARVOX GitHub repository, nor distribute
the extracted sound pack as part of a public release.
