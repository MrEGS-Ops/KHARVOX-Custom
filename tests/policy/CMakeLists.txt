cmake_minimum_required(VERSION 3.20)
project(KharvoxGesturePolicyTests LANGUAGES CXX)
enable_testing()
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
