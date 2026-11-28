# RIPSHIFT UI cleanup (post #163)

Baseline: successful experimental #163, commit 3a80a6a9417d7fa99dd931ff510eebaaa1502c38, rollback/RIPSHIFT-experimental-163-last-known-good.

- Public Mods is radial; DEV remains its own list. Remove unreachable public-list input, rendering, row generation and old zone-browsing branches. Retain shared policy helpers and saved configuration readers until their callers and regression coverage are audited.
- Use radialInteractiveTarget for actionable hit targets and haptic detents. Stepper and reset centre readouts are silent.
- Hip entry radius 0.25 m, exit radius 0.32 m. Preserve original saved centre offsets, including the factory values. Visual rings show the actual entry boundary; hysteresis is intentionally larger. Wheel geometry unchanged.
- Gunslinger Tutorial displays the lower-to-hip hint on valid locked selection; native equip acknowledgement is still required before committing the wheel lesson. Cancellation or failed equip restores selection guidance.
- Haptics retain 10 ms hover, 20 ms button/save tick, 85 ms zone and two 85 ms physical confirmation pulses separated by 50 ms. Combat cues remain independent.
- Old gunslinger_helper_saved.cfg name intentionally retained for rollback/user compatibility. Internal gameplay visibility and log naming use Tutorial.
- Static radial coverage is already cached. Avoid unmeasured renderer rewrites or speculative configuration deletion.
- Headset validation: both hip spheres and saved centres, entry/exit hysteresis, both-handed wheel lock/release/cancel, DEV navigation, radial stepper/reset controls, HUD editor, calibration save/cancel, all four interface cues, existing UserData.
- Research suite is not changed here. Further layout/haptic consolidation and config audit should remain separate from the next research revision and must not remove working paths without proof.
