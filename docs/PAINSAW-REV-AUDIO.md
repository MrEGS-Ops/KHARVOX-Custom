# PAINSAW: Ripshift Overdrive — owner-extracted DOOM (2016) chainsaw revs

PAINSAW's XAudio2 system can play **three local WAV files** supplied by
the DOOM game owner. No game audio is tracked in the public KHARVOX repo,
bundled with releases, or downloaded automatically.

## Correct retail soundbank event mapping

The `doom_weapon_sp.bnk` archive and `soundbanksinfo.events` catalog confirm
that these TWO events reference the same three startup WAVs:

- `Play_wpn_sp_chainsaw_start_up` (event 861646195)
- `Play_wpn_sp_chainsaw_rev_surge` (event 3275147133)

Both refer to media IDs 206403322, 165174355 and 504012594, which are
`wpn_chainsaw_start_up_01.wav` through `_03.wav`. These were initially
chosen in error for PAINSAW because the rev-surge event reuses them; they
are not the high-RPM roar intended for the user's trigger-pull effect.

The corrected pack uses the **actual roaring engine recordings** selected
by `Play_wpn_sp_chainsaw_rev_up` (3346453514) and
`Play_wpn_sp_chainsaw_GLORY_chainsaw_rev_high_rpm` (2079975602):

| Original media ID | Actual sample |
|---|---|
| 899952141 | `weapons/chainsaw/3/sfx_chainsaw_roar_01.wav` |
| 1003138417 | `weapons/chainsaw/3/sfx_chainsaw_roar_02.wav` |

The recommended personal-use pack clips three natural rev bursts from
these two recordings (roar_01, roar_02, roar_01 for pull 1, 2 and 3).
The excerpts have short boundary fades and are resampled to the
PAINSAW loader's 44.1 kHz 16-bit stereo PCM format. No synthesized
audio is used in the *owner-supplied* cue files.

## Local installation

Extract the corrected audio pack and place its `audio` directory in the
same folder as the **active `KharvoxLayer.dll`**:

- `audio/painsaw/rev_1.wav`
- `audio/painsaw/rev_2.wav`
- `audio/painsaw/rev_3.wav`

Overwrite any files from the original startup pack. Restart DOOM to
reload samples. Trigger events and animation/haptics are unchanged.

The native loader checks for all three files; if any WAV is absent or in
an unsupported format, it falls back to the previous synthesized cues.
The runtime log's legacy `DOOM2016-rev-surge-original` marker currently
means only **local sample playback succeeded**, not which specific source
event supplied the WAV. `synthetic-fallback` identifies the fallback.

The standalone manager update preserves the three local WAVs when it
replaces the native-bridge directory. No changes to the game's files or
original upstream launcher are necessary.

**Copyright:** Extract from the owner's own installed DOOM game; do not
add DOOM audio to public source repositories, mod releases, or installers.
