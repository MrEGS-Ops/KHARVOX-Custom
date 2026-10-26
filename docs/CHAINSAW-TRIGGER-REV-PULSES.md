# KHARVOX X6 — Trigger Rev Pulses (one pull = one short burst)

This is a new mutually exclusive **Chainsaw A/B TEST** mode, ID 9.
Enable **Physical Chainsaw · Motion / Haptics / Zones**, expand
**Sync Action Speed → Advanced**, select
**X6 · Trigger Rev Pulses (Release + Pull)**, then restart DOOM.

## Behaviour

1. Native DOOM chainsaw execution starts normally. Hold Trigger,
   move the weapon hand OUTSIDE the 1.42m circular activation marker
   and bring it INSIDE once. The marker disappears; there is no
   post-activation controller motion/distance rule.
2. **The original trigger hold that activated the circle never grants
   a speed boost.** You MUST release to <=18% analog depth first.
3. Pull the trigger past >=33% depth: this creates exactly ONE burst.
   It begins immediately, and tracks the peak analog depth for ~80ms.
   A shallow pull produces a lighter rev, a deep squeeze approaches
   the fastest 30Hz engine request. A full 100% squeeze hits 30Hz.
4. The acceleration stays briefly elevated for **90–220ms**, depending
   on squeeze depth, then decays over **70ms** to 200Hz slow baseline.
   Holding the trigger down indefinitely does not prolong the burst.
5. To accelerate again, physically release to <=18% and pull past
   >=33% again. Button mashing yields a succession of short revs.
   Each new pull resets the pulse timer, **not** an accumulated boost.
6. On EACH new rev, request **maximum haptic amplitude 1.0** for
   90ms on the chainsaw hand. The rumble then drops to a quiet idle;
   keeping the trigger held will NOT renew maximum vibration.
   Before activation, the existing outside-ring haptic guide remains.
7. The prior global **second Trigger press = permanent fast-finish**
   shortcut is DISABLED *only in X6*. Otherwise, any attempt to
   mash Trigger would finish immediately after its second pull.
8. DOOM still owns the native execution and exit, damage/fuel and
   cinematic/head camera. There is no animation frame seeking, no
   action timing ownership during ledge climbs/landings, and no
   trigger remapping. 200/30Hz are scoped **engine action clock**
   requests, not Quest refresh/haptic rates.

## Verification / debugging

- `[CHAINSAW-REV] ARMED`: observed activation crossing.
- `[CHAINSAW-REV] RELEASE_READY`: analog went below 0.18; a new
   pull may now create a burst.
- `[CHAINSAW-REV] PULL`: new >=0.33 analog squeeze; increments
   unique revCount. A held trigger MUST NOT increment revCount.
- `[CHAINSAW-AB] SAMPLE`: pressure, revCount, ready flag,
   observed peak pressure, normalized strength, pulse age, requested
   native Hz, haptic amplitude, haptic frequency and renderFrame.
- `[CHAINSAW-AB] END`: confirmed native action exit. The separate
   read-only Chainsaw Animation Investigator remains optionally enabled.

Test: Activate ring → KEEP HOLDING for ~1 sec (no burst) →
release fully → squeeze gently → HOLD for 1 sec (return to slow) →
release fully → squeeze hard (stronger, faster short burst) →
rapidly alternate full release and pull ~4–6 times. Compare the
native action response and full-strength haptic response to pressure.

If DOOM exits the native execution when Trigger is released, logs
should reveal that: do not change DOOM's input hook until the cause
is validated, since this experiment deliberately avoids remapping.
