cmake_minimum_required(VERSION 3.20)
project(KharvoxGesturePolicyTests LANGUAGES CXX)
enable_testing()
add_executable(KharvoxGestureWheelExitPolicyTests ../gesture_wheel_exit_policy_tests.cpp)
target_compile_features(KharvoxGestureWheelExitPolicyTests PRIVATE cxx_std_17)
add_test(NAME gesture-wheel-exit-policy COMMAND KharvoxGestureWheelExitPolicyTests)
add_executable(KharvoxPhysicalGrenadeGestureTests ../physical_grenade_gesture_tests.cpp)
target_compile_features(KharvoxPhysicalGrenadeGestureTests PRIVATE cxx_std_17)
add_test(NAME physical-grenade-gesture COMMAND KharvoxPhysicalGrenadeGestureTests)

# Pure colour math only. Renderer/Vulkan integration is a separate milestone.
add_executable(KharvoxVisualTonePolicyTests ../visual_tone_policy_tests.cpp)
target_compile_features(KharvoxVisualTonePolicyTests PRIVATE cxx_std_17)
add_test(NAME visual-tone-policy COMMAND KharvoxVisualTonePolicyTests)

add_executable(KharvoxDamageHapticsPolicyTests ../damage_haptics_policy_tests.cpp)
target_compile_features(KharvoxDamageHapticsPolicyTests PRIVATE cxx_std_17)
add_test(NAME damage-haptics-policy COMMAND KharvoxDamageHapticsPolicyTests)

add_executable(KharvoxPersistentSsgReloadPolicyTests ../persistent_ssg_reload_policy_tests.cpp)
target_compile_features(KharvoxPersistentSsgReloadPolicyTests PRIVATE cxx_std_17)
if(MSVC)
  target_compile_options(KharvoxPersistentSsgReloadPolicyTests PRIVATE /UNDEBUG)
else()
  target_compile_options(KharvoxPersistentSsgReloadPolicyTests PRIVATE -UNDEBUG)
endif()
add_test(NAME persistent-ssg-reload-policy COMMAND KharvoxPersistentSsgReloadPolicyTests)

add_executable(KharvoxOpenHandReleasePolicyTests ../open_hand_release_policy_tests.cpp)
target_compile_features(KharvoxOpenHandReleasePolicyTests PRIVATE cxx_std_17)
add_test(NAME open-hand-release-policy COMMAND KharvoxOpenHandReleasePolicyTests)

add_executable(KharvoxDualWieldFirePolicyTests ../dual_wield_fire_policy_tests.cpp)
target_compile_features(KharvoxDualWieldFirePolicyTests PRIVATE cxx_std_17)
if(MSVC)
  target_compile_options(KharvoxDualWieldFirePolicyTests PRIVATE /UNDEBUG)
else()
  target_compile_options(KharvoxDualWieldFirePolicyTests PRIVATE -UNDEBUG)
endif()
add_test(NAME dual-wield-fire-policy COMMAND KharvoxDualWieldFirePolicyTests)

add_executable(KharvoxQuickHolsterPolicyTests ../quick_holster_policy_tests.cpp)
target_compile_features(KharvoxQuickHolsterPolicyTests PRIVATE cxx_std_17)
add_test(NAME quick-holster-policy COMMAND KharvoxQuickHolsterPolicyTests)

add_executable(KharvoxPhysicalPunchPolicyTests ../physical_punch_policy_tests.cpp)
target_compile_features(KharvoxPhysicalPunchPolicyTests PRIVATE cxx_std_17)
add_test(NAME physical-punch-policy COMMAND KharvoxPhysicalPunchPolicyTests)

add_executable(KharvoxWeaponWheelHudPolicyTests ../weapon_wheel_hud_policy_tests.cpp)
target_compile_features(KharvoxWeaponWheelHudPolicyTests PRIVATE cxx_std_17)
add_test(NAME weapon-wheel-hud-policy COMMAND KharvoxWeaponWheelHudPolicyTests)

# October 5 native full-speed Glory Kill regression: immersive cinematics
# must not overwrite +sync_noAdaptiveTick 1 with 0.
add_executable(KharvoxFastKillSyncTimingTests ../fast_kill_sync_timing_tests.cpp)
target_compile_features(KharvoxFastKillSyncTimingTests PRIVATE cxx_std_17)
if(MSVC)
  target_compile_options(KharvoxFastKillSyncTimingTests PRIVATE /UNDEBUG)
else()
  target_compile_options(KharvoxFastKillSyncTimingTests PRIVATE -UNDEBUG)
endif()
add_test(NAME fast-kill-sync-timing COMMAND KharvoxFastKillSyncTimingTests)

# Safety regression: gameplay camera must discard DOOM's cinematic pitch/roll
# as soon as the engine returns from GK/chainsaw, even if its observer remains.
add_executable(KharvoxCameraBasisPolicyTests ../camera_basis_policy_tests.cpp)
target_compile_features(KharvoxCameraBasisPolicyTests PRIVATE cxx_std_17)
if(MSVC)
  target_compile_options(KharvoxCameraBasisPolicyTests PRIVATE /UNDEBUG)
else()
  target_compile_options(KharvoxCameraBasisPolicyTests PRIVATE -UNDEBUG)
endif()
add_test(NAME camera-basis-return-to-gameplay COMMAND KharvoxCameraBasisPolicyTests)

# Hand HUD mirror is a presentation-only transform. Protect Cactus panel
# centre, size and native fallback against accidental pose regressions.
add_executable(KharvoxOffhandHudPolicyTests ../hud_layout_policy_tests.cpp)
target_compile_features(KharvoxOffhandHudPolicyTests PRIVATE cxx_std_17)
add_test(NAME offhand-hud-mirror-policy COMMAND KharvoxOffhandHudPolicyTests)

add_executable(KharvoxStoryCameraFocusPolicyTests ../story_camera_focus_policy_tests.cpp)
target_compile_features(KharvoxStoryCameraFocusPolicyTests PRIVATE cxx_std_17)
add_test(NAME story-camera-focus-action-tail COMMAND KharvoxStoryCameraFocusPolicyTests)

add_executable(KharvoxMajorStoryFocusPolicyTests ../major_story_focus_policy_tests.cpp)
target_compile_features(KharvoxMajorStoryFocusPolicyTests PRIVATE cxx_std_17)
add_test(NAME major-story-focus-selection COMMAND KharvoxMajorStoryFocusPolicyTests)

add_executable(KharvoxActionExitHandoffPolicyTests ../action_exit_handoff_policy_tests.cpp)
target_compile_features(KharvoxActionExitHandoffPolicyTests PRIVATE cxx_std_17)
add_test(NAME action-exit-handoff-duration COMMAND KharvoxActionExitHandoffPolicyTests)
