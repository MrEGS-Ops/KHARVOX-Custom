# KHARVOX kill speed appearance slider — October 17 proposal

Status: **design/diagnostics only — NOT IMPLEMENTED IN RUNTIME**. The stable
Build #473 kill-speed and camera paths must remain untouched until there is a
measured, monotonic control for the intermediate levels.

User-confirmed #473 in Quest 3: native fast Glory Kills and chainsaw kills work;
the original Cactus 1.1.1 hand HUD is restored; the multi-second blackout is
gone; only the brief #456-style blink remains.

## Requested standalone slider

Proposed launcher label: **Cinematic Kill Speed**; scale 1–5:
- 1 — DOOM native slow cinematic.
- 2 — intermediate slow.
- 3 — medium.
- 4 — fast.
- 5 — confirmed Build #473 full-speed native bypass.

**Do not** assign fake intermediate speed values. In #473 the confirmed switch
is `sync_noAdaptiveTick=1` plus native pacing, a **binary** selection. Older
experimental fractional `timescale` did not demonstrate working intermediate
speeds and must not silently be sold as the same proven engine behavior.

Headset 120 Hz is display cadence, not DOOM's observed 37–39 Hz Present rate.
Forcing `com_adaptiveTickMinHz=120` previously stretched fixed-tick execution
animations. Do not change the display refresh frequency to simulate a speed
slider, or affect the weapon-wheel time dilation.

Before enabling levels 2–4:
1. Correlate **real** per-kill native sync/adaptive tick values, present cadence,
   animation wall-time and audio/haptic endpoints.
2. Identify one continuously variable engine-side parameter which controls
   perceptual kill speed while preserving native execution and camera safety.
3. Apply it only across real sync-attack/chainsaw execution lifetimes, with
   explicit restore on exit, abort, death, menu and reload.
4. Keep opt-in default OFF and preserve an exact #473 full-speed preset.
5. Verify progressive wall-clock timing and identical left/right-eye behavior
   at all five levels on Quest 3; reject any new blink, head-pose freeze or
   seconds-long black frame.

## Blink follow-up

Keep #456 stereo scene-domain rollback. Add diagnostic counters for
`xrEndFrame` projection layer submissions, skipped native stereo frames and
last-valid frame age at cinematic exit. Only try a short valid-frame hold if
the source confirms that a projection-layer gap actually causes the blink;
never present a stale frame without headset pose handling.
