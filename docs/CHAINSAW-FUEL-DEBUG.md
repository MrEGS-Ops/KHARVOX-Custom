# KHARVOX — Unlimited Chainsaw Fuel debug experiment

The launcher has a new opt-in **Unlimited Chainsaw Fuel (Debug · test)**
checkbox under VR Mods. It is OFF on all fresh configurations and survives
later manual ON/OFF selections.

**Implementation:** the next direct DOOM 2016 launch appends the game-native
command-line setting `+g_permaInfiniteAmmo 1`. The existing CVar is
documented in DOOM 2016 console-cheat lists, but the setting's effect on
**chainsaw fuel in this specific executable** has not been verified.
It may affect regular weapon ammunition, not only chainsaw fuel.

We intentionally do NOT enable `+devMode_enable 1` automatically, because
developer-mode/cheat activation can interfere with achievements or saved-game
behaviour. Depending on the game build, the command may be ignored entirely.
We never guess a fuel-decrement patch or overwrite the game's native inventory.
Uncheck the checkbox and restart DOOM to stop sending this launch setting.
If DOOM persisted its native cheat state, verify it has reset; do not assume
KHARVOX can restore state that the game itself serializes.

## Headset test

1. Back up the campaign save if you are concerned about achievements.
2. Tick **Unlimited Chainsaw Fuel (Debug · test)**, launch DOOM through
   KHARVOX with at least one normal chainsaw fuel charge already collected.
3. Record fuel before the first kill, perform three ordinary chainsaw kills
   on suitable enemies with no intervening fuel pickup, and compare the
   in-game fuel HUD after each.
4. Look for `[CHAINSAW-FUEL-DEBUG] KILL_ENTER/KILL_EXIT/AMMO_CHANGED`
   entries in KHARVOX logs. These read an already-signature-checked native
   inventory getter. `-1` means unavailable. The weapon-specific native
   ammo total has not been independently confirmed as the HUD fuel pip count.
5. If fuel still decreases, **the native infinite-ammo setting is not a
   verified chainsaw solution on this build**. Untick and restart. Do not
   increase permissions, force developer mode or guess binary offsets.
6. Negative control: untick and restart the game; check that chainsaw fuel
   consumption returns to ordinary DOOM behaviour.

## Existing evidence and limits

DOOM 2016 game-console references document `g_permaInfiniteAmmo` with
`cvarAdd g_permaInfiniteAmmo 1`, but sources disagree about whether all
cheat codes remain enabled in consumer releases:
- https://www.gamerevolution.com/guides/70366-doom-2016-console-command-cheat-codes-list
- https://gamefaqs.gamespot.com/pc/805860-doom/cheats
- https://steamcommunity.com/sharedfiles/filedetails/?id=686972243

There are external DOOM 2016 trainers advertising a separate, dedicated
"Unlimited Chainsaw" option, but their fuel interception point is not
documented or verified for this KHARVOX binary. We have not copied their
patch or installed an external cheat. If native g_permaInfiniteAmmo fails,
a fuel-only memory change would require discovering/validating DOOM's
specific fuel ownership and decrement path before implementation.

The debug checkbox must not alter chainsaw animation, XR haptics, weapon
input, glory kill timing, action Hz, enemy behaviour, fuel pickup assets or
VR render paths.
