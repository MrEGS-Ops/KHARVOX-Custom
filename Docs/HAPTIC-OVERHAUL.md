# KHARVOX — Haptic Overhaul (prototype)
Updated 2026-10-11

## Goal

One dedicated, expandable VR mod for player-impact sensations and later haptic customization. Keep DOOM's normal XInput vibration and weapon feedback intact.

## Approved damage-feedback design

- One constant-strength, distinguishable controller rumble, currently 70% at 115 Hz.
- Duration = 50 ms per combined health + armour point lost.
- Maximum 600 ms per continuous impact.
- Rapid damage extends an existing impact up to the same 600 ms cap, never queues long buzzing trains.
- 65 ms minimum quiet interval after a finished saturated pulse.
- Both controllers receive damage feedback; existing stronger native rumble takes priority.
- Pickups/increases, missing input, menus, loading and respawns must not trigger damage.
- Disabled by default, controlled by `KHARVOX_MOD_HAPTIC_OVERHAUL` and launcher custom-mod settings.

## Implementation status

- [x] Pure `DamageHapticsPolicy.h` (health/armour deltas, duration, caps, cooldown, respawn/invalid input resets).
- [x] Unit tests: `tests/damage_haptics_policy_tests.cpp` run by `tests/policy/CMakeLists.txt`.
- [x] XInput/OpenXR additive haptic mixing and tests. One controller-haptics writer remains authoritative.
- [x] Haptic Overhaul independent mod checkbox in new HAPTICS & FEEDBACK group; #01–#15 remain unchanged, mod at #16.
- [x] Persist setting and forward to native launch environment.
- [x] Verified-input mailbox boundary `KharvoxHapticSubmitPlayerVitals(health, armour, lifeGeneration, gameplay)`; it **does not** read memory by itself.
- [ ] **BLOCKER:** confirm live DOOM (2016) v6.66 Vulkan health and armour source and hook it to that boundary.
- [ ] Test in headset: damage from low-level Imp fireballs, Possessed Soldier bullets, environmental hazards, armour-only impacts, pickup immediately after hit, death/respawn, loading screens, and DOOM's stronger default vibrations.
- [ ] Confirm actual Quest 3 haptic amplitude/frequency behaviour and adjust feel.

## Reader validation plan

1. Examine DOOM (2016) player declarations and publicly documented health/armour trainer pointer paths as research references, not unverified runtime addresses.
2. Confirm two *live* values against on-screen HUD while taking small hits and picking up health/armour.
3. Capture the module/signature and player-life generation, and validate pointer lifetime through checkpoint load and level transition.
4. Use read-only access only; no writes, health cheats, or modifications to save files.
5. Publish validated vitals via `KharvoxHapticSubmitPlayerVitals`; fail silently/no pulse if validation or source freshness fails.
6. Use Supervisor for tracing and logs. Keep in-process work restricted to minimal gameplay value capture and VR controller output.

### Not yet functional as a damage-detection mod

Until step 5 is done, the checkbox can be selected and the haptic pipeline is ready, but **it deliberately emits no damage pulses**. This prevents random or incorrect rumble caused by guessing pointers. This status must not be reported as a complete gameplay feature.

## Future Haptic Overhaul subfeatures (ideas only)

- Separate profile presets for impact/weapon/UI/holster/gesture haptics.
- Optional directional damage once a reliable attacker-direction source exists.
- Tunable duration multiplier, cap, frequency and amplitude.
- bHaptics vest routing for confirmed damage types.
- Weapon drawing, holstering, grenade release and physical gesture feedback.
- Independent on/off for damage, weapon, UI and movement effects.
