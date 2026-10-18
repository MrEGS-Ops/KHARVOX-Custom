# KHARVOX RS Story Free Walk — experimental build

This is an opt-in headset test. It is NOT an instant cinematic skip.

During a verified independent in-world scripted cinematic, click the primary-hand Melee/Use thumbstick (normally RS) to request native left-stick player-body movement. Click RS again to stop. A request also ends on cutscene exit, menu, loss of focus, Glory Kill, chainsaw, ledge, boss, or after 30 seconds. Normal scripts, camera, collisions, weapons and objectives are not deliberately modified.

## Safety
- Checkbox Story Free Walk (RS) · Experimental defaults OFF and is separate from the previous ineffective timescale skip. If both are checked, Free Walk takes priority.
- The native helper captures an unrestricted gameplay player-control baseline and verifies that the same player object remains active, using the existing reflected DOOM 6.66 idPlayer field signature.
- A stable six-frame observation of horizontal movement-inhibit bits 0x01 and/or 0x02 must occur before KHARVOX attempts a checked atomic clearing of ONLY those directional inhibit bits. These flag values are known from related idTech source but have not been independently confirmed in DOOM 2016. This test is therefore experimental.
- Unknown baselines, unknown owners, absent movement-only bits and unwritable memory all fail closed. There are no arbitrary offset guesses or teleports.
- Another script or physics lock may prevent walking even after a safe inhibit clear. The game may reapply the bits every frame.
- This code stops clearing when disabled. DOOM owns its normal movement flags afterwards.

## Test
1. Back up your save. Start with Action Exit Handoff OFF.
2. Enable Story Free Walk (RS) in VR Mods. Launch DOOM and play ordinary gameplay briefly to establish a movement-control baseline.
3. At an independent scripted story scene, wait a second, press RS, then try walking with the left stick. Press RS again to stop.
4. Look in the KHARVOX native log for [STORY-FREE-WALK] RS unlock requested and either horizontal inhibit release APPLIED, no horizontal inhibit bit observed, or fail closed.
5. Repeat at one Glory Kill, chainsaw execution and ledge: the free-walk request must NOT activate.
6. If it does move the body, verify the story script finishes, the next objective works, and camera and player pose return without snapping or stuck controls. If anything malfunctions, disable the option and reload a backed-up save.

## Known limitations
Native body locomotion may remain impossible if the scene locks physics/animation rather than input. A log reporting an applied inhibit clear is NOT proof that walking succeeded. This experiment does not attempt to speed up cinematics or move player coordinates directly.

Sources: src/camera/CameraHook.cpp, src/openxr/StoryFreeWalkPolicy.h, src/openxr/OpenXRBootstrap.cpp. Tests: tests/story_free_walk_policy_tests.cpp.
