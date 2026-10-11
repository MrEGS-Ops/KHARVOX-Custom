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
