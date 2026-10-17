# RS Story Cinematic Skip — first test build (18 Oct 2026)

Status: **Experimental 4x native fast-forward request; NOT instant mission-state skipping.**
This is a separate opt-in VR MOD named **Story Cinematic Skip (RS)**.
The baseline full-speed Glory/Chainsaw modes and 30–240Hz action sliders are untouched.

## What RS does
- When an independent in-world story cutscene has stayed eligible for **400 ms**, press the *primary-hand* right-stick click (RS on standard right-hand layout).
- First click: request **4.0x native `timescale`** using the existing validated game CVar resolver.
- Second click: restore exact saved native CVar value immediately.
- Automatically restore on cinematic end, boss/ledge/kill transition, menu, unfocused XR session, action loss, or **20 seconds** after activation.
- The RS skip handler is deliberately excluded from normal gameplay, Glory Kills, chainsaw kills, ledge climbs, and boss cinematics.
- Left-handed users: activation follows the existing KHARVOX *Melee/Use thumbstick click* action binding; the physical controller side may differ.
- Does **not** jump mission scripting, mutate objective progression, warp the player, fake 3D camera frames, or disable animation callbacks.

## Important limitation
The native **`timescale` CVar sometimes fails unique resolution** on the current DOOM binary (confirmed in the 17 October headset logs). If so, the mod logs
`[STORY-SKIP] RS fast-forward unavailable` and does **not** touch game memory. This makes an opt-in test build safe but means the fast-forward effect may not occur until native timescale lookup is proven. A reliable instant skip requires separately validated per-sequence script-completion hooks.

## How to test
1. Back up the DOOM save before experimenting. Enable Story Cinematic Skip (RS) in **Custom Mods**, relaunch DOOM.
2. Confirm the native log contains `storySkipRS=on (experimental native 4x)`.
3. Trigger a long scanner, NPC dialogue, elevator, or other independent *in-world* story sequence. Let it play for at least 0.4 seconds, then click the primary-hand Melee/Use thumbstick once.
4. Observe one of `[STORY-SKIP] RS 4x fast-forward ON` or `[STORY-SKIP] RS fast-forward unavailable`.
5. Click again and verify `[STORY-SKIP] RS 4x fast-forward OFF`; check regular locomotion/weapon rate remains native.
6. Repeat during a Glory Kill, chainsaw execution and ledge climb. They **must not** activate the story skip.
7. Repeat and let a story scene finish naturally. Confirm native time is restored.
8. If a scene is unusually long, confirm automatic restoration after 20 seconds, and verify no sound/script desync or loss of tracking.

## Source implementation
- `src/openxr/StoryCinematicSkipPolicy.h`: pure input/classification policy and 20s timeout.
- `src/openxr/OpenXRBootstrap.cpp`: RS edge input, story classification, logging and cleanup.
- `src/camera/CameraHook.cpp`: native 4x timescale request and original-value restore.
- `tests/story_cinematic_skip_policy_tests.cpp`: automated CTest coverage.
- `launcher/KharvoxLauncher`: persisted checkbox and environment propagation.

Do not describe this as a universal instant skip or guaranteed 4x effect until a headset log demonstrates the resolved timescale and improved wall-clock sequence duration.
