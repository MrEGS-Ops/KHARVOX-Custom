#pragma once

#include <windows.h>
#include <cstring>

namespace kharvox {

inline bool customModEnvironmentEnabled(const char* name) {
    char value[16]{};
    const DWORD copied = GetEnvironmentVariableA(name, value, sizeof(value));
    if (!copied || copied >= sizeof(value)) return false;
    return !_stricmp(value, "1") || !_stricmp(value, "true")
        || !_stricmp(value, "yes") || !_stricmp(value, "on");
}

struct CustomModFlags {
    bool disableHud{};
    bool disableWeaponWheel{};
    bool gaussChargeSlowMovement{};
    bool backOfHandHud{};
    bool cinematicExitHandoff{};
    bool boxingGuard{};
    bool fastGloryKills{};
    bool fastChainsawKills{};
    bool handFocusedRs{};
    bool directionalDash{};
    bool behindHeadWeaponWheel{};
    bool leftWristWeaponWheel{};
    bool behindHeadWheelHandSelection{true};
    bool physicalCrouch{};
    bool revengeDemon{};
    bool dynamicShoulderHolster{};
    bool persistentSuperShotgunReload{};
    bool physicalGrenadeThrow{};
    bool storyCinematicSkip{};
    bool storyFreeWalk{};
    bool storyCameraFocus{};
    bool motionGloryKillSpeed{};
    bool physicalChainsawGestures{};
    bool hapticOverhaul{};
};

inline CustomModFlags loadCustomModFlags() {
    CustomModFlags flags;
    flags.disableHud = customModEnvironmentEnabled("KHARVOX_MOD_DISABLE_HUD");
    flags.disableWeaponWheel = customModEnvironmentEnabled("KHARVOX_MOD_DISABLE_WEAPON_WHEEL");
    flags.gaussChargeSlowMovement = customModEnvironmentEnabled("KHARVOX_MOD_GAUSS_SLOW_CHARGE");
    // The separate mod only mirrors the already-restored Cactus hand HUD.
    flags.backOfHandHud = customModEnvironmentEnabled("KHARVOX_MOD_BACK_OF_HAND_HUD");
    // Opt-in diagnostic. OFF leaves DOOM/native cinematic entry and exit
    // handling unchanged; do not force early handoff as a global default.
    flags.cinematicExitHandoff = customModEnvironmentEnabled("KHARVOX_MOD_ACTION_EXIT_HANDOFF");
    flags.boxingGuard=customModEnvironmentEnabled("KHARVOX_MOD_BOXING_GUARD");
    flags.fastGloryKills=customModEnvironmentEnabled("KHARVOX_MOD_FAST_GLORY_KILLS");
    flags.fastChainsawKills=customModEnvironmentEnabled("KHARVOX_MOD_FAST_CHAINSAW_KILLS");
    flags.handFocusedRs = customModEnvironmentEnabled("KHARVOX_MOD_HAND_FOCUS_RS");
    flags.directionalDash = customModEnvironmentEnabled("KHARVOX_MOD_DIRECTIONAL_DASH");
    flags.behindHeadWeaponWheel = customModEnvironmentEnabled("KHARVOX_MOD_BEHIND_HEAD_WHEEL");
    flags.leftWristWeaponWheel = customModEnvironmentEnabled("KHARVOX_MOD_LEFT_WRIST_WHEEL");
    char behindHeadHandSelection[16]{};
    const DWORD behindHeadHandSelectionLength = GetEnvironmentVariableA(
        "KHARVOX_MOD_BEHIND_HEAD_WHEEL_HAND_SELECTION", behindHeadHandSelection,
        sizeof(behindHeadHandSelection));
    flags.behindHeadWheelHandSelection = behindHeadHandSelectionLength == 0
        || (!_stricmp(behindHeadHandSelection, "1")
            || !_stricmp(behindHeadHandSelection, "true")
            || !_stricmp(behindHeadHandSelection, "yes")
            || !_stricmp(behindHeadHandSelection, "on"));
    flags.physicalCrouch = customModEnvironmentEnabled("KHARVOX_MOD_PHYSICAL_CROUCH");
    flags.revengeDemon = customModEnvironmentEnabled("KHARVOX_MOD_REVENGE_DEMON");
    flags.dynamicShoulderHolster = customModEnvironmentEnabled("KHARVOX_MOD_DYNAMIC_SHOULDER");
    flags.persistentSuperShotgunReload = customModEnvironmentEnabled("KHARVOX_MOD_SSG_PERSISTENT_RELOAD");
    flags.physicalGrenadeThrow = customModEnvironmentEnabled("KHARVOX_MOD_PHYSICAL_GRENADE");
    flags.storyCinematicSkip = customModEnvironmentEnabled("KHARVOX_MOD_STORY_SKIP_RS");
    flags.storyFreeWalk = customModEnvironmentEnabled("KHARVOX_MOD_STORY_FREE_WALK_RS");
    flags.storyCameraFocus = customModEnvironmentEnabled("KHARVOX_MOD_STORY_CAMERA_FOCUS");
    flags.motionGloryKillSpeed = customModEnvironmentEnabled("KHARVOX_MOD_MOTION_GLORY_SPEED");
    flags.physicalChainsawGestures = false; // retired; fast/native toggle replaces gesture speed control
    flags.hapticOverhaul = customModEnvironmentEnabled("KHARVOX_MOD_HAPTIC_OVERHAUL");
    return flags;
}

} // namespace kharvox
