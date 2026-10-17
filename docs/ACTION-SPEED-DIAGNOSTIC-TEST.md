# KHARVOX Action-Speed Diagnostic Test (Build 512+)

This is an **experimental engine-tick timing** test. The slider targets do not
represent rendered FPS, and do not independently control enemy animations.
No action profiles are enabled automatically.

## Launcher setup

1. Under **Sync Action Speed**, tick **Advanced**.
2. Choose a 30 Hz general target (far right) if you want a common baseline.
3. Tick **Log Test: All detected actions** at the bottom of Advanced.
4. Optionally tick **one** validated action profile (Glory Kill, Chainsaw,
   Ledge climbing, Gauss Siege charge, Other scripted action) and choose a
   different target, e.g. 80 Hz. Higher values typically slow scripted
   animations in current KHARVOX testing. They do NOT guarantee higher FPS.
5. Restart DOOM to load the selections. Attempt that action several times
   and then repeat with its override disabled.
6. Upload the latest `logs/KHARVOX.log` and launcher log.

## Log markers

- `[ACTION-HZ] ENTER <action> target=... Hz`: selected, verified trigger.
- `[ACTION-HZ] TRANSITION <old> -> <new>`: two action states or fallback
  to baseline while the native cinematic hold is still active.
- `[ACTION-HZ][TEST] ... completed; presents=... elapsedMs=... measuredPresentHz=...`:
  event duration and observed presentation rate, not unique engine frames.
- `[ACTION-HZ] RESTORE ... engine minHz=...`: saved CVars restored after the
  scoped override ends. **Confirm this for each action**.
- `[ACTION-HZ][PROBE]`: read-only state edges, including melee input,
  grenade throw input, boss camera state, weapon wheel, native sync and ledge.
- `UNAVAILABLE`: native reload and per-enemy animation lifecycle hooks are
  **not verified**. Their checkboxes only enable diagnostic reporting; their
  grey sliders do not change any speed.
- `[CINEMATIC-HZ]`: the existing adaptive-tick CVar path and full-hold
  presentation counts.

## Safety boundaries and experiment design

- Fast Glory Kills and Fast Chainsaw Kills retain higher priority; the
  proven sync_noAdaptiveTick native bypass must not be overwritten.
- Action Hz affects the *whole engine tick while an action is detected*,
  not only that animation. Ledge, Gauss, and scripted profiles are
  **experimental**. Keep them OFF except for controlled tests.
- A profile is disabled automatically when its native state is no longer
  detected. Original CVar values are restored, including when VR stops.
- Never claim enemy-specific or weapon-reload-specific speed control based
  on a button press. Logs only identify what is actually observed.
- Compare the **same execution type** under baseline and selected Hz.
  Turn on one timing profile at a time before testing overlap/priority.
- If a kill causes a headset snap, a black frame, nausea, or engine instability,
  discontinue that profile and provide its logs. Keep backup Build #473.
