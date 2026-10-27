# KHARVOX X6 — Circle-free Trigger Rev Pulses

Experimental chainsaw A/B mode ID 9.
Enable **Physical Chainsaw · Motion / Haptics / Zones**, expand
**Sync Action Speed → Advanced**, select **X6 · Trigger Rev Pulses**,
then restart DOOM. The launcher checkbox retains its historical label,
but X6 ignores zones and does not render an activation circle.

## Behaviour

1. DOOM begins a chainsaw execution normally. The moment the native
   execution is confirmed, X6 controls the action's slow baseline at
   **200 engine tick Hz**. There is NO circle, arm gesture, hand tracking
   or hand motion requirement.
2. The trigger that originally began the DOOM kill does not generate a
   second artificial rev. Release the trigger below 18% analog depth.
3. Every fresh squeeze above 33% produces exactly one rev. Depth controls
   strength; a full squeeze reaches **30 engine tick Hz**. A sustained
   hold does not create any more revs.
4. Each pulse eases in for **35ms**, holds its pressure-dependent peak
   for **90–220ms**, and smoothly eases out over **110ms**.
   A new deliberate pull starts a new burst, enabling trigger mashing.
5. The weapon-hand controller vibrates at approximately **96–100%
   amplitude throughout the entire confirmed chainsaw kill**.
   Small 75ms changes in intensity/frequency create a jaw/grinding texture.
   Each new rev briefly accents the vibration at 100% and 190Hz, but
   holding or releasing does not silence the sustained kill vibration.
6. With the chainsaw only equipped, normal soft idle vibration continues;
   continuous full-intensity grinding begins only when the native kill is
   confirmed. The haptics stop normally on native execution exit, focus
   loss or excluded pause/ledge contexts.
7. X6 does not alter grenade/trigger input mapping, fuel, collision,
   native animation playhead, ledge handling, cinematic camera or other
   experimental chainsaw modes. DOOM determines when the kill ends.

## Diagnostic signatures

- `[CHAINSAW-REV] READY immediate native kill`: immediately active,
  no zone or ring crossing required.
- `[CHAINSAW-REV] RELEASE_READY`: trigger is below 18%.
- `[CHAINSAW-REV] PULL`: fresh pull crossed 33% and one rev was counted.
- `[CHAINSAW-AB] SAMPLE`: requested Hz, rev count and haptic samples.
- `[CHAINSAW-AB] END`: native chainsaw execution completed.

Test a chainsaw kill by repeatedly releasing fully and pulling, both
slowly and rapidly, without moving the hand. Verify smoothing, sustained
near-maximum controller rumble, no circle, and that a held trigger does not
retrigger boosts. The animation investigation probes remain read-only;
changes to clock Hz still require in-headset validation.
