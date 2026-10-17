# KHARVOX Sync Action Speed — focused control and logging

## Sync Action Speed selector

One compact dropdown replaces five oversized buttons. It offers:

- **Native**: original DOOM sync-action timing
- **90 Hz, 70 Hz, 50 Hz, 30 Hz**: experimental engine timing presets,
  with lower Hz generally producing faster synchronized actions in current tests
- **Full speed · no slow-mo**: the **separate, proven native
  `sync_noAdaptiveTick` bypass**, used for BOTH Glory Kills and chainsaw
  executions. This is not equivalent to 30 Hz and must not be removed

The former independent *Fast Glory Kills* and *Fast Chainsaw Kills*
checkboxes have been removed from VR Mods. Existing saved preferences for
either switch migrate into **Full speed**, preserving Build #83 behaviour.

Changing the selector is direct: click and choose, or use keyboard arrow
keys. Restart DOOM after changing the speed selection.

## Advanced (3 measured action states)

Four scoped override controls exist: **Glory Kill, Chainsaw kill,
Ledge climbing, Scripted cinematics (experimental)**. The last is a new,
narrowly gated event from Foundry's observed `cutscene=1/syncAttack=0`,
not the deleted generic scripted-action probe.

- Tick a row to enable its reversible **30–240 Hz** whole-engine tick
  override for that action.
- Slider **right = faster action (30 Hz)**; **left = slower action
  (240 Hz)**. This is *not* a rendered frame-rate limit.
- Any checked action greys out the shared selector. The saved selector
  choice, including Full speed, returns when you uncheck the action or
  collapse Advanced. Between active overridden actions, Native timing
  is used instead of the stored shared preset.
- The old **Gauss Siege charge** and broad **Other scripted action** entries
  are retired. A *new* **Scripted cinematics** control identifies immersive
  non-execution cutscenes only: Foundry's arm-rip, access scanner, and console
  interactions are representative. This is based on native cutscene state, not
  an identified per-animation hook. The separate **Gauss Charge Slow Movement**
  VR mod remains unchanged.
- With Scripted cinematics **OFF** (default), immersive passive cutscenes
  preserve DOOM's native engine tick CVars. Unticking `Regular cinematics in
  CineWindow` changes presentation but must not force 120 Hz on these scenes.
- With the scripted override **ON**, KHARVOX temporarily applies the selected
  Hz target to those cutscenes and restores engine timing on exit. This is
  experimental global tick timing, not a guarantee that every Foundry
  animation changes wall-clock speed. Do not enable it for general gameplay.
- No unverified enemy/reload/boss/wheel/grenade animation controls remain.

**Log native actions + melee input** is the only diagnostic checkbox.
It prints action-state transitions (including explicit scripted cinematic) and observed melee input edges; the
melee edge is NOT a confirmed animation lifecycle. This checkbox does
not change gameplay speed.

## Suggested headset test

1. Start with **30 Hz**. Perform several Glory Kills and chainsaw kills.
2. Switch to **Full speed** (no slow-mo), restart DOOM, and repeat.
   Compare actual execution times and head-position comfort.
3. Choose the previous preset, expand Advanced, then enable only one of
   the 4 action overrides. Verify that the shared dropdown becomes
   disabled while the row is checked.
4. Choose 200 Hz for that action; capture its ENTER / TRANSITION / TEST /
   RESTORE events in `logs/KHARVOX.log`. Confirm the saved engine minimum
   tick rate returns after the action ends.
5. Collapse Advanced and verify the stored selector mode is available again.

Do not confuse 30 Hz and Full speed: the first changes engine timing
while Full speed bypasses native synchronization slow-motion. Keep
a known-good backup (Build #473) for headset rollback.

## Foundry-specific regression

1. Enable Immersive Mode + cinematic freelook, and uncheck
   `Regular cinematics in CineWindow`.
2. Leave **Scripted cinematics** OFF. Try the Foundry arm-rip, scanner,
   and computer button scenes. Expect
   `[CINEMATIC-HZ] scripted/passive immersive sequence: DOOM native tick timing PRESERVED`
   and **no** 120 Hz engine timing hold for these actions.
3. Enable Advanced → **Scripted cinematics** and choose e.g. **170 Hz**.
   Restart DOOM and retry. Expect
   `[ACTION-HZ] ENTER scripted-cinematic target=170 Hz`, an elapsed-duration
   test, and a corresponding restore of original engine settings.
4. Verify that Glory Kills and chainsaw executions do not get misclassified
   as scripted interactions. Verify both CineWindow and immersive rendering
   still behave as selected.
5. Disable the scripted override after testing to return to DOOM's normal
   timing for passive story interactions.
