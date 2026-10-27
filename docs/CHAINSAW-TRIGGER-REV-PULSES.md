# KHARVOX — Chainsaw Revving (X6)

The launcher exposes **one checkbox**, named **Chainsaw Revving**.
It enables native chainsaw experiment mode 9 automatically. The former
Sync Action Speed dropdown, chainsaw idle-Hz slider, ring preview and
other chainsaw experiments are no longer part of the launcher UI.

## Trigger control: THREE pulls total, maximum

1. **Pull ONE** starts DOOM's native chainsaw execution normally.
   Once the native execution is confirmed this very same trigger pull
   counts as rev **#1**. The effect starts on the confirmed action,
   without a circle or hand motion requirement.
2. Release below **18%** analog pressure, then squeeze above **33%**
   for **pull TWO**. This creates one further bounded rev speed pulse.
   Holding a trigger does not repeat, retrigger or hold the rev at maximum.
3. Release below 18% and pull again above 33% for **pull THREE**.
   This immediately latches the fastest previously tested engine timing,
   **30 tick Hz**, continuously **until DOOM finishes the native animation**.
   A fourth pull cannot increase the rev count, restart the sequence, or
   alter the timing. Native damage, fuel, playhead and exit remain DOOM-owned.

The first and second revs ease in for **35 ms**, hold a pressure-dependent
peak for **90–220 ms**, and ease out over **110 ms**. Slow baseline is
**200 engine tick Hz**. Lower tick Hz makes the observed kill run faster.
The third pull overrides the normal burst tail with the latched 30 Hz finish.

## Right controller haptics

During a confirmed X6 chainsaw execution, the **right-hand OpenXR Touch
controller** has a dedicated high-priority rumble signal. It is continually
refreshed with **100 ms pulses every 75 ms or when amplitude changes**,
maintaining around **89–92% amplitude between trigger pulls**. Each actual
rev is **100% amplitude** for its full rev window, with a 190 Hz accent.
On the third pull, the finishing tail remains at 100% until native exit.

The X6 right-hand signal intentionally takes priority over native XInput
rumble and ordinary damage/weapon vibration so that a strong native effect
cannot mask the rev accent. Left-hand haptics and unrelated mods retain
their normal mixer. Rumbling ends on native kill exit, loss of XR focus or
excluded menu contexts. The physical amplitude depends on the headset
and controller hardware; the OpenXR runtime accepting the signal alone
does not prove what is felt in the headset.

## Diagnostics

- `[CHAINSAW-REV] READY`: native chainsaw action confirmed; no circle.
- `[CHAINSAW-REV] RELEASE_READY`: analog release permits the next pull.
- `[CHAINSAW-REV] PULL`: fresh pull crossing 33%; third emits
  `THIRD_REV_LATCH_30HZ`.
- `[CHAINSAW-REV-HAPTIC] RIGHT OpenXR apply accepted`: haptic call
  succeeded at the runtime layer, **not** guaranteed headset sensation.
- `[CHAINSAW-AB] SAMPLE`: native clock request, rev count, haptic signal.
- `[CHAINSAW-AB] END`: confirmed native chainsaw exit.

The optional **Chainsaw Animation Investigator** is in **Diagnostics**.
Its probes are read-only. Do not alter native animation playheads, ledges,
story cinematics, or other action-speed overrides for this experiment.
