# PAINSAW: Ripshift Overdrive — experimental rev sound v1

The normal DOOM engine owns its gameplay audio. PAINSAW now produces an
**additional synthesized two-stroke rev cue**, using Windows XAudio2 rather
than unverified DOOM sound-function hooks.

- First confirmed native chainsaw execution: one 490 ms rev.
- Valid second pull after analog release: one 540 ms rev.
- Third pull: one 900 ms sharper/higher-rev cue. Native 30 Hz finish is
  unchanged; the cue stops when the execution ends.
- Holding the trigger cannot repeat playback; only existing X6 rev edges
  retrigger. A new cue replaces the previous one, never stacks with it.
- Outside a confirmed X6 execution, no PAINSAW audio is emitted.
- Sound is generated once per DOOM process, with 48 kHz/16-bit PCM and
  sample peak limiting. Sound speed is never affected by the native tick Hz.
- Playback follows the **Windows default audio output**. Ensure Windows
  sends game audio to the VR headset if no sound is heard. Volume is limited
  by Windows/headset output settings; no change to system master volume.
- There are no downloaded audio samples or copyrighted game sound assets.
  This is a first synthesized test cue, not a reproduction of DOOM's
  original recorded chainsaw.
- Windows/driver may decline XAudio2 initialization. First failure emits
  `[CHAINSAW-REV-AUDIO]` to runtime log even without extended diagnostics.

The native KharvoxLayer DLL **must be rebuilt and delivered with the manager**.
Changing the bridge launcher alone does not add audio to existing native DLL.
